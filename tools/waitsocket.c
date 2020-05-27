/* waitsocket - delay execution of program until its sockets are written to */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../util.h"

#ifndef ENV_LISTEN_FDS
#define ENV_LISTEN_FDS "LISTEN_FDS"
#endif
#ifndef ENV_LISTEN_PID
#define ENV_LISTEN_PID "LISTEN_PID"
#endif

const char *argv0;

static void usage(void) {
	dprintf(2,
		"usage: %s [-d dgram_addr] [-q seqpacket_addr] [-r raw_addr] "
		"[-s stream_addr] program [arg...]\n",
		argv0
	);
	exit(1);
}

static int parseaddr_unix(char *str, struct sockaddr *addr) {
	struct sockaddr_un *sun = (struct sockaddr_un *)addr;
	if (strncpy(sun->sun_path, str, sizeof(sun->sun_path))
		>= endof(sun->sun_path)
	) return -1;
	unlink(str);
	return 0;
}

static int parseaddr_inet(char *str, struct sockaddr *addr) {
	void *paddr;
	in_port_t *pport;
	char *saddr = str, *sport = NULL;
	if (addr->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *)addr;
		paddr = &sin->sin_addr.s_addr;
		pport = &sin->sin_port;
		sport = strrchr(str, ':');
		if (sport) *sport++ = '\0';
	} else {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
		paddr = &sin6->sin6_addr.s6_addr;
		pport = &sin6->sin6_port;
		if (*str == '[') {
			char *closing = strrchr(str, ']');
			++saddr;
			if (!closing) return -1;
			*closing = '\0';
			if (closing[1] == ':') {
				sport = closing + 2;
			} else if (closing[1]) {
				return -1;
			}
		}
	}
	if (sport) {
		char *p = sport;
		*pport = parseuint(&p, 0xFFFF, 10);
		if (*p) return -1;
	} else {
		*pport = 0;
	}
	if (inet_pton(addr->sa_family, saddr, paddr) <= 0) return -1;
	return 0;
}

static int parseaddr(char *str, struct sockaddr *addr) {
	static const struct {
		const char *s;
		int i;
		int (*f)(char *str, struct sockaddr *addr);
	} domains[] = {
		{"unix", AF_UNIX, parseaddr_unix},
		{"inet", AF_INET, parseaddr_inet},
		{"inet6", AF_INET6, parseaddr_inet}
	};
	char *split = strchr(str, ':');
	if (!split) return -1;
	*split = '\0';
	for (unsigned i = 0; i < lenof(domains); ++i) {
		if (strcasecmp(str, domains[i].s) == 0) {
			addr->sa_family = domains[i].i;
			return domains[i].f(split + 1, addr);
		}
	}
	return -1;
}

int main(int argc, char **argv) {
	fd_set fds;
	int nfds = 0;
	int c;
	FD_ZERO(&fds);
	argv0 = *argv;
	while ((c = getopt(argc, argv, "d:q:r:s:")) >= 0) {
		int fd;
		int type;
		struct sockaddr_storage addr;
		switch (c) {
		case 'd':
			type = SOCK_DGRAM;
			break;
		case 'q':
			type = SOCK_SEQPACKET;
			break;
		case 'r':
			type = SOCK_RAW;
			break;
		case 's':
			type = SOCK_STREAM;
			break;
		default:
			usage();
		}
		if (parseaddr(optarg, (struct sockaddr *)&addr) < 0) {
			DIE("invalid address!");
		}
		fd = socket(addr.ss_family, type, 0);
		if (fd < 0) DIE("failed to open socket: %s!", err());
		if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
			DIE("failed to bind socket: %s!", err());
		}
		if (listen(fd, 0) < 0 && errno != EOPNOTSUPP) {
			DIE("failed to listen on socket: %s!", err());
		}
		FD_SET(fd, &fds);
		if (fd >= nfds) nfds = fd + 1;
	}
	if (!argv[optind]) usage();
	select(nfds, &fds, NULL, NULL, NULL);
	{
		char buf[snprintf(NULL, 0, "%i", nfds - 3) + 1];
		snprintf(buf, sizeof(buf), "%i", nfds - 3);
		setenv(ENV_LISTEN_FDS, buf, 1);
	}
	{
		pid_t pid = getpid();
		char buf[snprintf(NULL, 0, "%li", (long)pid) + 1];
		snprintf(buf, sizeof(buf), "%li", (long)pid);
		setenv(ENV_LISTEN_PID, buf, 1);
	}
	execvp(argv[optind], argv + optind);
	DIE("failed to exec: %s", err());
}
