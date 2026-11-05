# The Shall kernel

A small x86-64 kernel that runs Shall's programs, and Linux programs, as
Linux would: the same system call numbers, structures, ELF loading, initial
stack, signal frames and `/proc` files.  It boots in QEMU in text mode,
mounts an ext2 root file system and starts `/sbin/init`.

First version (November 2026): one processor; networking over an Intel
e1000 card (QEMU's default) and the loopback interface: IPv4 with ARP,
ICMP, UDP and TCP on the wire, DHCP, and IPv4/IPv6 TCP, UDP and raw
sockets on the loopback; no graphics.  About 23 000 lines of C and 700 of
assembly.

## Status

Inside QEMU, on an ext2 disk made from a bootstrapped Shall root:

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
  them; `s_collsym` with locales `tests/libc/run.sh` makes there with
  Shall's own `localedef`); `s_math` meets QEMU's software x87, a few ulp
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
* The network: `k_net` passes through QEMU's user networking (UDP,
  fragmented both ways; 2 MiB of TCP each way; refused and non-blocking
  connects; ping; a server reached from the host), also with every 13th
  frame lost each way.
* The kernel is built by Shall's own c17 alone (it compiles the C,
  assembles `arch/*.s` and links with `kernel.ld`; no binutils), on the
  host by `bootstrap.sh` and inside the running system by `make` in
  `/usr/src/shall/kernel`, where it comes out byte for byte the same.
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
  them.  The
  library tests once whether `F_GETFD` reports `FD_CLOFORK` after an
  `open` with `O_CLOFORK` (`tests/k_clofork` checks that it then uses the
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

    arch/      boot.s (Multiboot entry, long mode), entry.s (traps, system
               calls, context switch, user copies), cpu.s (instructions C
               cannot express), x86.c (GDT/TSS/IDT, PIC, PIT, trap dispatch)
    kern/      main, scheduler, processes (clone/exit/wait/ids/limits),
               signals, exec, futexes, time and timers, System V IPC, POSIX
               message queues, uname/sysinfo/reboot/syslog, the system call
               table
    mm/        physical pages (buddy allocator with reference counts),
               kernel heap, address spaces and page faults, mmap & co.
    fs/        VFS (inode cache, mounts, path lookup), descriptors and
               I/O calls, name calls, buffer cache, page cache, ext2,
               tmpfs/devtmpfs, procfs, pipes/FIFOs, record locks, sockets
               (AF_UNIX; AF_INET/AF_INET6: addresses, ports, the loopback)
    net/       ip.c (Ethernet, ARP, IPv4 with fragments, ICMP, UDP, DHCP,
               the network thread), tcp.c (TCP)
    drivers/   16550 serial console, VGA text console, PS/2 keyboard, ATA
               disks (PIO), virtio disks, terminals (line discipline, console, ptys),
               memory devices, PCI, e1000, typed memory (tym.c)
    include/   the kernel's headers (its own types; no C library headers)
    user/      init (POSIX C, built with Shall's c17 and C library)
    tools/     mkimage (ELF to flat Multiboot image), mkdisk.sh (ext2
               image of a Shall root), gensyscalls.sh (prototypes),
               nethost.c (the host side of tests/k_net)
    tests/     small test programs (k_*.c) and the C library's tests, put
               on the disk image in /root/ktest

## Building and running

The kernel is part of Shall (`kernel/` in its sources): `bootstrap.sh`
builds it with the c17 of its stage 2, together with `/sbin/init`, into
`root/boot/shall-kernel` and `root/usr/sbin/init`.  To boot that root,
the host needs QEMU (`qemu-system-x86_64`) and e2fsprogs (`mke2fs -d`);
in the top directory:

    sh mkdisk.sh                  # shall.img: the root as ext2 (1 GiB)
    sh boot.sh                    # QEMU; the console is this terminal

(The head of `boot.sh` lists its options.)  Here, `make disk.img ROOT=../root`
makes `disk.img` with the test programs too, and `make run` boots it
with the kernel built here.  Inside the system everything
needed is there:

    cd /usr/src/shall/kernel
    make                          # shall-kernel: c17 compiles, assembles, links
    make init testprogs           # /sbin/init and the test programs

The Makefile's defaults are Shall's tools (`CC=c17`, `AS="c17 -c"`,
`LD=c17` with `LDFLAGS="-B standalone"`, `HOSTCC=c17` for mkimage);
programs for the system (`init`, the tests) are made with `C17="c17 -B
static"`.  On a host, the c17 of a bootstrap kept with `-k` serves
(`work/stage2/root/usr/bin`, statically linked, with
`C17_LIBDIR=work/stage2/root/usr/lib/c17 C17_LIBC=root/usr`), or `make
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

Kernel command line (`-append`): `root=/dev/vda` or `/dev/sda` (default:
the first disk),
`rootfstype=ext2`, `ro`, `init=/path`; `ip=dhcp` (default when there is a
card), `ip=off` or `ip=ADDR/PREFIX[,GATEWAY[,DNS]]`; `tym=NAME:SIZE,...`
(typed memory pools, `off` for none); `netloss=N` (testing: lose every
Nth frame each way).  As on Linux, other `name=value`
words become init's environment; init uses `CONSOLE` (default
`/dev/ttyS0`) as the terminal for its shell.

## How it boots

1. QEMU (or GRUB) loads the flat image at 1 MiB (Multiboot address
   kludge: the loader needs no ELF64 support).  `boot.s` maps the first GiB
   at 0, at the direct map 0xffff888000000000 and at 0xffffffff80000000,
   enables long mode and calls `kmain` in the higher half.
2. `kmain`: memory map, page allocator (the first GiB), direct map of all
   RAM, then the rest of the memory to the allocator; GDT/IDT,
   TSC calibration, scheduler, timer at 1000 Hz, devices, root mount.
3. Process 1 mounts devtmpfs on `/dev` and proc on `/proc` (Shall has no
   `mount` utility: mount is not POSIX), opens `/dev/console` as 0, 1, 2
   and executes `init=`, `/sbin/init`, `/etc/init`, `/bin/init` or
   `/bin/sh`.
4. `/sbin/init` (user/init.c) runs `/etc/rc` if present, then `sh` on
   the console as a session leader, so it gets a controlling terminal
   (as on Linux, `/dev/console` itself never becomes one).

## Design

* **Memory.** User space below 0x800000000000, Linux's layout: programs at
  0x400000 (ET_EXEC) or 0x555555554000 (PIE), mmap top-down from just below
  the stack, stack at 0x7ffffffff000.  Private mappings are copy on write;
  file pages are read at the first touch.  Anonymous shared memory, SysV
  segments, tmpfs files and the page cache share one "shared memory
  object" type.
* **Page cache** (`fs/pcache.c`). A `MAP_SHARED` mapping of an ext2 file
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
  these per thread; Shall's C library broadcasts set*id and nice to every
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
  allocator runs out (`Buffers` in `/proc/meminfo`).  ext2's state is
  "not clean" while mounted and "clean" again after a proper shutdown, so
  `e2fsck -p` (which `boot.sh` runs) checks a disk after QEMU was
  stopped.  Disks: virtio (`vda`, QEMU's `if=virtio`, what `boot.sh`
  uses: one request per block, waited for by watching the used ring in
  memory) and ATA by PIO (`sda`, the PC's IDE disk: polled with
  interrupts on, so the clock and the terminals keep going while the
  disk works; much slower, especially under KVM);
  tmpfs; devtmpfs; procfs (what `ps`, `fuser`, `df`, `ipcs` and the C
  library read); pipes; AF_UNIX sockets with `SCM_RIGHTS` and
  `SCM_CREDENTIALS`; POSIX record locks and OFD locks.
* **Internet sockets** (`fs/socket.c`). AF_INET and AF_INET6 over the one
  interface, `lo` (127.0.0.0/8, ::1; the wildcard addresses bind to it):
  TCP and UDP with the same connected pairs and queues as AF_UNIX, which
  on a loopback is what TCP and UDP deliver.  Ports (ephemeral 32768-60999,
  below 1024 for root), `EADDRINUSE` with Linux's `SO_REUSEADDR` rules,
  dual-stack AF_INET6 sockets (IPv4 peers as `::ffff:a.b.c.d`) unless
  `IPV6_V6ONLY`, connected UDP sockets that hear only their peer and get
  `ECONNREFUSED` when it is gone.  Raw sockets (root): a packet to a
  loopback address reaches the raw sockets of its protocol (IPv4 with its
  header); the kernel answers ICMP and ICMPv6 echo requests.  Other
  destinations: `ENETUNREACH`.  `SIOCGIF*` and `/proc/net/dev` show `lo`.
* **Network.** `drivers/pci.c` finds the card, `drivers/e1000.c` drives
  an Intel 8254x (registers memory-mapped through `ioremap`; 64 receive
  and 64 transmit descriptors).  The card's interrupt only wakes the
  network thread, which takes the frames and does all the protocol work
  (with the system calls; the kernel is not preemptive, so they never
  interleave).  `net/ip.c`: ARP (cache, queued packets, retries), IPv4
  (fragmentation out, reassembly in), ICMP echo and unreachable (to
  connected UDP sockets as ECONNREFUSED), UDP, a DHCP client;
  `/proc/net/pnp` gives the name server, and `/etc/resolv.conf` on the
  disk image is a link to it, as on Linux with `ip=dhcp`.  `net/tcp.c`:
  TCP (RFC 9293's states, MSS option, RTO from smoothed RTT with
  backoff, fast retransmit, slow start and congestion avoidance,
  zero-window probes, early segments kept for the gap, TIME_WAIT 10 s;
  no window scaling, SACK or urgent data).  Sockets to the network are
  the same sockets as to the loopback (`fs/socket.c`): a TCP socket to
  another host has a `struct tcb`, whose received data goes into the
  socket's queue.  IPv6 is loopback only so far.
* **Typed memory** (`drivers/tym.c`). Pools reserved at boot; mappings
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

224 of Linux's x86-64 system calls, among them every one Shall's C
library uses.
`AF_NETLINK` exists only as the channel of `mq_notify(SIGEV_THREAD)`.
Absent calls answer `ENOSYS`, as Linux does for those it lacks.

`uname` says `Linux` and release `6.9.0-shall`, so that programs that
check (glibc's start-up test, configure scripts) accept the system.

## Testing

`tests/k_*.c` are small programs for particular areas; `tests/libc` gets
the C library's `s_*` tests (from `../libc/tests`).  `make testprogs`
builds both, and `make disk.img` then puts them in `/root/ktest`
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

    cc -o nethost tools/nethost.c
    ./nethost 41000 41001 5577 result &
    qemu-system-x86_64 ... -nic user,model=e1000,hostfwd=tcp:127.0.0.1:5577-:7777
    (inside) /root/ktest/k_net 41000 41001

It sends UDP (also fragmented both ways) and 2 MiB of TCP each way to
echo servers on the host, has a non-blocking and a blocking connect
refused, pings the gateway, and serves a connection the host makes
through the forwarded port.  With `netloss=13` on the command line (every
13th frame lost, each way) it still passes, in about 30 s.  The utility suite: inside the system,
`cd /usr/src/shall/utilities; CC=c17 sh run_tests.sh </dev/null` from a
process without a controlling terminal (several tests check behaviour
without one: `xargs -p`, `test -t`, `tput cols`), for example from
`/etc/rc`.

## Not yet

* IPv6 on the wire (neighbour discovery, ICMPv6, SLAAC); TCP window
  scaling and SACK; more network cards (virtio-net).  SMP, preemption,
  swap.
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
