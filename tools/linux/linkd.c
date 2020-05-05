/* linkd - runs command on network interface changes */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <sys/socket.h>
#include <sys/wait.h>

#include "../../util.h"

const char *argv0;
char **command;
unsigned delay = 1;
bool ignore_exit;
int nl;
volatile sig_atomic_t delaystate;

static void usage(void) {
	dprintf(2, "usage: %s [-d seconds_delay] [-i] command [arg...]\n", argv0);
	exit(1);
}

static bool is_trigger_msg(struct nlmsghdr *msg) {
	return !msg->nlmsg_pid;
}

static void loop(void) {
	if (delaystate == 2 || !delay) {
		int status;
		pid_t pid = fork();
		if (pid == 0) {
			execvp(*command, command);
			LOG_ERRNO("failed to exec command");
			exit(127);
		} else if (pid < 0) {
			DIE_ERRNO("fork failed");
		}
		delaystate = 0;
		if (waitpid(pid, &status, 0) < 0) DIE_ERRNO("wait failed");
		if (!ignore_exit && WIFEXITED(status) && WEXITSTATUS(status)) exit(0);
	} else if (delaystate == 0) {
		delaystate = 1;
		alarm(delay);
	}

	struct nlmsghdr hdr;
	do {
		ssize_t s = recv(nl, &hdr, sizeof(hdr), 0);
		if (s == 0) {
			DIE("netlink closed unexpectedly!");
		} else if (s < 0 && errno != EINTR && errno != ENOBUFS) {
			DIE_ERRNO("failed to read netlink");
		}
	} while (!is_trigger_msg(&hdr));
}

static void handle_alarm(int sig) {
	delaystate = 2;
}

int main(int argc, char **argv) {
	int c;
	argv0 = *argv;
	while ((c = getopt(argc, argv, "d:i")) >= 0) {
		switch (c) {
		case 'd':
			{
				char *endptr;
				unsigned long l = strtoul(optarg, &endptr, 10);
				if (!*optarg || *optarg == '-' || *endptr) usage();
				delay = min(l, UINT_MAX);
			}
			break;
		case 'i':
			ignore_exit = true;
			break;
		default:
			usage();
		}
	}
	command = argv + optind;
	if (!*command) usage();

	struct sigaction sa = {.sa_handler = handle_alarm};
	if (sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGALRM, &sa, NULL) < 0) {
		DIE_ERRNO("failed to install signal handlers");
	}

	struct sockaddr_nl addr = {
		.nl_family = AF_NETLINK,
		.nl_groups = RTMGRP_LINK
	};
	nl = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (nl < 0 || bind(nl, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DIE_ERRNO("failed to open rtnetlink");
	}
	while (1) loop();
}
