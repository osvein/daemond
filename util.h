#include <stddef.h>
#include <stdint.h>

#define lenof(array) (sizeof(array) / sizeof(*array))
#define endof(array) (array + lenof(array))
#define member(type, name) ((type *)0)->name // can be passed to sizeof etc.

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

#define syslog(fmt, ...) ( \
	dprintf(2, "%s: ", argv0), \
	dprintf(2, fmt, __VA_ARGS__), \
	putc('\n', stderr) \
)
#define sysfatal(fmt, ...) (syslog(fmt, __VA_ARGS__), exit(1))
