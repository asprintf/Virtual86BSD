# Makefile - init, the first process (POSIX make), linked statically.
C17 = c17 -B static

init: init.c poweroff.s
	$(C17) -o $@ init.c poweroff.s

clean:
	rm -f init
