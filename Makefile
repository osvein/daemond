.POSIX :

CFLAGS += -D _POSIX_C_SOURCE=200809L -D SIGNAMELEN=7

DAEMOND = daemond.c getsignal.o service.o
CLEAN += daemond
daemond : $(DAEMOND) getsignal.h service.h util.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(DAEMOND)

TOOLS = tools/mklock tools/waitsocket
CLEAN += $(TOOLS)
.PHONY : tools
tools : $(TOOLS)
TOOLS_MKLOCK = tools/mklock.c parsechmod.o
tools/mklock : $(TOOLS_MKLOCK) parsechmod.h util.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TOOLS_MKLOCK)

tools/waitsocket : tools/waitsocket.c util.h

TOOLS_LINUX = tools/linux/kreboot tools/linux/linkd
CLEAN += $(TOOLS_LINUX)
.PHONY : tools/linux
tools/linux : $(TOOLS_LINUX)
tools/linux/kreboot : tools/linux/kreboot.c
tools/linux/linkd : tools/linux/linkd.c util.h

CLEAN += getsignal.o parsechmod.o service.o
getsignal.o : getsignal.c getsignal.h util.h
parsechmod.o : parsechmod.c parsechmod.h util.h
service.o : service.c service.h getsignal.h util.h

clean:
	rm -f $(CLEAN)
