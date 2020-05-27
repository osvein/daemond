#include <string.h>

#include <sys/stat.h>

#include "parsechmod.h"
#include "util.h"

#ifdef S_ISVTX
#define ISVTX_CHAR "t"
#else
#define S_ISVTX
#define ISVTX_CHAR
#endif

static int _index(const char *s, int c) {
	char *p = strchr(s, c);
	return p && *p ? p - s : -1;
}

int parsechmod(const char *str, mode_t *modep, mode_t mask) {
	mode_t mode = *modep;
	const char *p = str;
	if (!*p) return -1;
	mode = parseuint((char **)&p, 07777, 8);
	if (p == str) while (1) {
		static const mode_t whomasks[] = {04700, 02070, 07, 06777}; // ugoa
		static const mode_t permmasks[] = {0444, 0222, 0111, 06000, S_ISVTX}; // rwxst
		mode_t who = 0, clear = 0;
		int idx;
		while ((idx = _index("ugoa", *p)) >= 0) {
			who |= whomasks[idx];
			++p;
		}
		if (who) {
			clear = ~who;
		} else {
			who = ~mask;
		}
		if (!*p) return -1;
		do {
			char op = *p++;
			mode_t perm = 0;
			idx = _index("ugo", *p);
			if (idx >= 0) {
				mode_t permcopy = mode & whomasks[idx];
				for (const mode_t *m = permmasks; m < endof(permmasks); ++m) {
					if (permcopy & *m) perm |= *m;
				}
				++p;
			} else while ((idx = _index("rwxs" ISVTX_CHAR "X", *p)) >= 0) {
				if (*p++ == 'X') {
					idx = 2; // x
					if (!(mode & permmasks[idx] || S_ISDIR(mode))) continue;
				}
				perm |= permmasks[idx];
			}
			perm &= who;
			switch (op) {
			case '=':
				mode &= clear;
			case '+':
				mode |= perm;
				break;
			case '-':
				mode &= ~perm;
				break;
			default:
				return -1;
			}
		} while (*p && *p != ',');
		if (!*p) break;
		++p;
	}
	if (!*p) return -1;
	*modep = mode;
	return 0;
}
