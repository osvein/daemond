#include <dirent.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "service.h"
#include "util.h"

volatile sig_atomic_t termflag;
sigset_t sync_sigmask;
const char *argv0;
Service *services;
time_t timeout;

static void usage(void) {
	dprintf(2, "usage: %s [-t timeout] [next_program [arg...]]\n", argv0);
	exit(1);
}

static void scan(void) {
#if !SKIP_MTIME
	{
		static struct timespec scantime;
		struct stat st;
		stat(execdir, &st);
		if (st.st_mtim.tv_sec == scantime.tv_sec &&
			st.st_mtim.tv_nsec == scantime.tv_nsec
		) return;
		scantime = st.st_mtim;
	}
#endif

	// i don't like dirent
	DIR *dir = opendir(execdir);
	if (!dir) return;
	struct dirent *srvfile;
	while ((srvfile = readdir(dir))) {
		if (*srvfile->d_name == '.') continue;
		Service **pos = service_from_name(&services, srvfile->d_name);
		if (*pos) continue;
		Service *srv = service(srvfile->d_name);
		if (srv) {
			service_spawn(srv);
			if (srv->pid > 0) {
				service_insert(pos, srv);
				dprintf(1, "%s service added\n", srv->name);
				continue;
			}
			service_destroy(srv);
		}
		dprintf(2, "%s service was not added\n", srvfile->d_name);
	}
	closedir(dir);
}

static void reap(void) {
	int status;
	pid_t pid;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		Service **srv = service_from_pid(&services, pid);
		const char *name = *srv ? (*srv)->name : "";
		if (WIFEXITED(status)) {
			dprintf(1, "%s[%li] exited with code %i\n",
				name, (long)pid, (int)WEXITSTATUS(status)
			);
		} else {
			int sig = WTERMSIG(status);
			dprintf(1, "%s[%li] terminated by signal %s[%i]\n",
				name, (long)pid, strsignal(sig), sig
			);
		}
		if (*srv) {
			service_spawn(*srv);
			if ((*srv)->pid < 0) {
				service_destroy(service_delete(srv));
				dprintf(1, "%s service removed\n", name);
			}
		}
	}
}

static void loop(void) {
	scan();
	reap();

	int nfds = 0;
	fd_set readfds;
	FD_ZERO(&readfds);
	for (Service *srv = services; srv; srv = srv->next) {
		if (srv->killfd < 0) continue;
		if (srv->killfd >= nfds) nfds = srv->killfd + 1;
		FD_SET(srv->killfd, &readfds);
	}
	nfds = pselect(nfds, &readfds, NULL, NULL,
		timeout > 0 ? &(struct timespec){.tv_sec = timeout} : NULL,
		&sync_sigmask
	);

	for (Service *srv = services; srv && nfds > 0; srv = srv->next) {
		if (FD_ISSET(srv->killfd, &readfds)) {
			service_handlekill(srv);
			--nfds;
		}
	}
}

static void terminate(int sig) {
	termflag = 1;
}

static void signop(int sig) {}

int main(int argc, char **argv) {
	argv0 = *argv;

	int c;
	while ((c = getopt(argc, argv, "t:")) >= 0) {
		switch (c) {
		case 't':
			{
				char *endptr;
				long i = strtol(optarg, &endptr, 10);
				if (!*optarg || *endptr || i < 0) usage();
				timeout = i;
				break;
			}
		default:
			usage();
		}
	}

	sigset_t sigmask;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGCHLD);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGTERM);
	sigprocmask(SIG_BLOCK, &sigmask, &sync_sigmask);

	struct sigaction act = {.sa_flags = 0};
	sigemptyset(&act.sa_mask);
	act.sa_handler = terminate;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_handler = signop;
	act.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &act, NULL);

	while (!termflag) loop();
	if (optind < argc) execvp(argv[optind], argv + optind);
	return 0;
}
