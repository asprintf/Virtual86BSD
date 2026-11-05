# compress - compress and expand data (with uncompress and zcat) (POSIX.1-2024)
#
# Targets: all (the default), test, difftest, install, clean.
#
# uncompress and zcat are links to compress (it acts by its name).

.POSIX:

CFLAGS   = -O2
LDFLAGS  =
PREFIX   = /usr/local
BINDIR   = $(PREFIX)/bin

# STDFLAGS: more options for the compiler, none needed (the sources are
# C11 with POSIX interfaces); with gcc or clang, for example,
# STDFLAGS="-std=c11 -Wall -Wextra".  CC is make's own (POSIX: c17).
STDFLAGS =
LDLIBS   =

PROG     = compress
OBJS     = compress.o lzw.o gzip.o

all: $(PROG) uncompress zcat

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

uncompress: $(PROG)
	rm -f uncompress
	ln -s $(PROG) uncompress

zcat: $(PROG)
	rm -f zcat
	ln -s $(PROG) zcat

$(OBJS): compress.h

.c.o:
	$(CC) $(STDFLAGS) $(CFLAGS) -c $<

test: all
	sh ./test_compress.sh

# compares with other implementations; needs
#   gzip
difftest: all
	sh ../testlib/tl.sh tests/compressdiff.tl ./compress

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(PROG) $(DESTDIR)$(BINDIR)/$(PROG)
	rm -f $(DESTDIR)$(BINDIR)/uncompress
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/uncompress
	rm -f $(DESTDIR)$(BINDIR)/zcat
	ln -s $(PROG) $(DESTDIR)$(BINDIR)/zcat

clean:
	rm -f $(PROG) $(OBJS) uncompress zcat

.PHONY: all test difftest install clean
