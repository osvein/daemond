#include <errno.h>
#include <locale.h>
#include <stdint.h>
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
 * - use exclamation marks at the very end of the line to indicate severity!!
 * - make it easy to grep the code for messages, don't split literals
 * - don't rely on the log being synchronized with stdin and stdout
 *   if a message is related to data in another stream, refer to it explicitly
 *   e.g. by line number
 */
#define LOG(...) LOG_INTERNAL_(__VA_ARGS__, "")
#define DIE(...) (LOG(__VA_ARGS__), exit(1))

/* workaround for empty VA_ARGS */
#define LOG_INTERNAL_(f, ...) dprintf(2, "%s: " f "%s\n", argv0, __VA_ARGS__)

extern const char *argv0;

/* thread-safe strerror(errno) */
static inline const char *err(void) {
	locale_t l = newlocale(LC_ALL_MASK, "", 0);
	if (!l) return "locale error";
	return strerror_l(errno, l);
}

/* locale independent. no sign character. 2 <= base <= 36
 * stops at the first character that is not a digit for base, or is a digit that
 * would cause the result to be larger than max. a pointer to that character is
 * stored in *str
 */
static inline uintmax_t parseuint(char **str, uintmax_t max, int base) {
	uintmax_t i = 0;
	while (1) {
		char c = **str;
		if (c >= '0' && c <= '9') c -= '0';
		else if (c >= 'a' && c <= 'z') c -= 'a' - 10;
		else if (c >= 'A' && c <= 'Z') c -= 'A' - 10;
		else break;
		if (c >= base || (max - c) / base < i) break;
		i = i * base + c;
		++*str;
	}
	return i;
}
