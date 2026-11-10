# Makefile - init, the first process (POSIX make), linked statically.
#
#   make              build it
#   make install      install it as $(DESTDIR)$(BINDIR)/init
#   make clean        remove what was built

C17 = c17 -B static
BINDIR = /sbin

init: init.c
	$(C17) -o $@ init.c

install: init
	mkdir -p $(DESTDIR)$(BINDIR)
	cp init $(DESTDIR)$(BINDIR)/init

clean:
	rm -f init

.PHONY: install clean
