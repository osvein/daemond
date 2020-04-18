#include <fcntl.h>
#include <stdbool.h>
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

const char pidfile[] = "/pid";
const char killfile[] = "/kill";

Service *service(const char *name) {
	size_t namelen = strlen(name);
	Service *self = malloc(sizeof(*self) + namelen + 1);
	self->next = NULL;
	self->pid = 0;
	mkdir(name, 0777);
	char path[namelen + sizeof(killfile)];
	stpcpy(stpcpy(path, name), killfile);
	mkfifo(path, 0777);
	self->killfd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	fcntl(self->killfd, F_SETFL, fcntl(self->killfd, F_GETFL) | O_NONBLOCK);
	self->killbuf[0] = '\0';
	stpcpy(self->name, name);
	return self;
}

void service_destroy(Service *self) {
	while (self) {
		Service *next = self->next;
		char path[strlen(self->name) + max(sizeof(pidfile), sizeof(killfile))];
		char *base = stpcpy(path, self->name);
		stpcpy(base, pidfile);
		unlink(path);
		stpcpy(base, killfile);
		unlink(path);
		rmdir(path);
		if (self->killfd >= 0) close(self->killfd);
		free(self);
		self = next;
	}
}

void service_setpid(Service *self, pid_t pid) {
	int fd;
	{
		char path[strlen(self->name) + sizeof(pidfile)];
		stpcpy(stpcpy(path, self->name), pidfile);
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
	}
	char buf[3 * sizeof(pid) + 1];
	char *str = int_to_str(pid, buf, sizeof(buf), 10);
	endof(buf)[-1] = '\n';
	write(fd, str, endof(buf) - str);
	close(fd);
	self->pid = pid;
}

/* return >0 - valid signal
 * return =0 - no signal available
 * return <0 - invalid signal
 */
int service_readkill(Service *self) {
	char *sep, *pos = self->killbuf;
	ssize_t n = strnlen(pos, lenof(self->killbuf));
	bool overflow = n >= lenof(self->killbuf);
	while (!(sep = memchr(pos, '\n', n))) {
		pos += n;
		if (pos >= endof(self->killbuf)) {
			pos = self->killbuf;
			overflow = true;
		} else if (!overflow) {
			*pos = '\0';
		}
		n = read(self->killfd, pos, endof(self->killbuf) - pos);
		if (n < 0) return 0;
		char *c = pos + n;
		while (c-- > pos) if (!*c) *c = '?'; // neutralize NUL chars
	}
	*sep = '\0';
	int sig = overflow ? -1 : getsignal(self->killbuf);
	memmove(self->killbuf, sep, n - (sep - self->killbuf));
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
