#include <errno.h>
#include <locale.h>
#include <string.h>

#define lenof(array) (sizeof(array) / sizeof(*array))
#define endof(array) (array + lenof(array))
#define member(type, name) ((type *)0)->name // can be passed to sizeof etc.

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CAP(i, min, max) MIN(MAX(i, min), max)

/* LOGGING HELPERS
 * argv0 must be defined, preferably set to argv[0] in main
 * format string must be a literal and must not contain numbered %n$ specifiers
 * also keep these conventions in mind:
 * - warnings/errors include an exclamation mark at the very end of the line!
 *   (LOG_ERRNO already does this, DIE does not)
 * - make it easy to grep the code for messages, don't split literals
 * - don't rely on the log being synchronized with stdin and stdout
 *   if a message is related to data in another stream, refer to it explicitly
 *   e.g. by line number
 * - include global identifiers for localized messages
 *   e.g. errno for strerror like LOG_ERRNO does
 */
#define LOG(...) LOG_INTERNAL_(__VA_ARGS__, 0)
#define LOG_ERRNO(...) LOG_ERRNO_INTERNAL_(__VA_ARGS__, err(), errno)
#define DIE(...) (LOG(__VA_ARGS__), exit(1))
#define DIE_ERRNO(...) (LOG_ERRNO(__VA_ARGS__), exit(1))

/* workaround for empty VA_ARGS */
#define LOG_INTERNAL_(f, ...) dprintf(2, "%s: " f "\n", argv0, __VA_ARGS__)
#define LOG_ERRNO_INTERNAL_(f, ...) LOG_INTERNAL_(f ": %s[%i]!", __VA_ARGS__)

extern const char *argv0;

/* thread-safe strerror(errno) */
static inline const char *err(void) {
	locale_t l = newlocale(LC_ALL_MASK, "", 0);
	if (!l) return "locale error";
	return strerror_l(errno, l);
}
