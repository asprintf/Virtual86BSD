# crontab - schedule periodic background work (with the daemon crond) (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# crond, the daemon that runs the entries, is installed in $(SBINDIR).

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin
SBINDIR  = $(PREFIX)/sbin

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = crontab
OBJS     = crontab.o cron.o
CROND_OBJS = crond.o cron.o

all: $(PROG) crond

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

crond: $(CROND_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(CROND_OBJS) $(LDLIBS)

crontab.o crond.o cron.o: cron.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	cd ../../regress/usr.bin/crontab && $(MAKE) test

difftest:
	@echo "crontab: no differential tests"

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	mkdir -p $(DESTDIR)$(SBINDIR)
	cp crond $(DESTDIR)$(SBINDIR)/crond

clean:
	rm -f $(PROG) $(OBJS) crond crond.o

.PHONY: all test difftest install clean
