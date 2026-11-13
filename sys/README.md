# The Virtual/86 BSD kernel

A small x86-64 kernel that runs V86BSD's programs, and Linux programs, as
Linux would: the same system call numbers, structures, ELF loading, initial
stack, signal frames and `/proc` files.  It boots in QEMU in text mode,
mounts the root file system (FFS2; `rootfstype=ext2` for an ext2 disk) and starts `/sbin/init`.

First version (November 2026): one processor; networking over an Intel
e1000 card (QEMU's default) and the loopback interface: IPv4 with ARP,
ICMP, UDP and TCP on the wire, DHCP, and IPv4/IPv6 TCP, UDP and raw
sockets on the loopback; no graphics.  About 23 000 lines of C and 700 of
assembly.

## Status

Inside QEMU, on a disk made from a bootstrapped V86BSD root:

* The interactive shell on the serial console, with a controlling terminal
  and job control (Ctrl-C, Ctrl-Z, `jobs`, `fg`, `bg`); `vi`, `more`,
  `ps`, `talk` etc.
* c17 compiles, assembles and links programs inside the system, and
  `make` builds the utilities there.
* The utility test suite runs inside the system, each utility first
  rebuilt there by c17 (see "Testing"): 125 suites, 6735 tests passed and
  5 failed in the latest full run (with the system's programs linked
  dynamically) (the first one had 17 failures; 12 were
  fixed: `/proc/self/fd` as Linux's "magic" links for `uuencode`,
  pseudo-terminal output no longer dropped for `ed`).  Left: `bc` (endless recursion in a
  1 GiB thread stack exhausts the VM's memory), `mailx` (2: the expected
  output depends on the length of the day of the month), `ed` (an
  interrupt test whose 5-second window is too short under emulation) and
  one `talk` case ("rung again after 10 seconds", not yet understood).
* The C library's self-checking tests: 22 of 23 pass (`s_net` with its
  own DNS server on 127.0.0.1:53, `s_sporadic` and `s_typedmem` among
  them; `s_collsym` with locales `regress/sys/kern/libc/run.sh` makes there with
  V86BSD's own `localedef`); `s_math` meets QEMU's software x87, a few ulp
  off in `y1`/`jn` (the floating-point state a process gets is the same as
  on Linux, checked by `tests/k_fpu`).
* The Open POSIX Test Suite (LTP's, 1888 tests) built and run inside the
  system: of the 1777 tests that compile (the other 111 use interfaces
  POSIX.1-2024 removed), 1711 passed in the first run, glibc on Linux
  1693.  What it found here was fixed: a POSIX timer queues one signal at
  a time and counts the expirations while it is pending as overruns
  (`timer_getoverrun`, `si_overrun`), and an interval of 1 ns no longer
  freezes the system (the expirations are computed, not counted one by
  one); `sigqueue` with signal 0, `ESRCH` and `EPERM`; a `nanosleep` or
  relative `clock_nanosleep` stopped and continued goes on to its
  deadline instead of failing with `EINTR`; absolute `CLOCK_REALTIME`
  timers and sleeps follow `clock_settime`, and relative sleeps on it do
  not (they are measured on the monotonic clock); `wait` with `SA_NOCLDWAIT`
  ends with `ECHILD`; a shared mapping of an unlinked file is written
  back; `mlock`/`mlockall` mark the memory, so `msync(MS_INVALIDATE)`
  fails with `EBUSY` there; futexes wake the highest priority first (so
  `sem_post` does); `PROT_NONE` gives `SIGSEGV`, not `SIGBUS`;
  `RLIMIT_AS` is enforced (`mmap`, `brk`); a process has at most 65530
  mappings (as Linux), and finding room for a new one no longer takes time
  quadratic in their number (`mmap` right below the one placed last, else
  one walk down the sorted list).
* os-test (Sortix's, 5816 tests) built inside the system and run on this
  kernel: 5610 pass, glibc on Linux 5228 (`libc/STATUS.md`, stage 18).
  It found here: a pseudoterminal's slave opened with `O_NOCTTY` still
  became the controlling terminal (the flag is not kept in the file, and
  the pty's own open looked for it there: `tty_maybe_set_ctty` now does
  ptys too); zombies count as members of their process group (`tcsetpgrp`,
  `setpgid`); `open` of a directory with `O_CREAT` and without
  `O_DIRECTORY` fails with `EISDIR`; `setpgid`/`setsid` wake a parent
  waiting for the old group (it may have no children left in it:
  `ECHILD`); `poll` on a hung-up terminal gives `POLLIN | POLLHUP |
  POLLERR`, not `POLLOUT`; `F_GETLK`/`F_OFD_GETLK` of `F_UNLCK` answer
  `F_UNLCK`; a new pseudoterminal is 0x0 until its controller sets a size.
* The network: `k_net` passes through QEMU's user networking (UDP,
  fragmented both ways; 2 MiB of TCP each way; refused and non-blocking
  connects; ping; a server reached from the host), also with every 13th
  frame lost each way.
* The kernel is built by V86BSD's own c17 alone (it compiles the C,
  assembles `arch/amd64/amd64/*.s` and links with `ld.script`; no binutils), on the
  host by `bootstrap.sh` and inside the running system by `config GENERIC` and `make` in
  `/usr/src/sys/arch/amd64/compile/GENERIC`, where it comes out byte for byte the same.
  The whole system rebuilds itself inside (`bootstrap.sh`, 41 minutes
  under QEMU's emulation) into files identical to those the host built.
  `make gnu` builds it with gcc and binutils instead.

## Beyond Linux

Where POSIX asks for something Linux lacks, the kernel provides it with
an interface a Linux kernel would ignore or reject, so that the C library
can find out at run time and keep its emulation on Linux:

* Close-on-fork (POSIX.1-2024): `O_CLOFORK` (`010000000000`) in `open`,
  `dup3` and `pipe2`; `FD_CLOFORK` (2) in `F_GETFD`/`F_SETFD`;
  `F_DUPFD_CLOFORK` (`0x10000`); `SOCK_CLOFORK` in `socket`,
  `socketpair` and `accept4`; `MSG_CMSG_CLOFORK` in `recvmsg`.  A new
  process (`fork`, `vfork`, `clone` without `CLONE_THREAD`, hence also
  `posix_spawn`) gets a table without the flagged descriptors; exec keeps
  them and clears the flag (Austin Group interpretation 1851, for TC1: the
  2024 text kept it).  The
  library tests once whether `F_GETFD` reports `FD_CLOFORK` after an
  `open` with `O_CLOFORK` (`regress/sys/kern/k_clofork` checks that it then uses the
  kernel's).
* The sporadic server (POSIX `SCHED_SPORADIC`, for processes and
  threads): policy 4, a number Linux leaves unused and refuses, with the
  long `struct sched_param` (POSIX's `sched_ss_*` members after
  `sched_priority`, read and written only for this policy).  The library's
  `sysconf(_SC_SPORADIC_SERVER)` asks `sched_get_priority_max(4)`.
* Typed memory objects: pools of physically contiguous memory, set aside
  at boot (`tym=ram0:4M,ram1:1M` is the default), as `/dev/tym/NAME`;
  `/dev/tymctl` answers `posix_mem_offset`.  The library's
  `posix_typed_mem_open("/NAME", ...)` opens the device and passes the
  allocation flags by ioctl.

## Layout

The tree is that of OpenBSD's `sys/`.  Headers are included as
`<sys/systm.h>`, `<machine/cpu.h>`, `<uvm/uvm_extern.h>`: a kernel is built
with `-D_KERNEL -I<sys> -I.` in a compile directory that has a `machine`
link to `arch/amd64/include`.  The compile directory is made by
`config(8)` (`usr.sbin/config`) from `conf/files`, `arch/amd64/conf/files.amd64`
and the configuration, `arch/amd64/conf/GENERIC`: the Makefile (from
`Makefile.amd64`), `ioconf.c` (the device table), `locators.h`, `options.h`
and the `machine` link.

    arch/amd64/amd64/  boot.s (Multiboot entry, long mode), entry.s (traps,
                       system calls, context switch, user copies), cpu.s
                       (instructions C cannot express), x86.c (GDT/TSS/IDT,
                       PIC, PIT, trap dispatch), mem.c (memory devices)
    arch/amd64/include machine headers: cpu.h, vmparam.h, _types.h
                       autoconf.c (cpu_configure), mainbus.c and cpu_attach.c
                       (mainbus0, cpu0)
    arch/amd64/conf/   GENERIC (the configuration), files.amd64 (its sources and
                       devices), Makefile.amd64 (the template config fills in),
                       ld.script (the linker script)
    arch/amd64/compile/GENERIC/  where config writes what builds the kernel (and
                       the kernel is built); not in the tree
    conf/              files (the machine-independent sources and devices),
                       newvers.sh and RELDATE (vers.c: the banner)
    arch/amd64/stand/  mkimage (ELF to flat Multiboot image)
    kern/              init_main, subr_prf (printf, panic), the scheduler
                       (kern_sched), processes (kern_proc, kern_exec,
                       kern_sig, kern_time), System V IPC (sysv_ipc), futexes,
                       POSIX message queues, kern_xxx (uname/sysinfo/
                       reboot/syslog), init_sysent (the native system call
                       table, generated from syscalls.master),
                       the VFS (vfs_subr, vfs_lookup, vfs_bio, vfs_pcache,
                       vfs_lockf), descriptors (kern_descrip), pipes,
                       sockets (uipc_socket), terminals (tty), kern_malloc,
                       typed memory (kern_tym), autoconfiguration
                       (subr_autoconf), disks as devices (subr_disk);
                       makesyscalls.sh (syscalls.conf: what it writes)
    miscfs/deadfs/     revoke(2): the dead files a device's open files become
    compat/linux/      the Linux emulation (option COMPAT_LINUX): its own
                       table (syscalls.master, linux_sysent.c), linux_exec
                       (struct emul), linux_misc (uname, personality, reboot)
    uvm/               physical pages (buddy allocator with reference
                       counts), address spaces and page faults, mmap & co.
    ufs/ffs/          FFS2 (the root), ufs/ext2fs/ ext2 (to read other
                       disks; tmpfs/ and miscfs/procfs/ are tmpfs/devtmpfs
                       and procfs)
    net/               if.c (the interfaces: lo0, em0, vio0, their
                       addresses and flags), route.c (the routing table)
    netinet/           ip.c (Ethernet, ARP, IPv4 with fragments, ICMP, UDP,
                       DHCP, the network thread), tcp.c (TCP)
    dev/               ic/ (16550 serial console, VGA text console, PS/2
                       keyboard), isa/ (the ISA bus), pci/ (PCI and pcidevs.h,
                       e1000 as if_em.c, ATA disks (PIO) as pciide.c), pv/
                       (virtio, vioblk, virtio-net as if_vio.c)
    sys/ net/ ...      the kernel's headers (its own types; no C library
                       headers)
    lib/libkern/       memory and string routines

`sbin/init` is init (POSIX C, built with V86BSD's c17 and C library),
`regress/sys/kern` the test programs (k_*.c, and the C library's tests, put
on the disk image in /root/ktest) and `distrib/mkdisk-ffs.sh` makes the
FFS2 image of a V86BSD root (`mkdisk-ext2.sh` an ext2 one).

## Configuration and autoconfiguration

Which drivers are in a kernel, and where they attach, is a configuration
file, as in OpenBSD: `arch/amd64/conf/GENERIC`.  `config(8)`
(`usr.sbin/config/README.md`) reads it with `conf/files` and
`arch/amd64/conf/files.amd64` and writes the compile directory
`arch/amd64/compile/NAME` (`cd arch/amd64/conf; config NAME`; `buildkernel`
does it, `KERNCONF=NAME` picks the configuration).  A device that is not in
the file is not compiled in: `file dev/pci/if_em.c em` in `conf/files` is
the line for em, and a kernel without `em* at pci?` has no network driver.

`kern/subr_autoconf.c` is the kernel's side, as OpenBSD's: `struct cfdriver`
and `struct cfattach` (match and attach functions) in each driver,
`cfdata[]` (the table, `ioconf.c`), `config_found()` for a bus to offer what
it found, `config_scan()` for one that has to ask the table.  `kmain` calls
`cpu_configure()`, which finds `mainbus0`; its attach finds `cpu0`, `pci0`
and `isa0`, and so on down.  The tree is printed as it is found:

    mainbus0 at root
    cpu0 at mainbus0: (uniprocessor)
    pci0 at mainbus0 bus 0
    "Intel 82441FX" rev 0x02 at pci0 dev 0 function 0 not configured
    pciide0 at pci0 dev 1 function 1 "Intel 82371SB IDE" rev 0x00
    disk0 at pciide0 channel 0 drive 0: <QEMU HARDDISK>
    vga0 at pci0 dev 2 function 0 "QEMU Standard VGA" rev 0x02: console (80x25)
    wsdisplay0 at vga0: console (std, vt100 emulation)
    em0 at pci0 dev 3 function 0 "Intel 82540EM" rev 0x03: irq 11, address 52:54:00:12:34:56
    virtio0 at pci0 dev 4 function 0 "Qumranet Virtio Storage" rev 0x00
    vioblk0 at virtio0
    disk1 at vioblk0: <VirtIO, Block Device>
    isa0 at mainbus0
    com0 at isa0 port 0x3f8/8 irq 4: ns16550a, 16 byte fifo
    pckbc0 at isa0 port 0x60/5 irq 1
    pckbd0 at pckbc0 (kbd slot)

`dev/pci/pcidevs.h` has the names of the vendors and products of QEMU's
machines; what is not in it is printed with its numbers, and a device no
driver takes gets `not configured`.  The console is set up before any of
this (`serial_init`, `vga_init`, polled output), so the first messages
appear; `com0` and `vga0` attach to it later.  `vga0` is the PCI display adapter
(class display, subclass VGA); the text console it shows is the legacy VGA
at its fixed ports and memory.  The messages are OpenBSD's:
the banner, `real mem` and `avail mem`, `bootargs:` (what follows the
kernel's name on the command line, if anything), the `xx0 at yy0` tree
(`cpu0: apid 0 (boot processor)` and its brand string, MHz and
family-model-stepping; `pchb0` and `pcib0` as the PCI bridges, `isa0 at pcib0`
once the PCI bus is walked; `wskbd0` and `wsdisplay0` only say what the
console is), `tym0:` and `root on disk0s1 (ffs, read-only)`.  The kernel's own `printf`
is what prints them.  A panic is `panic: ...` and `The operating system has
halted.`; a trap in the kernel is `kernel: ... trap, code=...`; a reboot says
`syncing disks... done` and then `rebooting...`, `powering off...` or the
halt line.

### Devices, names and partitions

* The network card is `em0` (Intel e1000) or `vio0` (virtio-net, QEMU's
  `-nic model=virtio-net-pci`, `boot.sh -v`), and `lo0` the loopback: in
  `SIOCGIFNAME` and friends, `getifaddrs()`, `/proc/net/dev` and the DHCP
  line.  A second card is em1 or vio1.
* The serial console, COM1, is `/dev/tty00` (dial-in) and `/dev/cua00`
  (call-out): the same device, major 4 minor 64.  The VGA console is
  `/dev/ttyC0` (major 4 minor 1).  `/dev/pts` and `ptmx` stay.
* Disks are `disk0`, `disk1` ... in the order they attach, whichever
  controller (`pciide`, `vioblk`) they are on: `/dev/disk0` is the whole
  disk (block device, major 8 minor `unit * 16`) and `/dev/disk0s1` ... its
  partitions (minor `unit * 16 + n`, n from 1, at most 15).
  `/dev/rdisk0` and `/dev/rdisk0s1` are the raw devices: character devices
  (major 241, the same minors) that read and write the sectors with no buffer
  cache, in whole sectors from a multiple of 512.  A partition is a block
  device of its own, so it has buffers of its own: nothing should write
  `disk0` while one of its partitions is mounted.
* The table is read when the disk attaches (`kern/subr_disk.c`): the four
  primary entries of an MBR (extended partitions are not looked at), or the
  GPT if the MBR is a protective one (type 0xee): both headers and the entry
  array have to have good CRC-32s, else dmesg says so (`disk0: GPT entries
  checksum is bad`) and the disk has no partitions.
  `disk0: MBR, 1 partition` is what dmesg says.
* The root is `root=` on the command line, else the `root on` of the
  configuration (GENERIC: `disk0s1`); if that is not there, the first
  disk (an image with no partition table: `root on disk0`).
  `mkdisk.sh` makes an image with an MBR and one partition (type 0xa6, 0x83
  for ext2) from sector 2048 (1 MiB), the file system: `dd skip=2048` takes it
  out (`fsck_ffs` checks that file); the system checks its own root at boot.

## Building and running

The kernel is part of V86BSD (`sys/` in its sources): `bootstrap.sh`
builds it with the c17 of its stage 2, together with `/sbin/init`, into
`root/bsd` and `root/sbin/init`.  To boot that root,
the host needs QEMU (`qemu-system-x86_64`); `makefs` of the build makes the image
(`mkdisk.sh -t ext2` wants e2fsprogs' `mke2fs -d`);
in the top directory:

    sh mkdisk.sh                  # v86bsd.img: the root as FFS2 (1 GiB)
    sh boot.sh                    # QEMU; the console is this terminal

(The head of `boot.sh` lists its options.)  Inside the system everything
needed is there:

    cd /usr/src/sys/arch/amd64/conf
    config GENERIC                # the compile directory, ../compile/GENERIC
    cd ../compile/GENERIC
    make                          # bsd: c17 compiles, assembles, links
    cd /usr/src/sbin/init && make       # /sbin/init
    cd /usr/src/regress/sys/kern && make testprogs  # the test programs

The Makefile's defaults are V86BSD's tools (`CC=c17`, `AS="c17 -c"`,
`LD=c17` with `LDFLAGS="-B standalone"`, `HOSTCC=c17` for mkimage);
programs for the system (`init`, the tests) are made with `C17="c17 -B
static"`.  On a host, the c17 of a bootstrap kept with `-k` serves
(`obj/stage2/root/usr/bin`, statically linked, with
`C17_LIBDIR=obj/stage2/root/usr/lib/c17 C17_LIBC=root/usr`), or `make
gnu` uses gcc and binutils.

`make run` uses `-nographic`: the serial port is the console, Ctrl-A x
leaves QEMU.  Without `-nographic` the VGA screen and the PS/2 keyboard
work too (both are the same console terminal).  When the console shell
exits, init syncs and powers off.

c17 links the kernel by its own rules (read-only data, code, data, each
segment on its own page) and takes from `kernel.ld` only the address of
the image, the entry point and `_kernel_end`; the Multiboot header is in a
section `.boot`, which c17 (and `kernel.ld` for ld) puts first, and
`mkimage` makes the flat image from the program headers either way.

Kernel command line (`-append`): `root=/dev/disk0s1` (default: the
`root on` of the configuration, GENERIC: disk0s1),
`rootfstype=ext2` (default ffs), `ro`, `rw` (an FFS root is read-only until rc has checked it), `init=/path`; `ip=dhcp` (default when there is a
card; it configures the first one), `ip=off` or `ip=ADDR/PREFIX[,GATEWAY[,DNS]]`; `tym=NAME:SIZE,...`
(typed memory pools, `off` for none); `netloss=N` (testing: lose every
Nth frame each way).  As on Linux, other `name=value`
words become init's environment; init uses `CONSOLE` (default
`/dev/tty00`) as the terminal for its shell.

## How it boots

1. QEMU (or GRUB) loads the flat image at 1 MiB (Multiboot address
   kludge: the loader needs no ELF64 support).  `boot.s` maps the first GiB
   at 0, at the direct map 0xffff888000000000 and at 0xffffffff80000000,
   enables long mode and calls `kmain` in the higher half.
2. `kmain`: memory map, page allocator (the first GiB), direct map of all
   RAM, then the rest of the memory to the allocator; GDT/IDT,
   TSC calibration, scheduler, timer at 1000 Hz, devices, root mount.
3. Process 1 mounts devtmpfs on `/dev` and proc on `/proc` (mount(8) and
   umount(8) are programs now, and `/etc/rc` mounts the rest), opens `/dev/console` as 0, 1, 2
   and executes `init=`, `/sbin/init`, `/etc/init`, `/bin/init` or
   `/bin/sh`.
4. `/sbin/init` (sbin/init/init.c) runs `/etc/rc` if present, then a getty
   (libexec/getty) on each line of `/etc/ttys` that is on, each as a session
   leader with its terminal as controlling terminal (`/dev/console` itself never
   becomes one); without `/etc/ttys`, `sh` on the console instead.  getty asks for
   a name and runs login (usr.bin/login).  The kernel passes `-s` from its command
   line to init for single-user mode.

## Design

* **Memory.** User space below 0x800000000000, Linux's layout: programs at
  0x400000 (ET_EXEC) or 0x555555554000 (PIE), mmap top-down from just below
  the stack, stack at 0x7ffffffff000.  Private mappings are copy on write;
  file pages are read at the first touch.  Anonymous shared memory, SysV
  segments, tmpfs files and the page cache share one "shared memory
  object" type.
* **Page cache** (`kern/vfs_pcache.c`). A `MAP_SHARED` mapping of an FFS or ext2 file
  maps the file's cached pages, so every process mapping it shares them,
  and `read`/`write` use a cached page where there is one, so they agree
  with the mappings at once.  Pages a shared mapping has mapped are
  written back where they differ from the file (compared, so no reverse
  mappings are needed) on `msync`, `fsync`, `sync`, the 5-second flush,
  the last unmapping and when the inode goes; that sets `st_mtime`.  Whole
  pages beyond the end of the file are `SIGBUS`, also after `ftruncate`
  shrinks the file under a mapping.  Private mappings (programs, shared
  libraries) map the cache's pages too, copy on write (also where
  `mprotect` makes them writable later, or after `fork`), so every
  process running a program shares its pages; a `write` shows in a page
  not yet copied, as on Linux.  The cache exists while the file is
  mapped (reading and writing otherwise go through the buffer cache).
* **Processes.** A `struct task` per thread, a `struct proc` per process
  (thread group): credentials, nice value, descriptors, signal actions and
  record locks belong to the process, as POSIX says (Linux keeps several of
  these per thread; V86BSD's C library broadcasts set*id and nice to every
  thread anyway, so programs see the same).  `clone` takes the flag
  combinations of fork, vfork, posix_spawn and threads.
* **Scheduling.** One processor, round robin with 10 ms slices; SCHED_FIFO
  and SCHED_RR priorities are honoured, with priority inheritance through
  PI futexes and Linux's real-time throttling (95 %; while throttled,
  real-time tasks are time-sliced, and the end of the throttle reschedules).
  SCHED_SPORADIC (`kern/sched.c`): a server runs as SCHED_FIFO at
  `sched_priority` while it has budget, at `sched_ss_low_priority` when
  not; what it runs at the high priority is charged per tick, and a
  replenishment of what it used since its activation is due a period
  after the activation, when it blocks or runs out (POSIX XSH 2.8.4; at
  most `sched_ss_max_repl` pending, up to 32).  The kernel is not
  preemptive: interrupts are on in the kernel, task switches happen only
  on the way back to user mode or when a task sleeps.
* **Signals.** Linux's `rt_sigframe`/`ucontext`/`siginfo` layouts,
  `SA_RESTART`/`ERESTART*` restarts, alternate stacks, job-control stops
  and orphaned process groups, `SIGTTIN`/`SIGTTOU` from terminals.
* **Files.** VFS with an inode cache and mount table; ext2 (revisions 0
  and 1, 1-4 KiB blocks, large inodes with nanosecond times; refuses
  unknown incompatible features, mounts read-only with unknown read-only
  ones) with a write-back buffer cache flushed every 5 s and by `sync`
  (the disk's own write cache too, then; writing back, also for `sync`,
  lets other tasks run between buffers; the blocks of a deleted or
  shortened file are not written any more).  The cache grows up to half of the memory
  while a sixteenth stays free, and gives memory back when the page
  allocator runs out (`Buffers` in `/proc/meminfo`).  The state of ext2 and
  of FFS is "not clean" while mounted and "clean" again after a proper
  shutdown: an ext2 disk is checked by `e2fsck -p` (which `boot.sh` runs), an
  FFS one mounts read-only when it is not clean and `fsck -p` of `/etc/rc`
  repairs it (and reboots, for a root it changed).  FFS2 (`ufs/ffs/`, from
  OpenBSD's algorithms) has fragment-sized buffers throughout, writes
  inodes, directories and indirect blocks through and the rest later, and
  never panics on a damaged disk: it says so, fails the call and goes
  read-only.  Disks: virtio (`disk0` at `vioblk0`, QEMU's `if=virtio`, what `boot.sh`
  uses: one request per block, waited for by watching the used ring in
  memory) and ATA by PIO (`disk0` at `pciide0`, the PC's IDE disk: polled with
  interrupts on, so the clock and the terminals keep going while the
  disk works; much slower, especially under KVM);
  tmpfs; devtmpfs; procfs (what `ps`, `fuser`, `df`, `ipcs` and the C
  library read); pipes; AF_UNIX sockets with `SCM_RIGHTS` and
  `SCM_CREDENTIALS`; POSIX record locks and OFD locks.
* **Internet sockets** (`kern/uipc_socket.c`). AF_INET and AF_INET6 over the one
  interface, `lo` (127.0.0.0/8, ::1; the wildcard addresses bind to it):
  TCP and UDP with the same connected pairs and queues as AF_UNIX, which
  on a loopback is what TCP and UDP deliver.  Ports (ephemeral 32768-60999,
  below 1024 for root), `EADDRINUSE` with Linux's `SO_REUSEADDR` rules,
  dual-stack AF_INET6 sockets (IPv4 peers as `::ffff:a.b.c.d`) unless
  `IPV6_V6ONLY`, connected UDP sockets that hear only their peer and get
  `ECONNREFUSED` when it is gone.  A UDP socket's address that connect()
  chose (not bind()) is chosen again by the next connect() and dropped by
  a disconnect (`AF_UNSPEC`).  Raw sockets (root): a packet to a
  loopback address reaches the raw sockets of its protocol (IPv4 with its
  header); the kernel answers ICMP and ICMPv6 echo requests.  Other
  destinations: `ENETUNREACH`.  `SIOCGIF*` and `/proc/net/dev` show `lo0`.
* **Network.** `dev/pci/pci.c` finds the card, `dev/pci/if_em.c` drives
  an Intel 8254x (registers memory-mapped through `ioremap`; 64 receive
  and 64 transmit descriptors).  The card's interrupt only wakes the
  network thread, which takes the frames and does all the protocol work
  (with the system calls; the kernel is not preemptive, so they never
  interleave).  `netinet/ip.c`: ARP (cache, queued packets, retries), IPv4
  (fragmentation out, reassembly in), ICMP echo and unreachable (to
  connected UDP sockets as ECONNREFUSED), UDP, a DHCP client;
  `/proc/net/pnp` gives the name server, and `/etc/resolv.conf` on the
  disk image is a link to it, as on Linux with `ip=dhcp`.  `netinet/tcp.c`:
  TCP (RFC 9293's states, MSS option, RTO from smoothed RTT with
  backoff, fast retransmit, slow start and congestion avoidance,
  zero-window probes, early segments kept for the gap, TIME_WAIT 10 s;
  no window scaling, SACK or urgent data).  Sockets to the network are
  the same sockets as to the loopback (`kern/uipc_socket.c`): a TCP socket to
  another host has a `struct tcb`, whose received data goes into the
  socket's queue.  IPv6 is loopback only so far.
  **Interfaces and routes** (`net/if.c`, `net/route.c`, `net/if_var.h`):
  `struct ifnet` as in the BSDs (`if_xname`, `if_flags` with the BSD
  `IFF_` values, `if_mtu`, up to four IPv4 addresses, counters, the
  driver's `if_xmit`/`if_poll`/`if_start`); a driver attaches with
  `if_attach_ether("em", mac, ...)` and the interface is `em0`, then
  `em1`...  `lo0` is index 1 with 127.0.0.1/8 (loopback traffic stays in
  the socket layer).  Every address has its connected route; the table
  (32 entries, `RTF_UP/GATEWAY/HOST`, a metric) is searched for the longest
  prefix and then the lowest metric, and an interface that is down is not
  used.  DHCP and `ip=` put the address, the default route and the name
  servers there; the first Ethernet interface goes up at boot and is the
  one DHCP configures.  A packet from the wire whose source or destination
  is 127.x is dropped.  `ifioctl` (`net/if.c`) and `rtioctl` (`net/route.c`)
  are the socket ioctls of `<sys/sockio.h>`, with the BSDs' names, Linux's
  numbers (a Linux program works under the compat layer: it also sees
  Linux's `IFF_` values) and Linux's 40-byte `struct ifreq`: `SIOCGIFCONF`
  (an entry per address), `SIOC[GS]IFFLAGS` (only `IFF_UP`, `IFF_DEBUG` and
  `IFF_LINK0-2` change; promiscuous and ARP flags are `EOPNOTSUPP`),
  `SIOC[GS]IFADDR` (the first address; setting it keeps its mask, a first
  one gets that of its class), `SIOC[GS]IFNETMASK`, `SIOC[GS]IFBRDADDR` (always
  `addr | ~mask`: only that is accepted), `SIOC[GS]IFMTU` (68 up to the card's
  own), `SIOC[GS]IFMETRIC` (of the connected routes), `SIOCGIFINDEX`,
  `SIOCGIFHWADDR`, and the two of ours in the range Linux leaves to protocols:
  `SIOCAIFADDR` (`struct ifaliasreq`: an alias with its mask) and `SIOCDIFADDR`.
  The getters of the mask and the broadcast address answer for the address
  in `ifr_addr` if the interface has it, else for the first.  The setters are
  root's (`EPERM`).  `SIOCADDRT`/`SIOCDELRT` take Linux's `struct rtentry`
  (`<net/route.h>`; the interface by `rt_dev`, else it is the gateway's), and
  `sysctl net.route` (`CTL_NET, AF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0`) returns the
  table as an array of `struct rtinfo`: not BSD's routing socket messages (a
  `struct sockaddr` has no `sa_len` and there is no `PF_ROUTE` socket), so BSD's
  own `route.c` and `ifconfig.c` do not compile; `sbin/ifconfig`, `sbin/route`
  and libc's `getifaddrs()` (`AF_INET` entries and an `AF_LINK` one per
  interface, `struct sockaddr_dl` without `sdl_len`) are written for these.
  **vio0** (`dev/pv/if_vio.c`) is written from the Virtio 1.2
  specification (5.1) on the legacy PCI interface of `virtio.c`: features
  MAC and STATUS only, a 10-byte header and one descriptor per packet, 64
  receive buffers and 32 transmit buffers of 2 KiB, receive interrupts on
  (the ISR register acknowledges them), transmit interrupts off.  PCI
  interrupt lines can be shared (`irq_register` keeps up to four handlers
  on a line).
* **Network hardening** (what the wire may do to the system).
  *Random numbers* (`arch/amd64/amd64/mem.c`): `get_random_bytes` is
  ChaCha20 with fast key erasure (RFC 8439's block function; checked
  against its test vector), keyed from RDSEED/RDRAND when the processor has
  them and fed by the timer, the keyboard and the arrival of frames; it
  feeds `getrandom(2)`, `/dev/urandom`, the TCP sequence numbers and the
  ports.  *Ethernet and ARP*: frames shorter than a header, or from a
  group address, are dropped; an ARP from a group hardware address or
  from an address that is not a host's is ignored; one that claims one of our
  own addresses is not learned, and is reported ("duplicate IP address
  ... sent from ethernet address ..."); a reply only changes an entry that
  exists, and an entry that a request made, not yet used, is the first to be
  replaced when the table is full.  *IP*: a source that is 0, a group, all
  ones, a class E, loopback, a broadcast address of an interface or one of
  our own addresses is dropped; so is a packet with a source route or with
  an option that is not well formed (RFC 7126); the identification is a
  counter per destination that starts at random and grows by 1 to 4;
  reassembly (8 datagrams, 30 s) holds a datagram up to 65535 bytes in a
  buffer that is large enough (it used to be written 40 bytes past the end
  by a last fragment at the end of the largest datagram), drops a
  datagram whose fragments overlap with different bytes or end in two
  places, and makes room for a new datagram by dropping the oldest.
  *ICMP*: an echo request to a broadcast address is not answered
  (`net.inet.icmp.bmcastecho`, 0); errors are limited to 100 a second
  (`net.inet.icmp.errppslimit`, -1 for no limit); an "unreachable" is
  believed only when it is about a packet from one of our addresses, and
  by TCP only when it names a sequence number that is in flight (RFC 5927).
  *DHCP*: an address a host cannot have is not taken.  *Ports*: the
  ephemeral ports are chosen at random (the next free one in turn only
  when 64 random tries all met a port in use).  *TCP* (`netinet/tcp.c`):
  initial sequence numbers are a SipHash-2-4 of the four addresses and ports
  under a random key plus a 4 microsecond clock (RFC 6528); a RST is
  accepted only at the next sequence number, and a RST or SYN elsewhere in
  the window, an ACK for what was not sent and one more than a window old
  get a challenge ACK, at most 100 a second (RFC 5961); the window of a
  segment counts only if the segment is newer than the last that set it
  (`snd_wl1`/`snd_wl2`); an MSS under 216 is taken as 216 (an MSS of 0 used
  to make the transmit loop spin for ever); a SYN with FIN or RST, and
  port 0, are ignored; the send buffer is allocated when the connection is
  established; a listener with 16 handshakes in progress answers further
  SYNs with SYN cookies (the sequence number of the SYN-ACK holds a clock,
  the MSS as an index into 6 sizes and 24 bits of SipHash of the connection;
  the ACK that returns it makes the connection) so that a flood of
  forged SYNs cannot fill it; an ACK that finds the accept queue full is
  ignored, not reset; waiting segments are limited to 32 per connection
  and counted with their overhead; the RSTs that answer segments to
  closed ports are limited (`net.inet.tcp.rstppslimit`, 100 a second);
  the connection of a closed socket gives up after 8 retransmissions, a
  zero window included, and one in FIN_WAIT_2 after 60 s; `connect()` to a
  group or broadcast address fails, and replaces a TIME_WAIT connection of
  the same four.  Not done: window scaling and timestamps (so no PAWS),
  ICMP redirects and source quench are ignored altogether, no strict
  reverse-path filter.  `regress/sys/netinet/run_hard.sh` tests it all with
  a second network card whose other end is `nethard.c` on the host, a
  program that sends the forged frames, fragments, floods and resets and
  checks what the guest does.
* **Typed memory** (`kern/kern_tym.c`). Pools reserved at boot; mappings
  are shared memory objects over the pool's pages (with a release hook
  that returns allocated pages and unpins mapped ones); allocation by
  page or as one contiguous run; `posix_mem_offset` reads the physical
  offset from the mapping.
* **Terminals.** Linux's termios and line discipline (canonical editing,
  echo, `VMIN`/`VTIME`, `ISIG`, `IXON`), controlling terminals and
  sessions, the console (serial + VGA + keyboard) and pseudo-terminals
  (`/dev/ptmx`, `/dev/pts/N`).
* **No inline assembly** anywhere and only C17 (plus the attributes c17
  understands), so that c17 can compile the kernel.

## System calls

There are two system call ABIs, as in OpenBSD's compat layers.  A process
has an emulation (`struct emul`, `p_emul`): the table of its call numbers,
the number of its sigreturn, and what `uname` says.  `exec` chooses it from
the program: one that carries the V86BSD brand (a PT_NOTE with the note
`V86BSD`, type 1, that the C library's start files `crt1.o`, `Scrt1.o` and
`rcrt1.o` have in the native build) runs by the **native** numbers; any
other runs as a **Linux** program, by Linux's numbers (a kernel without
`option COMPAT_LINUX` refuses it, `ENOEXEC`).  Fork and clone keep the
emulation, exec sets it.

* **Native:** `kern/syscalls.master`, with numbers of V86BSD's own (the first
  six are OpenBSD's: exit, fork, read, write, open, close; no number equals
  the Linux number of the same call; a number is never reused, new calls go
  at the end).  `kern/makesyscalls.sh` writes `sys/syscall.h` (`SYS_name`),
  `sys/syscallargs.h` and `kern/init_sysent.c` from it.  The arguments and
  structures are still Linux's (a first native ABI changes the numbers and
  the identity only), and `reboot(2)` takes OpenBSD's `howto`
  (`<sys/reboot.h>`).  Native only: `revoke(2)` (number 224, `<unistd.h>` with the extensions
  only): the device the path names, which must be a character or block
  device owned by the caller or the caller is root (else EPERM, EINVAL for
  other files), is taken from every process that has it open.  Those open
  files stay in their descriptors as dead files: read is end-of-file on a
  terminal and EIO otherwise, write EIO, ioctl EBADF, poll POLLHUP, close
  works; sleepers in a terminal's read or write are woken, the terminal's
  session gets SIGHUP and loses it, and a later open is live.  Mapped memory
  and a descriptor in a socket's queue are not revoked.  The match is the
  device number, so `tty00` and `cua00` are one.  `uname` says what `sys/conf/newvers.sh` writes into
  `vers.c`: `V86BSD`, release `1.0`, version `GENERIC#1`, machine `amd64`.
