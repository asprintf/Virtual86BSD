# Makefile - the kernel, compiled in this directory (POSIX make).  Until
# config(8) exists (usr.sbin/config) this file is written by hand; it has
# the form config's output will have.
#
#   make            bsd (a Multiboot image) and bsd.elf
#   make proto      sys/syscallargs.h again
#   make gnu        with the host's gcc and binutils instead of c17
#   make run        boot it in QEMU on disk.img
#
# vers.c (the banner, ostype, osrelease: sys/conf/newvers.sh) is made again
# whenever an object changes; BUILDER=user@host names the builder in it.
#
# By default the system's own tools build it: c17 compiles the C, assembles the
# assembly and links the kernel (with ld.script) -- inside a running system,
# nothing else is needed.

S = ../../../..
CC = c17
CFLAGS = -D_KERNEL -DDIAGNOSTIC -I$(S) -I.
AS = c17 -c
LD = c17
LDFLAGS = -B standalone
HOSTCC = c17

GNU_CFLAGS = -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -ffreestanding -fno-pic \
	-mcmodel=kernel -mno-red-zone -mno-sse -mno-mmx -mno-80387 -fno-stack-protector \
	-fno-asynchronous-unwind-tables -fno-omit-frame-pointer -D_KERNEL -DDIAGNOSTIC -I$(S) -I.
GNU_LDFLAGS = -nostdlib -z max-page-size=4096 -z noexecstack --no-warn-rwx-segments -T
QEMU = qemu-system-x86_64
QEMUFLAGS = -m 512M -nographic -no-reboot
APPEND = root=/dev/vda CONSOLE=/dev/ttyS0
DISK = disk.img

OBJS0 = boot.o cpu.o entry.o x86.o init_main.o libkern.o \
	subr_prf.o kern_sched.o kern_proc.o kern_sig.o kern_exec.o sys_futex.o \
	kern_time.o kern_xxx.o sysv_ipc.o sys_mqueue.o init_sysent.o uvm_page.o \
	kern_malloc.o uvm_map.o uvm_mmap.o vfs_subr.o kern_descrip.o vfs_lookup.o \
	vfs_bio.o ext2fs.o tmpfs.o procfs.o sys_pipe.o vfs_lockf.o \
	uipc_socket.o vfs_pcache.o ip.o tcp.o pci.o if_em.o \
	kern_tym.o com.o vga.o pckbc.o pciide.o vioblk.o \
	tty.o mem.o
OBJS = $(OBJS0) vers.o

all: bsd

bsd: bsd.elf mkimage
	./mkimage bsd.elf $@

bsd.elf: machine $(OBJS) $(S)/arch/amd64/conf/ld.script
	$(LD) $(LDFLAGS) $(S)/arch/amd64/conf/ld.script -o $@ $(OBJS)

gnu:
	$(MAKE) CC=gcc CFLAGS="$(GNU_CFLAGS)" AS=as LD=ld LDFLAGS="$(GNU_LDFLAGS)" HOSTCC=cc all

mkimage: $(S)/arch/amd64/stand/mkimage/mkimage.c
	$(HOSTCC) -o $@ $(S)/arch/amd64/stand/mkimage/mkimage.c

# <machine/...> is this architecture's include directory
machine:
	ln -s $(S)/arch/amd64/include machine

HDRS = $(S)/sys/param.h $(S)/sys/systm.h $(S)/sys/types.h \
	$(S)/sys/errno.h $(S)/sys/signal.h $(S)/sys/signalvar.h \
	$(S)/sys/proc.h $(S)/sys/termios.h $(S)/sys/tty.h \
	$(S)/sys/socket.h $(S)/sys/socketvar.h $(S)/sys/vnode.h \
	$(S)/sys/mount.h $(S)/sys/file.h $(S)/sys/conf.h \
	$(S)/sys/buf.h $(S)/sys/syscallargs.h $(S)/uvm/uvm_extern.h \
	$(S)/uvm/uvm.h $(S)/net/if_var.h $(S)/netinet/in_var.h \
	$(S)/dev/pci/pcivar.h $(S)/arch/amd64/include/cpu.h $(S)/arch/amd64/include/_types.h \
	$(S)/arch/amd64/include/vmparam.h

$(OBJS0): machine $(HDRS)

vers.c: $(S)/conf/newvers.sh $(S)/conf/RELDATE $(OBJS0)
	sh $(S)/conf/newvers.sh GENERIC
