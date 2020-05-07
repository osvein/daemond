/* service - handles service resources and control interfaces */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "getsignal.h"
#include "service.h"
#include "util.h"

#define SERVICE_LOG(self, ...) SERVICE_LOG_INTERNAL_((self), __VA_ARGS__, 0)
#define SERVICE_LOG_ERRNO(self, ...) ( \
	SERVICE_LOG_ERRNO_INTERNAL_((self), __VA_ARGS__, strerror(errno), errno) \
)
#define SERVICE_LOG_INTERNAL_(s, f, ...) (s->pid > 0 ? \
	LOG_INTERNAL_("%s[%li]: " f, s->name, (long)s->pid, __VA_ARGS__) : \
	LOG_INTERNAL_("%s: " f, s->name, __VA_ARGS__) \
)
#define SERVICE_LOG_ERRNO_INTERNAL_(s, f, ...) (self->pid > 0 ? \
	LOG_ERRNO_INTERNAL_("%s[%li]: " f, s->name, (long)s->pid, __VA_ARGS__) : \
	LOG_ERRNO_INTERNAL_("%s: " f, s->name, __VA_ARGS__) \
)

const char execdir[] = "exec/";
const char killpipe[] = "kill";
const char pidfile[] = "pid";
const char substfile[] = "subst";

Service *service(const char *name) {
	Service *self = malloc(sizeof(*self) + strlen(name) + 1);
	if (!self) {
		LOG_ERRNO("%s: malloc failed", name);
		return NULL;
	}
	self->next = NULL;
	self->pid = 0;
	self->killbuf[0] = '\0';
	stpcpy(self->name, name);
	mkdir(name, 0777);

	char path[snprintf(NULL, 0, "%s/%s", name, killpipe) + 1];
	snprintf(path, sizeof(path), "%s/%s", name, killpipe);
	mkfifo(path, 0777);
	self->killfd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (self->killfd >= 0) {
		int flags = fcntl(self->killfd, F_GETFL);
		if (flags < 0 ||
			fcntl(self->killfd, F_SETFL, flags | O_NONBLOCK) < 0 ||
			(self->killfdr = open(path, O_WRONLY | O_CLOEXEC)) < 0
		) {
			close(self->killfd);
			self->killfd = -1;
		}
	}
	if (self->killfd < 0) SERVICE_LOG_ERRNO(self, "failed to open killpipe");
	return self;
}

void service_destroy(Service *self) {
	while (self) {
		Service *next = self->next;
		if (self->killfd >= 0) {
			close(self->killfd);
			close(self->killfdr);
		}
		char path[strlen(self->name) + 1
			+ MAX(sizeof(pidfile), sizeof(killpipe))
		];
		char *base = stpcpy(path, self->name);
		*base++ = '/';
		stpcpy(base, pidfile);
		unlink(path);
		stpcpy(base, killpipe);
		unlink(path);
		*base = '\0';
		rmdir(path);
		free(self);
		self = next;
	}
}

static void service_writepid(Service *self) {
	int fd;
	{
		char path[snprintf(NULL, 0, "%s/%s", self->name, pidfile) + 1];
		snprintf(path, sizeof(path), "%s/%s", self->name, pidfile);
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	}
	if (fd < 0 || dprintf(fd, "%li\n", (long)self->pid) < 0) {
		SERVICE_LOG_ERRNO(self, "failed to write pidfile");
	}
	close(fd);
}

void service_spawn(Service *self) {
	self->pid = -1;
	char path[MAX(
		snprintf(NULL, 0, "../%s/%s", self->name, substfile),
		snprintf(NULL, 0, "../%s%s", execdir, self->name)
	)];
	snprintf(path, sizeof(path), "../%s/%s", self->name, substfile);
	if (access(path + 1, X_OK) < 0) {
		snprintf(path, sizeof(path), "../%s%s", execdir, self->name);
		if (access(path + 1, X_OK) < 0) return;
	}
	self->pid = fork();
	if (self->pid == 0) {
		sigset_t sigmask;
		errno = 0;
		sigemptyset(&sigmask);
		sigprocmask(SIG_SETMASK, &sigmask, NULL);
		chdir(self->name);
		setsid();
		close(0);
		close(1);
		close(2);
		if (errno) exit(125);
		execv(path, (char *const []){(char *)self->name, NULL});
		exit(127);
	} else if (self->pid > 0) {
		SERVICE_LOG(self, "forked");
		service_writepid(self);
	} else {
		SERVICE_LOG_ERRNO(self, "fork failed");
	}
}

/* return >0 - valid signal
 * return =0 - invalid signal
 * return <0 - no signal available
 */
static int service_readkill(Service *self) {
	char *delim, *pos = self->killbuf;
	ssize_t n = strnlen(pos, lenof(self->killbuf));
	bool overflow = false;
	while (!(delim = memchr(pos, '\n', n))) {
		pos += n;
		if (pos >= endof(self->killbuf)) {
			pos = self->killbuf;
			overflow = true;
		} else if (!overflow) {
			*pos = '\0';
		}
		n = read(self->killfd, pos, endof(self->killbuf) - pos);
		if (n <= 0) return -1;
		char *c = pos + n;
		while (c-- > pos) if (!*c) *c = '?'; // neutralize NUL chars
	}
	*delim++ = '\0';
	int sig = overflow ? 0 : getsignal(self->killbuf);
	n += pos - delim;
	memmove(self->killbuf, delim, n);
	self->killbuf[n] = '\0';
	return sig;
}

void service_handlekill(Service *self) {
	int sig;
	while ((sig = service_readkill(self)) >= 0) {
		if (sig > 0) {
			const char *s = strsignal(sig);
			if (kill(self->pid, sig) >= 0) {
				SERVICE_LOG(self, "sent signal %s[%i]", s, sig);
			} else {
				SERVICE_LOG_ERRNO(self, "failed to send signal %s[%i]", s, sig);
			}
		} else {
			SERVICE_LOG(self, "invalid signal!");
		}
	}
}

Service **service_from_name(Service **list, const char *name) {
	while (*list) {
		if (strcmp((*list)->name, name) == 0) break;
		list = &(*list)->next;
	}
	return list;
}

Service **service_from_pid(Service **list, pid_t pid) {
	while (*list) {
		if ((*list)->pid == pid) break;
		list = &(*list)->next;
	}
	return list;
}

void service_insert(Service **pos, Service *element) {
	if (element) element->next = *pos;
	*pos = element;
}

Service *service_delete(Service **pos) {
	Service *element = *pos;
	if (element) {
		*pos = element->next;
		element->next = NULL;
	}
	return element;
}
