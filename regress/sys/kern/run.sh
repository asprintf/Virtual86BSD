#!/bin/sh
# run.sh - the kernel's tests and the C library's self-checking tests, run on
# this system (meant for inside V86BSD, as root): builds them (make testprogs:
# k_* here, s_* in libc/), runs each k_* with 600 seconds, then libc/run.sh.
#
# Usage: sh run.sh
#
# A test passes if its status is 0 and it prints no FAIL line (k_sigwait,
# k_shm and k_fpu only print what they saw).  k_net needs a helper on the
# host (regress/sys/netinet/nethost.c, see sys/README.md) and QEMU's
# forwarded port 7777 (boot.sh -p 5577:7777): it is run only with
# K_NET="41000 41001" (its two ports) in the environment.
#
#   MAKE=...  the make to build with (default: make)
cd "$(dirname "$0")" || exit 2
MAKE=${MAKE:-make}
"$MAKE" testprogs > build.log 2>&1 || { echo "kernel tests: build failed"; sed 's/^/    /' build.log; exit 1; }
rm -f build.log
pass=0 fail=0
for t in k_*; do
    case $t in *.c|*.s|*.txt) continue ;; esac
    [ -x "$t" ] || continue
    if [ "$t" = k_net ]; then
        [ -n "$K_NET" ] || { echo "$t: skipped (K_NET not set)"; continue; }
        args=$K_NET
    else
        args=
    fi
    # /dev/klog has one read position for all: k_klog needs it to itself, so
    # syslogd, which reads it in a booted system, stops for the test
    resume=
    if [ "$t" = k_klog ] && [ -x /etc/rc.d/syslogd ] && /etc/rc.d/syslogd check > /dev/null 2>&1; then
        /etc/rc.d/syslogd stop > /dev/null 2>&1 && resume=yes
    fi
    timeout 600 ./$t $args > $t.txt 2>&1 < /dev/null; st=$?
    [ -z "$resume" ] || /etc/rc.d/syslogd start > /dev/null 2>&1
    if [ $st = 0 ] && ! grep -q FAIL $t.txt; then
        echo "$t: ok"; pass=$((pass + 1))
    else
        echo "$t: FAILED (status $st)"; grep FAIL $t.txt | sed -n '1,8p'; fail=$((fail + 1))
    fi
done
echo "kernel tests: $pass passed, $fail failed"
sh libc/run.sh
lst=$?
[ $fail = 0 ] && [ $lst = 0 ]