vers.o: vers.c
	$(CC) $(CFLAGS) -c -o $@ vers.c

boot.o: $(S)/arch/amd64/amd64/boot.s
	$(AS) -o $@ $(S)/arch/amd64/amd64/boot.s
cpu.o: $(S)/arch/amd64/amd64/cpu.s
	$(AS) -o $@ $(S)/arch/amd64/amd64/cpu.s
entry.o: $(S)/arch/amd64/amd64/entry.s
	$(AS) -o $@ $(S)/arch/amd64/amd64/entry.s
x86.o: $(S)/arch/amd64/amd64/x86.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/arch/amd64/amd64/x86.c
init_main.o: $(S)/kern/init_main.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/init_main.c
libkern.o: $(S)/lib/libkern/libkern.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/lib/libkern/libkern.c
subr_prf.o: $(S)/kern/subr_prf.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/subr_prf.c
kern_sched.o: $(S)/kern/kern_sched.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_sched.c
kern_proc.o: $(S)/kern/kern_proc.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_proc.c
kern_sig.o: $(S)/kern/kern_sig.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_sig.c
kern_exec.o: $(S)/kern/kern_exec.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_exec.c
sys_futex.o: $(S)/kern/sys_futex.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/sys_futex.c
kern_time.o: $(S)/kern/kern_time.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_time.c
kern_xxx.o: $(S)/kern/kern_xxx.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_xxx.c
sysv_ipc.o: $(S)/kern/sysv_ipc.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/sysv_ipc.c
sys_mqueue.o: $(S)/kern/sys_mqueue.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/sys_mqueue.c
init_sysent.o: $(S)/kern/init_sysent.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/init_sysent.c
uvm_page.o: $(S)/uvm/uvm_page.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/uvm/uvm_page.c
kern_malloc.o: $(S)/kern/kern_malloc.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_malloc.c
uvm_map.o: $(S)/uvm/uvm_map.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/uvm/uvm_map.c
uvm_mmap.o: $(S)/uvm/uvm_mmap.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/uvm/uvm_mmap.c
vfs_subr.o: $(S)/kern/vfs_subr.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/vfs_subr.c
kern_descrip.o: $(S)/kern/kern_descrip.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_descrip.c
vfs_lookup.o: $(S)/kern/vfs_lookup.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/vfs_lookup.c
vfs_bio.o: $(S)/kern/vfs_bio.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/vfs_bio.c
ext2fs.o: $(S)/ufs/ext2fs/ext2fs.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/ufs/ext2fs/ext2fs.c
tmpfs.o: $(S)/tmpfs/tmpfs.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/tmpfs/tmpfs.c
procfs.o: $(S)/miscfs/procfs/procfs.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/miscfs/procfs/procfs.c
sys_pipe.o: $(S)/kern/sys_pipe.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/sys_pipe.c
vfs_lockf.o: $(S)/kern/vfs_lockf.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/vfs_lockf.c
uipc_socket.o: $(S)/kern/uipc_socket.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/uipc_socket.c
vfs_pcache.o: $(S)/kern/vfs_pcache.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/vfs_pcache.c
ip.o: $(S)/netinet/ip.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/netinet/ip.c
tcp.o: $(S)/netinet/tcp.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/netinet/tcp.c
pci.o: $(S)/dev/pci/pci.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/pci/pci.c
if_em.o: $(S)/dev/pci/if_em.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/pci/if_em.c
kern_tym.o: $(S)/kern/kern_tym.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/kern_tym.c
com.o: $(S)/dev/ic/com.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/ic/com.c
vga.o: $(S)/dev/ic/vga.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/ic/vga.c
pckbc.o: $(S)/dev/ic/pckbc.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/ic/pckbc.c
pciide.o: $(S)/dev/pci/pciide.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/pci/pciide.c
vioblk.o: $(S)/dev/pv/vioblk.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/dev/pv/vioblk.c
tty.o: $(S)/kern/tty.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/kern/tty.c
mem.o: $(S)/arch/amd64/amd64/mem.c
	$(CC) $(CFLAGS) -c -o $@ $(S)/arch/amd64/amd64/mem.c

proto:
	sh $(S)/kern/makesyscalls.sh $(S)

run: bsd
	$(QEMU) $(QEMUFLAGS) -kernel bsd -append "$(APPEND)" -drive file=$(DISK),format=raw,if=virtio

clean:
	rm -f $(OBJS) vers.c vers.c.new bsd bsd.elf mkimage machine
