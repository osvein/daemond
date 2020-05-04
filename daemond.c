/* daemond - process supervisor that can run as PID 1 (init) */

#include <dirent.h>
#include <errno.h>
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

#ifndef ismodified
#define ismodified(newtime, prevtime) ( \
	newtime.tv_sec != prevtime.tv_sec || newtime.tv_nsec != prevtime.tv_nsec \
)
#endif

const char *argv0;
char **next_program;
time_t timeout;
sigset_t sigmask_sync;

volatile sig_atomic_t termflag;
Service *services;

static void usage(void) {
	dprintf(2, "usage: %s [-t timeout] [next_program [arg...]]\n", argv0);
	exit(1);
}

static void scan(void) {
	{
		static struct timespec scantime;
		struct stat st;
		if (stat(execdir, &st) < 0) {
			LOG_ERRNO("failed to stat execdir");
		} else if (ismodified(st.st_mtim, scantime)) {
			scantime = st.st_mtim;
		} else {
			return;
		}
	}

	// i don't like dirent
	DIR *dir = opendir(execdir);
	if (!dir) LOG_ERRNO("failed to open execdir");
	struct dirent *srvfile;
	while ((srvfile = readdir(dir))) {
		if (*srvfile->d_name == '.') continue;
		Service **pos = service_from_name(&services, srvfile->d_name);
		if (*pos) continue;
		Service *srv = service(srvfile->d_name);
		if (!srv) continue;
		service_spawn(srv);
		if (srv->pid > 0) {
			service_insert(pos, srv);
			LOG("%s service added", srv->name);	service_destroy(srv);
			continue;
		}
		service_destroy(srv);
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
			LOG("%s[%li] exited with code %i",
				name, (long)pid, (int)WEXITSTATUS(status)
			);
		} else {
			int sig = WTERMSIG(status);
			LOG("%s[%li] terminated by signal %s[%i]",
				name, (long)pid, strsignal(sig), sig
			);
		}
		if (*srv) {
			service_spawn(*srv);
			if ((*srv)->pid < 0) {
				service_destroy(service_delete(srv));
				LOG("%s service removed", name);
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
		&sigmask_sync
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

static void signop(int sig) {/* interrupts pselect */}

static void exec_next(void) {
	execvp(*next_program, next_program);
	LOG_ERRNO("failed to exec next_program");
}

int main(int argc, char **argv) {
	int c;

	argv0 = *argv;
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
	next_program = argv + optind;
	if (*next_program) atexit(exec_next);

	struct sigaction sa = {0};
	errno = 0;
	if (sigemptyset(&sa.sa_mask) < 0) DIE_ERRNO("failed to init signal mask");
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGINT);
	sigaddset(&sa.sa_mask, SIGTERM);
	if (errno || sigprocmask(SIG_BLOCK, &sa.sa_mask, &sigmask_sync) < 0) {
		DIE_ERRNO("failed to set signal mask");
	}
	sa.sa_handler = terminate;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sa.sa_handler = signop;
	sa.sa_flags = SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa, NULL);
	if (errno) DIE_ERRNO("failed to set signal handlers");

	while (!termflag) loop();
}
