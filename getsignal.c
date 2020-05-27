/* getsignal - associates signal names and signal numbers */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>

#include "getsignal.h"
#include "util.h"

const Getsignal getsignals[] = {
	{SIGABRT, "ABRT"},
	{SIGALRM, "ALRM"},
	{SIGBUS, "BUS"},
	{SIGCHLD, "CHLD"},
#ifdef SIGCLD
	{SIGCLD, "CLD"},
#endif
	{SIGCONT, "CONT"},
#ifdef SIGEMT
	{SIGEMT, "EMT"},
#endif
	{SIGFPE, "FPE"},
	{SIGHUP, "HUP"},
	{SIGILL, "ILL"},
#ifdef SIGINFO
	{SIGINFO, "INFO"},
#endif
	{SIGINT, "INT"},
#ifdef SIGIO
	{SIGIO, "IO"},
#endif
#ifdef SIGIOT
	{SIGIOT, "IOT"},
#endif
	{SIGKILL, "KILL"},
#ifdef SIGLOST
	{SIGLOST, "LOST"},
#endif
	{SIGPIPE, "PIPE"},
#ifdef SIGPOLL
	{SIGPOLL, "POLL"},
#endif
#ifdef SIGPROF
	{SIGPROF, "PROF"},
#endif
#ifdef SIGPWR
	{SIGPWR, "PWR"},
#endif
	{SIGQUIT, "QUIT"},
	{SIGSEGV, "SEGV"},
#ifdef SIGSTKFLT
	{SIGSTKFLT, "STKFLT"},
#endif
	{SIGSTOP, "STOP"},
#ifdef SIGSYS
	{SIGSYS, "SYS"},
#endif
	{SIGTERM, "TERM"},
	{SIGTRAP, "TRAP"},
	{SIGTSTP, "TSTP"},
	{SIGTTIN, "TTIN"},
	{SIGTTOU, "TTOU"},
#ifdef SIGUNUSED
	{SIGUNUSED, "UNUSED"},
#endif
	{SIGURG, "URG"},
	{SIGUSR1, "USR1"},
	{SIGUSR2, "USR2"},
#ifdef SIGVTALRM
	{SIGVTALRM, "VTALRM"},
#endif
#ifdef SIGWINCH
	{SIGWINCH, "WINCH"},
#endif
#ifdef SIGXCPU
	{SIGXCPU, "XCPU"},
#endif
#ifdef SIGXFSZ
	{SIGXFSZ, "XFSZ"},
#endif
	{0}
};

int getsignal(const char *name) {
	char *p = (char *)name;
	int num = parseuint(&p, INT_MAX, 10);
	if (*p) {
		return num;
	} else if (p != name) {
		return 0;
	}
	for (const Getsignal *s = getsignals; s->num; ++s) {
		if (strncasecmp(s->name, name, SIGNAMELEN) == 0) return s->num;
	}
	return 0;
}
