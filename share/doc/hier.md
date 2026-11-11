# hier - the file system hierarchy of Virtual/86 BSD

The layout follows OpenBSD's hier(7).  `make installworld distribution
installkernel DESTDIR=dir` (see the `Makefile`) makes it; `/bin` and `/sbin`
are real directories (not links into `/usr`).

    /bsd            the kernel
    /bin/           the essential utilities (the sources: bin/): linked
                    statically, they need nothing in /usr
    /sbin/          the system utilities (sbin/), and init, the first
                    process: linked statically
    /dev/           the devices (the kernel mounts devtmpfs here)
    /proc/          the kernel's process file system (it is always mounted)
    /etc/           configuration files (the sources: etc/): passwd, group,
                    profile, hosts, services, protocols, shells, motd
    /home/          the home directories of users
    /mnt/           a place to mount file systems for a while
    /root/          root's home directory (mode 700)
    /tmp/           temporary files (mode 1777)
    /usr/bin/       the other utilities (usr.bin/), among them the compiler
                    c17 and make, dynamically linked with /usr/lib/libc.so
    /usr/sbin/      daemons (crond, atd, uucico)
    /usr/lib/       the C library (libc.a; libc.so, which is also the dynamic
                    linker), c17's support library and profiles (c17/)
    /usr/libexec/   helper programs run by other programs (getty)
    /usr/include/   the C library's headers
    /usr/share/     architecture-independent data: time zones, terminfo,
                    locale sources and charmaps (copied from the host by
                    `make distribution`), the manual (posixutilities/)
    /usr/local/     what is added to the system: ports/ installs here
                    (bin, lib, include, share/man)
    /usr/src/       these sources, so that the system can rebuild itself
    /var/log/       log files
    /var/run/       run-time data (utmp, pid files)
    /var/db/        databases
    /var/tmp/       temporary files that stay across a boot (mode 1777)
    /var/spool/     queues (cron, at, mail)
    /var/cron/      crontabs

There is no `/boot` (the kernel is `/bsd`), no `/lib` and no `/sys`.

`PATH` is `/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin` (`/etc/profile`, and
what init gives its children).  The shell is `/bin/sh` (the
C library's `system()`, `popen()`, `wordexp()` and `execvp()` of a script run
it); `env` is `/usr/bin/env`.

Stages 1 and 2 of the build (distrib/build.sh) run on the host and keep
everything in `usr/bin`; the programs of `bin/` and `sbin/` are in `/bin` and
`/sbin` only in the world that stage 3 makes.
