#!/bin/sh
# test_rc.sh - tests for /etc/rc (V86BSD).  Run as root, inside V86BSD:
#   sh test_rc.sh        (RC=/path to test another copy of the script)
# It runs the real script with prepared /etc/myname, /etc/fstab,
# /etc/sysctl.conf and /etc/rc.conf.local, and puts everything back.
cd "$(dirname "$0")" || exit 2
pass=0 fail=0
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
RC=${RC:-/etc/rc}
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "rc: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
TD=/var/tmp/test_rc.$$   # not in /tmp: rc empties that
mkdir -m 700 "$TD" || exit 2
mkdir "$TD/save" "$TD/m"
files="/etc/myname /etc/fstab /etc/sysctl.conf /etc/rc.conf.local /etc/rc.local /etc/nologin"
for f in $files; do [ -e "$f" ] && cp -p "$f" "$TD/save/$(basename "$f")"; done
oldhost=$(hostname); olddom=$(sysctl -n kern.domainname)
cleanup() {
    umount "$TD/m" 2>/dev/null
    for f in $files; do
        if [ -e "$TD/save/$(basename "$f")" ]; then cp -p "$TD/save/$(basename "$f")" "$f"; else rm -f "$f"; fi
    done
    hostname "$oldhost"; sysctl -q kern.domainname="$olddom"
    mount -uw /
    rm -rf "$TD"
}
trap cleanup EXIT
# what a boot starts from
setup() {
    printf '# the name of this host\n\nrctest.example   # a comment\n' > /etc/myname
    printf '# fstab\ntmpfs %s/m tmpfs rw 0 0\n/dev/disk0s1 / ffs rw 1 0\n' "$TD" > /etc/fstab
    printf '# sysctl.conf\nkern.domainname=rc.example\n' > /etc/sysctl.conf
    rm -f /etc/rc.conf.local /etc/rc.local
    echo "no logins" > /etc/nologin
    echo junk > /tmp/rc_junk; mkdir -p /tmp/rc_dir/sub; echo x > /tmp/rc_dir/sub/f
    echo junk > /var/run/rc_junk; mkdir -p /var/run/rc_dir
    umount "$TD/m" 2>/dev/null
}
runrc() { out=$(sh "$RC" 2>&1); st=$?; }

# a boot from a read-only / (the kernel may be told so): rc makes it writable
setup
mount -ur /
runrc
t "rc succeeds" "0" 0 echo $st
t "the host name is /etc/myname's first word" "rctest.example" 0 hostname
ok "/ is writable again" sh -c '! mount | grep " on / " | grep read-only > /dev/null'
ok "the fstab was mounted" sh -c 'mount | grep " on $1/m type tmpfs" > /dev/null' sh "$TD"
t "sysctl.conf was applied" "rc.example" 0 sysctl -n kern.domainname
t "/tmp is empty" "" 0 sh -c 'ls -A /tmp'
ok "it said so" sh -c 'echo "$1" | grep "^clearing /tmp$" > /dev/null' sh "$out"
t "/etc/nologin is gone" "gone" 0 sh -c '[ ! -e /etc/nologin ] && echo gone'
t "/var/run has what the boot makes: the kernel messages, syslogd's pid, utmp" "dmesg.boot syslog.pid utmp" 0 sh -c 'ls -A /var/run | tr "\n" " " | sed "s/ \$//"'
ok "dmesg.boot is the kernel's messages" sh -c 'grep "^root on " /var/run/dmesg.boot > /dev/null'
ok "utmp is empty and mode 644" sh -c '[ ! -s /var/run/utmp ] && [ "$(ls -l /var/run/utmp | cut -c1-10)" = "-rw-r--r--" ]'
ok "the daemons are on by default, syslogd first" sh -c 'echo "$1" | grep "^starting daemons: syslogd cron\.$" > /dev/null' sh "$out"
ok "crond is running" sh -c 'ps -A -o comm= | grep "^crond$" > /dev/null'
ok "syslogd is running" sh -c 'ps -A -o comm= | grep "^syslogd$" > /dev/null'
ok "its pid file names it" sh -c 'kill -0 "$(cat /var/run/syslog.pid)"'
ok "/dev/log is there" sh -c '[ -S /dev/log ]'
logger -p local0.notice -t rctest "message from the rc test"
i=0; while [ $i -lt 10 ] && ! grep "rctest: message from the rc test" /var/log/messages > /dev/null 2>&1; do sleep 1; i=$((i+1)); done
ok "a message reached /var/log/messages" sh -c 'grep " rctest: message from the rc test$" /var/log/messages > /dev/null'
ok "so did the kernel's (the start of the boot)" sh -c 'grep " /bsd: root on " /var/log/messages > /dev/null'
ok "and syslogd's own start" sh -c 'grep " syslogd: start$" /var/log/messages > /dev/null'
# stop what it started
stopd() { for p in $(ps -A -o pid= -o comm= | awk '$2 == "crond" || $2 == "atd" || $2 == "syslogd" { print $1 }'); do kill $p 2>/dev/null; done; sleep 1; }
stopd

