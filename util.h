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

/* populates buf from the end
 * returns pointer to start of string inside buf
 * returns NULL if base is invalid or buf is too small
 *
 * an n-byte integer will always fit in n*ceil(8*log(2)/log(base))+1 chars,
 * including null-terminator (e.g. sizeof(val)*3+1 for base=10)
 *
 * an optimizer should be able to inline this and possibly use a shorter type
 * than intmax_t. imaxdiv is avoided because it can prevent this optimization,
 * and internally it usually just does division and modulo seperately anyway.
 */
static inline char *int_to_str(uintmax_t val, char *buf, size_t buflen, int base) {
	if (buflen <= 0 || base < 2 || base > 36) return NULL;
	buf += buflen;
	*--buf = '\0';
	while (val > 0) {
		if (--buflen <= 0) return NULL;
		int digit = val % base;
		*--buf = digit > 10 ? digit - 10 + 'a' : digit + '0';
		val /= base;
	}
	return buf;
}
