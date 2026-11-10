# Makefile - the kernel's test programs (POSIX make), for the system's
# /root/ktest (distrib/mkdisk-ext2.sh copies them in): k_* here, and the C
# library's self-checking tests in libc/.  Linked statically with c17.
#
#   make testprogs

C17 = c17 -B static
LIBCTESTS = ../../lib/libc

testprogs:
	$(C17) -o k_sigwait k_sigwait.c
	$(C17) -o k_shm k_shm.c
	$(C17) -o k_fpu k_fpu.c k_fpu_s.s
	$(C17) -o k_clofork k_clofork.c k_clofork_s.s
	$(C17) -o k_mmap k_mmap.c
	$(C17) -o k_inet k_inet.c
	$(C17) -o k_net k_net.c
	$(C17) -o k_timer k_timer.c
	$(C17) -o k_maxmap k_maxmap.c
	$(C17) -o k_disk k_disk.c
	mkdir -p libc
	cp $(LIBCTESTS)/collate.def libc/
	for t in $(LIBCTESTS)/s_*.c; do \
		b=`basename $$t .c`; \
		$(C17) -o libc/$$b $$t || echo "$$b: not built"; \
	done

clean:
	rm -f k_sigwait k_shm k_fpu k_clofork k_mmap k_inet k_net k_timer k_maxmap k_disk libc/s_* libc/collate.def

.PHONY: testprogs clean
