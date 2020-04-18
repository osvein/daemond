typedef struct Getsignal Getsignal;

struct Getsignal {
	int num;
	char name[SIGNAMELEN];
};

extern const Getsignal getsignals[];

int getsignal(const char *name);
