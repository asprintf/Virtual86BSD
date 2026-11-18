# hier - the file system hierarchy of Virtual/86 BSD

The layout of the userland follows OpenBSD's hier(7); the kernel and the
boot loader are FreeBSD's and sit where FreeBSD puts them.
`distrib/boot-freebsd.sh` makes it in `obj/boot/root` and
`distrib/mkimage-freebsd.sh` puts it on the disk image; `/bin` and `/sbin`
are real directories (not links into `/usr`).

    /boot/          FreeBSD's loader and what it reads: kernel/kernel (the
                    kernel), loader, defaults/loader.conf, loader.conf,
                    device.hints, lua/, fonts/ (the console font)
    /bin/           the essential utilities (the sources: bin/), linked
                    dynamically with /lib/libc.so
    /sbin/          the system utilities (sbin/), among them FreeBSD's UFS
                    tools (newfs, fsck_ffs, tunefs, dumpfs, growfs), and init,
                    the first process: linked statically
    /dev/           the devices (the kernel mounts devfs here)
    /lib/libc.so    the C library as the dynamic linker (the programs'
                    interpreter; also /usr/lib/libc.so)
    /etc/           configuration files (the sources: etc/): passwd, group,
                    profile, hosts, services, protocols, shells, motd,
                    rc, rc.conf, netstart, hostname.*, mygate,
                    syslog.conf; rc.d/ holds one script per
                    daemon (rcctl(8) runs them)
    /home/          the home directories of users
    /mnt/           a place to mount file systems for a while
    /root/          root's home directory (mode 700)
    /tmp/           temporary files (mode 1777)
    /usr/bin/       the other utilities (usr.bin/), among them the compiler
                    c17 and make, dynamically linked with /usr/lib/libc.so
    /usr/sbin/      daemons and administration programs (syslogd, crond, atd,
                    rcctl, uucico)
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
    /var/log/       log files (syslogd: messages, authlog, daemon)
    /var/run/       run-time data (utmp, pid files, dmesg.boot)
    /var/db/        databases
    /var/tmp/       temporary files that stay across a boot (mode 1777)
    /var/spool/     queues (cron, at, mail)
    /var/cron/      crontabs

The kernel is `/boot/kernel/kernel`, not `/bsd`; there is no `/sys`, and
`/proc` is not mounted (the programs that used it, `ps` among them, ask the
kernel through `sysctl`; `fuser` and `ipcs` do not work yet).  The root is a
UFS2 file system on the partition labelled `rootfs` (`/dev/gpt/rootfs`);
the image also has the EFI system partition and the boot partition that hold
the boot blocks.

`PATH` is `/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin` (`/etc/profile`, and
what init gives its children).  The shell is `/bin/sh` (the
C library's `system()`, `popen()`, `wordexp()` and `execvp()` of a script run
it); `env` is `/usr/bin/env`.

The image `distrib/boot-freebsd.sh` makes has so far the programs,
`/lib/libc.so`, `/etc`, `/usr/share/posixutilities` and the loader's files in
`/boot`; the rest of the list (`/usr/include`, `/usr/lib` with `libc.a`,
`/usr/src`, the time zones, terminfo and locale data, `/usr/local`) is the
layout the system is meant to have once it has a compiler, and is not on the
image yet.