# fsck: rc runs `fsck -p' first (the real one ran above, on the fstab with
# only passes of 0), and what it says decides: 4 the root was changed
# (reboot), 8 and the rest a failed check; both leave rc before /tmp is cleared
mkdir "$TD/fake"
printf '#!/bin/sh\necho "$@" >> %s/fsck.args\nexit $(cat %s/fsck.status)\n' "$TD" "$TD" > "$TD/fake/fsck"
printf '#!/bin/sh\necho reboot >> %s/reboot.log\n' "$TD" > "$TD/fake/reboot"
chmod +x "$TD/fake/fsck" "$TD/fake/reboot"
# (rc sets PATH itself: the copy of rc has the fake programs first in it)
sed "s|^PATH=|PATH=$TD/fake:|" "$RC" > "$TD/rc.fake"
runfake() { echo "$1" > "$TD/fsck.status"; rm -f "$TD/fsck.args" "$TD/reboot.log"; setup; out=$(sh "$TD/rc.fake" 2>&1); st=$?; }
runfake 8
t "fsck 8: rc fails" "1" 0 echo $st
ok "  and says to run fsck" sh -c 'echo "$1" | grep "^Automatic file system check failed: run fsck manually\.$" > /dev/null' sh "$out"
t "  fsck was run with -p" "-p" 0 cat "$TD/fsck.args"
ok "  nothing went on: /tmp is not cleared" sh -c '! echo "$1" | grep "clearing" > /dev/null' sh "$out"
ok "  and it did not reboot" sh -c '[ ! -e "$1/reboot.log" ]' sh "$TD"
runfake 4
t "fsck 4: rc fails (after the reboot)" "1" 0 echo $st
ok "  it said the root was changed" sh -c 'echo "$1" | grep "^The root file system was changed: rebooting\.$" > /dev/null' sh "$out"
t "  and rebooted" "reboot" 0 cat "$TD/reboot.log"
runfake 2
t "fsck 2: rc fails" "1" 0 echo $st
ok "  an unknown failure" sh -c 'echo "$1" | grep "^fsck failed in an unknown way: run fsck manually\.$" > /dev/null' sh "$out"
rm -f "$TD/fsck.status"

# again: the mount is there already (nothing is mounted twice), rc.conf.local
# changes the daemons and keeps /tmp, rc.local runs
setup
umount "$TD/m" 2>/dev/null
mount -t tmpfs tmpfs "$TD/m"
printf 'syslogd_flags=NO\ncron_flags=NO\natd_flags=\nclear_tmp=NO\n' > /etc/rc.conf.local
printf 'echo LOCAL RAN > %s/flag\n' "$TD" > /etc/rc.local
runrc
t "rc succeeds again" "0" 0 echo $st
t "one mount of the fstab entry" "1" 0 sh -c 'mount | grep -c " on $1/m type tmpfs" || true' sh "$TD"
t "clear_tmp=NO: /tmp is left" "rc_dir rc_junk" 0 sh -c 'ls -A /tmp | tr "\n" " " | sed "s/ \$//"'
ok "...and nothing was said about it" sh -c '! echo "$1" | grep "clearing" > /dev/null' sh "$out"
ok "atd started, cron not" sh -c 'echo "$1" | grep "^starting daemons: atd\.$" > /dev/null' sh "$out"
ok "atd is running" sh -c 'ps -A -o comm= | grep "^atd$" > /dev/null'
ok "crond is not" sh -c '! ps -A -o comm= | grep "^crond$" > /dev/null'
ok "nor is syslogd" sh -c '! ps -A -o comm= | grep "^syslogd$" > /dev/null'
t "rc.local ran" "LOCAL RAN" 0 cat "$TD/flag"
ok "and was announced" sh -c 'echo "$1" | grep "^starting local daemons:$" > /dev/null' sh "$out"
stopd
rm -f /tmp/rc_junk; rm -rf /tmp/rc_dir

# no /etc/myname: the host name stays; no sysctl.conf: no complaint
setup
hostname keepme
rm -f /etc/myname /etc/sysctl.conf /etc/rc.conf.local /etc/rc.local
printf 'syslogd_flags=NO\ncron_flags=NO\n' > /etc/rc.conf.local
runrc
t "rc succeeds without them" "0" 0 echo $st
t "the host name is as it was" "keepme" 0 hostname
t "no daemons: no line" "" 0 sh -c 'echo "$1" | grep "starting daemons"; true' sh "$out"
rm -f /etc/rc.conf.local

# pkg_scripts: a script of /etc/rc.d that rc.conf lists starts after the base
# ones (whatever its _flags), and rc.shutdown stops it; a script that is not
# there is told, one that fails too
setup
cat > /etc/rc.d/rctest <<'EOT'
#!/bin/sh
daemon="/bin/sleep"
daemon_flags="4321"
rc_bg=YES
. /etc/rc.d/rc.subr
rc_cmd $1
EOT
cat > /etc/rc.d/rcfail <<'EOT'
#!/bin/sh
daemon="/nonexistent/prog"
. /etc/rc.d/rc.subr
rc_cmd $1
EOT
printf 'syslogd_flags=NO\ncron_flags=NO\npkg_scripts="rctest rcfail rcgone"\n' > /etc/rc.conf.local
runrc
t "rc succeeds with failing scripts" "0" 0 echo $st
ok "the scripts are announced, the ones that did not start told" sh -c 'echo "$1" | grep "^starting daemons: rctest rcfail(failed) rcgone(absent)\.$" > /dev/null' sh "$out"
ok "the script's daemon runs" sh -c 'ps -A -o args= | grep "^/bin/sleep 4321$" > /dev/null'
out=$(sh /etc/rc.shutdown 2>&1); st=$?
t "rc.shutdown succeeds" "0" 0 echo $st
ok "and stopped it" sh -c '! ps -A -o args= | grep "^/bin/sleep 4321$" > /dev/null'
rm -f /etc/rc.d/rctest /etc/rc.d/rcfail /etc/rc.conf.local
done_tests rc
