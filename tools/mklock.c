#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "../parsechmod.h"
#include "../util.h"

const char *argv0;

void usage(void) {
	dprintf(2, "usage: %s [-m mode] file...", argv0);
	exit(1);
}

int main(int argc, char **argv) {
	const char *mstr = "+";
	mode_t mode = 0666;
	int c, errcount = 0;

	argv0 = *argv;
	while ((c = getopt(argc, argv, "m:")) >= 0) {
		if (c == 'm') {
			mstr = optarg;
		} else {
			usage();
		}
	}
	argv += optind;

	if (!*argv || parsechmod(mstr, &mode, umask(0)) < 0) usage();
	do {
		int fd = open(*argv, O_WRONLY|O_CREAT|O_EXCL, mode);
		if (fd >= 0) {
			close(fd);
		} else {
			LOG("failed to open %s: %s!", *argv, err());
			++errcount;
		}
	} while (*++argv);

	return errcount;
}
