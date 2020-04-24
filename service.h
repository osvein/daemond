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

extern const char execdir[];

/* service directory */
extern const char killpipe[];
extern const char pidfile[];
extern const char substfile[];

Service *service(const char *name);
void service_destroy(Service *self);
void service_spawn(Service *self);
void service_handlekill(Service *self);

/* list functions */
Service **service_from_name(Service **pos, const char *name);
Service **service_from_pid(Service **pos, pid_t pid);
void service_insert(Service **pos, Service *element);
Service *service_delete(Service **pos);