* **Linux:** `compat/linux/syscalls.master`, the 224 of Linux's x86-64
  system calls the kernel has, among them every one the C library uses
  (generated into `linux_syscall.h`, `linux_syscallargs.h`, `linux_sysent.c`).
  The calls the two share are the same functions; `uname` (`Linux`, release
  `6.9.0`, `x86_64`), `personality` and `reboot` are Linux's own
  (`compat/linux/linux_misc.c`).  The emulation does not add calls: it is the
  table the kernel always had.  Absent calls answer `ENOSYS`, as Linux does
  for those it lacks.

The generated files are in the tree; `sh kern/makesyscalls.sh .` (from
`sys/`) or `make proto` in a compile directory writes them again.

The kernel prints its banner first, as OpenBSD's does:

    Virtual/86 BSD 1.0 (GENERIC) #1: Tue Nov 10 00:00:00 UTC 2026
        user@host:/usr/src/sys/arch/amd64/compile/GENERIC

`user@host` is the machine that built the kernel (`BUILDER=user@host`
names another); the date is `sys/conf/RELDATE`, never the clock, so two
builds differ only in that name.  The kernel is installed as `/bsd`.

## Testing

`regress/sys/kern/k_*.c` are small programs for particular areas; `regress/sys/kern/libc` gets
the C library's `s_*` tests (from `regress/lib/libc`).  `make testprogs`
builds both, and `distrib/mkdisk-ext2.sh` then puts them in `/root/ktest`
(`sh /root/ktest/libc/run.sh` inside the system runs the library's).
`k_timer` (POSIX timers: one signal queued per timer, overruns counted
while it is pending, also behind a signal already pending from `kill`;
a relative sleep on `CLOCK_REALTIME` across `clock_settime`),
`k_maxmap` (the limit of 65530 mappings, reached in well under a second),
`k_clofork` (close-on-fork), `k_mmap` (shared file mappings: coherence,
`msync`, `SIGBUS`, truncation; run with `-k` it leaves its file for a
look from outside), `k_inet` (TCP, UDP, raw sockets, IPv4 and IPv6 on
the loopback) and `k_net` (the network card) cover what was added after
the first version; the C library's `s_sporadic` and `s_typedmem` the
sporadic server and typed memory.  `k_net` needs QEMU's user networking
and a helper on the host:

    cc -o nethost regress/sys/netinet/nethost.c
    ./nethost 41000 41001 5577 result &
    qemu-system-x86_64 ... -nic user,model=e1000,hostfwd=tcp:127.0.0.1:5577-:7777
    (inside) /root/ktest/k_net 41000 41001

It sends UDP (also fragmented both ways) and 2 MiB of TCP each way to
echo servers on the host, has a non-blocking and a blocking connect
refused, pings the gateway, and serves a connection the host makes
through the forwarded port.  With `netloss=13` on the command line (every
13th frame lost, each way) it still passes, in about 30 s.  The utility suite: inside the system,
`cd /usr/src/utilities; CC=c17 sh run_tests.sh </dev/null` from a
process without a controlling terminal (several tests check behaviour
without one: `xargs -p`, `test -t`, `tput cols`), for example from
`/etc/rc`.

## Not yet

* IPv6 on the wire (neighbour discovery, ICMPv6, SLAAC); TCP window
  scaling and SACK.  SMP, preemption, swap.
* Disks with the waiting task sleeping (now both drivers wait for the
  transfer: the virtio one by watching memory, the ATA one by polling the
  drive's ports, and the task doing disk I/O keeps the processor until
  its transfer is done; the file systems count on that), and more than one
  request at a time.
* A page cache for all file I/O (now only for mapped files; reading and
  writing otherwise use the buffer cache).
* Making the disk image inside the system (`mkdisk.sh` uses the host's
  `mke2fs -d`).
* Priority-aware wake-ups beyond futexes (which wake the highest
  priority first, the longest waiting among equals) and FIFO/RR selection;
  per-thread `ITIMER_VIRTUAL`/`ITIMER_PROF`.
