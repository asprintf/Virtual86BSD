# Makefile - the Shall kernel (POSIX make).
#
#   make            shall-kernel (a Multiboot image) and shall-kernel.elf
#   make init       user/init, the first process (/sbin/init)
#   make testprogs  test programs (tests/k_*, and the C library's tests)
#   make run        boot it in QEMU on disk.img (text console on stdio)
#   make disk.img   an ext2 image of a Shall root (ROOT=dir, default ../root,
#                   as bootstrap.sh makes it), with user/init and the test
#                   programs if built; needs the host's mke2fs and QEMU
#
# By default Shall's own tools build it: c17 compiles the C, assembles the
# assembly (arch/*.s) and links the kernel (with kernel.ld) -- inside a
# running Shall system, nothing else is needed.  "make gnu" builds it with
# the host's gcc and binutils instead.

CC = c17
CFLAGS = -Iinclude
AS = c17 -c
LD = c17
LDFLAGS = -B standalone
HOSTCC = c17

# the host toolchain ("make gnu")
GNU_CFLAGS = -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -ffreestanding -fno-pic \
	-mcmodel=kernel -mno-red-zone -mno-sse -mno-mmx -mno-80387 -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-omit-frame-pointer -Iinclude
GNU_LDFLAGS = -nostdlib -z max-page-size=4096 -z noexecstack --no-warn-rwx-segments -T
QEMU = qemu-system-x86_64
QEMUFLAGS = -m 512M -nographic -no-reboot
ROOT = ../root
APPEND = root=/dev/vda CONSOLE=/dev/ttyS0

OBJS = arch/boot.o arch/cpu.o arch/entry.o arch/x86.o \
	kern/main.o kern/lib.o kern/printk.o kern/sched.o kern/proc.o kern/signal.o \
	kern/exec.o kern/futex.o kern/time.o kern/misc.o kern/ipc.o kern/mqueue.o kern/syscall.o \
	mm/page.o mm/kmalloc.o mm/vm.o mm/mmap.o \
	fs/vfs.o fs/file.o fs/namei.o fs/bcache.o fs/ext2.o fs/tmpfs.o fs/procfs.o \
	fs/pipe.o fs/lock.o fs/socket.o fs/pcache.o \
	net/ip.o net/tcp.o drivers/pci.o drivers/e1000.o drivers/tym.o \
	drivers/serial.o drivers/vga.o drivers/kbd.o drivers/ata.o drivers/virtio_blk.o drivers/tty.o drivers/mem.o

all: shall-kernel

shall-kernel: shall-kernel.elf tools/mkimage
	./tools/mkimage shall-kernel.elf $@

shall-kernel.elf: $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) kernel.ld -o $@ $(OBJS)

gnu:
	$(MAKE) CC=gcc CFLAGS="$(GNU_CFLAGS)" AS=as LD=ld LDFLAGS="$(GNU_LDFLAGS)" HOSTCC=cc all

tools/mkimage: tools/mkimage.c
	$(HOSTCC) -o $@ tools/mkimage.c

$(OBJS): include/kernel.h include/net.h include/errno.h include/arch.h include/mm.h include/proc.h \
	include/signal.h include/fs.h include/dev.h include/tty.h include/syscall.h include/sysproto.h

.SUFFIXES: .c .s .o
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
.s.o:
	$(AS) -o $@ $<

include/sysproto.h:
	sh tools/gensyscalls.sh

proto:
	sh tools/gensyscalls.sh

# programs for the system, with c17 and Shall's C library, linked
# statically (init is process 1)
C17 = c17 -B static
LIBCSRC = ../libc

init: user/init

user/init: user/init.c user/poweroff.s
	$(C17) -o $@ user/init.c user/poweroff.s

# test programs: tests/k_* and the C library's self-checking tests in
# tests/libc
testprogs:
	$(C17) -o tests/k_sigwait tests/k_sigwait.c
	$(C17) -o tests/k_shm tests/k_shm.c
	$(C17) -o tests/k_fpu tests/k_fpu.c tests/k_fpu_s.s
	$(C17) -o tests/k_clofork tests/k_clofork.c tests/k_clofork_s.s
	$(C17) -o tests/k_mmap tests/k_mmap.c
	$(C17) -o tests/k_inet tests/k_inet.c
	$(C17) -o tests/k_net tests/k_net.c
	$(C17) -o tests/k_timer tests/k_timer.c
	$(C17) -o tests/k_maxmap tests/k_maxmap.c
	mkdir -p tests/libc
	cp $(LIBCSRC)/tests/collate.def tests/libc/
	for t in $(LIBCSRC)/tests/s_*.c; do \
		b=`basename $$t .c`; \
		$(C17) -o tests/libc/$$b $$t || echo "$$b: not built"; \
	done

disk.img: FRC
	sh tools/mkdisk.sh $(ROOT) $@
FRC:

run: shall-kernel
	$(QEMU) $(QEMUFLAGS) -kernel shall-kernel -append "$(APPEND)" -drive file=disk.img,format=raw,if=virtio

clean:
	rm -f $(OBJS) shall-kernel shall-kernel.elf tools/mkimage user/init
	rm -f tests/k_sigwait tests/k_shm tests/k_fpu tests/k_clofork tests/k_mmap tests/k_inet tests/k_net tests/k_timer tests/k_maxmap tests/libc/s_* tests/libc/collate.def
