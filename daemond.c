#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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

const char execdir[] = "exec/";

volatile sig_atomic_t termflag;
sigset_t sync_sigmask;
const char *argv0;
Service *services;
time_t timeout;

void usage(void) {
	fprintf(stderr,
		"usage: %s [-t timeout] [next_program [arg...]]\n",
		argv0
	);
	exit(1);
}

pid_t spawn(const char *name) {
	char path[lenof(execdir) + strlen(name)];
	stpcpy(stpcpy(path, execdir), name);
	if (access(path, X_OK) < 0) return 0;
	pid_t pid = fork();
	if (pid == 0) {
		sigset_t sigmask;
		sigemptyset(&sigmask);
		sigprocmask(SIG_SETMASK, &sigmask, NULL);
		close(0);
		close(1);
		close(2);
		execv(path, (char *const []){(char *)name, NULL});
		//fprintf(stderr, "exec failed: %s[%i]\n", strerror(errno), errno);
		exit(127);
	}
	printf("spawned %s[%li]\n", name, (long)pid);
	return pid;
}

void scan(void) {
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
		pid_t pid = spawn(srvfile->d_name);
		if (!pid) continue;
		Service *srv = service(srvfile->d_name);
		service_setpid(srv, pid);
		service_insert(pos, srv);
		if (*pos) printf("added service %s\n", srv->name);
	}
	closedir(dir);
}

void reap(void) {
	int status;
	pid_t pid;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		Service **srv = service_from_pid(&services, pid);
		const char *name = *srv ? (*srv)->name : "";
		printf("%s[%li] ", name, (long)pid);
		if (WIFEXITED(status)) {
			printf("exited with code %i\n", (int)WEXITSTATUS(status));
		} else {
			int sig = WTERMSIG(status);
			printf("was terminated by signal %s[%i]\n", strsignal(sig), sig);
		}
		if (*srv) {
			pid = spawn(name);
			if (pid) {
				service_setpid(*srv, pid);
			} else {
				service_destroy(service_delete(srv));
				printf("removed service %s\n", name);
			}
		}
	}
}

void readkill(Service *srv) {
	int sig;
	while ((sig = service_readkill(srv))) {
		if (sig > 0) {
			kill(srv->pid, sig);
			printf("sent signal to %s[%li] as requested on fifo: %s[%i]\n",
				srv->name, (long)srv->pid, strsignal(sig), sig
			);
		} else {
			fprintf(stderr, "invalid signal requested for %s[%li] on fifo\n",
				srv->name, (long)srv->pid
			);
		}
	}
}

void loop(void) {
	scan();
	reap();

	int nfds = 0;
	fd_set readfds;
	FD_ZERO(&readfds);
	for (Service *srv = services; srv; srv = srv->next) {
		FD_SET(srv->killfd, &readfds);
		++nfds;
	}
	fflush(stdout);
	nfds = pselect(nfds, &readfds, NULL, NULL,
		timeout > 0 ? &(struct timespec){.tv_sec = timeout} : NULL,
		&sync_sigmask
	);

	for (Service *srv = services; srv && nfds > 0; srv = srv->next) {
		if (FD_ISSET(srv->killfd, &readfds)) {
			readkill(srv);
			--nfds;
		}
	}
}

void terminate(int sig) {
	termflag = 1;
}

void signop(int sig) {}

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
