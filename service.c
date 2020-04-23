#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "getsignal.h"
#include "service.h"
#include "util.h"

#if USE_VFORK
#define fork vfork
#endif

const char pidfile[] = "pid";
const char killpipe[] = "kill";

Service *service(const char *name) {
	size_t namelen = strlen(name);
	Service *self = malloc(sizeof(*self) + namelen + 1);
	self->next = NULL;
	self->pid = 0;
	self->killbuf[0] = '\0';
	stpcpy(self->name, name);
	mkdir(name, 0777);

	char path[namelen + 1 + lenof(killpipe)];
	sprintf(path, "%s/%s", name, killpipe);
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
	return self;
}

void service_destroy(Service *self) {
	while (self) {
		Service *next = self->next;
		char path[strlen(self->name) + 1
			+ max(sizeof(pidfile), sizeof(killpipe))
		];
		char *base = stpcpy(path, self->name);
		*base++ = '/';
		stpcpy(base, pidfile);
		unlink(path);
		stpcpy(base, killpipe);
		unlink(path);
		*base = '\0';
		rmdir(path);
		if (self->killfd >= 0) {
			close(self->killfd);
			close(self->killfdr);
		}
		free(self);
		self = next;
	}
}

void service_setpid(Service *self, pid_t pid) {
	int fd;
	{
		char path[strlen(self->name) + 1 + lenof(pidfile)];
		sprintf(path, "%s/%s", self->name, pidfile);
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	}
	dprintf(fd, "%li\n", (long)pid);
	close(fd);
	self->pid = pid;
}

/* return >0 - valid signal
 * return =0 - invalid signal
 * return <0 - no signal available
 */
int service_readkill(Service *self) {
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
