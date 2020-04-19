#include <sys/types.h>

typedef struct Service Service;
struct Service {
	Service *next;
	pid_t pid;
	int killfd;
	int killfdr;
	char killbuf[SIGNAMELEN];
	char name[];
};

Service *service(const char *name);
void service_destroy(Service *self);
void service_setpid(Service *self, pid_t pid);
int service_readkill(Service *self);

Service **service_from_name(Service **pos, const char *name);
Service **service_from_pid(Service **pos, pid_t pid);
void service_insert(Service **pos, Service *element);
Service *service_delete(Service **pos);
