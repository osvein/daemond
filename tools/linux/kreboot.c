#include <stdio.h>
#include <unistd.h>

#include <sys/reboot.h>

int main(int argc, char **argv) {
	int c, cmd = RB_AUTOBOOT;
	while ((c = getopt(argc, argv, "hrp")) >= 0) {
		switch (c) {
		case 'h':
			cmd = RB_HALT_SYSTEM;
			break;
		case 'r':
			cmd = RB_AUTOBOOT;
			break;
		case 'p':
			cmd = RB_POWER_OFF;
			break;
		default:
			dprintf(2, "usage: %s [-h|-r|-p]", *argv);
		}
	}
	if (reboot(cmd) < 0) {
		perror(*argv);
		return 1;
	}
	return 0;
}
