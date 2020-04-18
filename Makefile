.POSIX :

CFLAGS += -D _POSIX_C_SOURCE=200809L -D SIGNAMELEN=7

ALL = daemond
CLEAN = $(ALL)
all : $(ALL)

DAEMOND = daemond.o service.o getsignal.o
daemond : $(DAEMOND)
	$(CC) $(LDFLAGS) -o $@ $(DAEMOND)

CLEAN += daemond.o getsignal.o service.o
deamond.o : daemond.c service.h util.h
getsignal.o : getsignal.c getsignal.h util.h
service.o : service.c service.h getsignal.h util.h

clean:
	rm -f $(CLEAN)
