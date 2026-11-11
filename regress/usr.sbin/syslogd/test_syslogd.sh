#!/bin/sh
# test_syslogd.sh - tests for syslogd (V86BSD).  Run: sh test_syslogd.sh
# Every daemon here has a socket, kernel log (a FIFO), configuration and pid
# file of its own, in a directory under /tmp: nothing of the running system
# is touched.  Messages are sent by logger -u and by dgram (regress/lib/
# testlib) and read back from the files the configuration names.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.sbin/syslogd}; export PROG_DIR
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
S=${S:-$PROG_DIR/syslogd}
LOGGER=${LOGGER:-logger}
T=${TMPDIR:-/tmp}/test_syslogd.$$
mkdir -m 755 "$T" || exit 2
pid=
cleanup() {
    [ -s "$T/pid" ] && kill "$(cat "$T/pid")" 2>/dev/null
    [ -n "$bgpid" ] && kill "$bgpid" 2>/dev/null
    exec 3>&-
    rm -rf "$T"
}
bgpid=
trap cleanup EXIT
usage="usage: syslogd [-d] [-f config_file] [-k klog_device] [-m mark_interval]"
t "a bad option" "$usage" 0 sh -c '"$1" -x 2>&1 | grep "^usage"' sh "$S"
t "an operand" "$usage" 1 sh -c 'o=$("$1" -p "$2/s" extra 2>&1); r=$?; echo "$o" | grep "^usage"; exit $r' sh "$S" "$T"
t "a bad mark interval" "syslogd: bad mark interval: x" 1 sh -c '"$1" -m x 2>&1' sh "$S"

cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T/dgram" ../../lib/testlib/dgram.c || { echo "syslogd: cannot build dgram"; exit 2; }

H=$(hostname | sed 's/\..*//')
sd() { "$S" -p "$T/log" -k "$T/klog" -f "$T/conf" -P "$T/pid" "$@"; }
# the lines of a file without their time stamp and host name
strip() { sed -e "s/^[A-Z][a-z][a-z] [ 0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] $H //" "$1"; }
has() { strip "$1" | grep -F -x -- "$2"; }
allsel() { strip "$T/all" | grep -E '^(t1|login):'; }
longlen() { strip "$T/all" | grep -E '^x+$' | awk '{ print length($0) }'; }
fromtwice() { strip "$T/rep" | sed -n '/^r: twice/,$p'; }
stamped() { grep -E "^[A-Z][a-z][a-z] [ 0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] $H prog\[7\]: old time$" "$T/all" | grep -v '^Jan  1 00:00:00'; }
stamped_ok() { stamped > /dev/null; }
waitfor() {   # file text: until it has been logged (15 seconds at most)
    i=0
    while [ $i -lt 15 ]; do
        grep -F -q -- "$2" "$1" 2>/dev/null && return 0
        sleep 1; i=$((i + 1))
    done
    return 1
}
n=0
sync_() {     # everything sent before this is in the files when it returns
    n=$((n + 1))
    "$LOGGER" -u "$T/log" -p user.debug -t sync "s$n"
    waitfor "$T/all" "sync: s$n"
}
waitgone() { i=0; while [ -e "$1" ] && [ $i -lt 15 ]; do sleep 1; i=$((i + 1)); done; [ ! -e "$1" ]; }
mode() { ls -l "$1" | cut -c1-10; }

mkfifo "$T/klog"
exec 3<>"$T/klog"
cat > "$T/conf" <<EOF
# a configuration
*.debug				$T/all
*.notice;auth,authpriv.none	$T/messages
auth.info			$T/authlog
kern.debug			$T/kern
local0.=info			$T/exact
local1.err;local1.!=emerg;local1.!=alert	$T/errs
local2.debug;local2.!warning	$T/notwarn
local3.debug			$T/rep
mail.none			$T/never
*.alert;local5.none		$T/newfile
EOF
sd 2> "$T/err"; st=$?
t "it starts" "0" 0 echo $st
t "it said nothing" "" 0 cat "$T/err"
ok "the pid file names it" sh -c 'kill -0 "$(cat "$1/pid")"' sh "$T"
t "the socket is for everyone" "srw-rw-rw-" 0 mode "$T/log"
ok "start is logged" waitfor "$T/all" "syslogd: start"
t "start, with the host" "syslogd: start" 0 has "$T/all" "syslogd: start"
t "a second daemon is refused" "syslogd: already running" 1 sh -c '"$1" -p "$2/log2" -k "$2/klog" -f "$2/conf" -P "$2/pid" 2>&1' sh "$S" "$T"
t "its files are 0640" "-rw-r-----" 0 mode "$T/newfile"

# priorities
"$LOGGER" -u "$T/log" -p user.notice -t t1 "un"
"$LOGGER" -u "$T/log" -p user.info -t t1 "ui"
"$LOGGER" -u "$T/log" -p auth.notice -t login "bad"
"$LOGGER" -u "$T/log" -p auth.debug -t login "dbg"
"$LOGGER" -u "$T/log" -p local0.info -t l0 "info"
"$LOGGER" -u "$T/log" -p local0.notice -t l0 "notice"
"$LOGGER" -u "$T/log" -p local0.err -t l0 "err"
"$LOGGER" -u "$T/log" -p local1.crit -t l1 "crit"
"$LOGGER" -u "$T/log" -p local1.emerg -t l1 "emerg"
"$LOGGER" -u "$T/log" -p local1.alert -t l1 "alert"
"$LOGGER" -u "$T/log" -p local1.warning -t l1 "warning"
"$LOGGER" -u "$T/log" -p local2.notice -t l2 "notice"
"$LOGGER" -u "$T/log" -p local2.warning -t l2 "warning"
"$LOGGER" -u "$T/log" -p local2.debug -t l2 "debug"
"$LOGGER" -u "$T/log" -p mail.crit -t mail "x"
"$LOGGER" -u "$T/log" -p local5.alert -t l5 "none"
"$LOGGER" -u "$T/log" -p local6.alert -t l6 "alert"
sync_
t "all: everything" "t1: un
t1: ui
login: bad
login: dbg" 0 allsel
t "messages: user.notice" "t1: un" 0 has "$T/messages" "t1: un"
t "messages: not user.info" "" 1 has "$T/messages" "t1: ui"
t "messages: not auth" "" 1 has "$T/messages" "login: bad"
t "authlog: auth.notice" "login: bad" 0 has "$T/authlog" "login: bad"
t "authlog: not auth.debug" "" 1 has "$T/authlog" "login: dbg"
t "=info: that level" "l0: info" 0 has "$T/exact" "l0: info"
t "=info: not a higher one" "" 1 has "$T/exact" "l0: notice"
t "=info: not a lower one" "" 1 has "$T/exact" "l0: err"
t "!=: crit stays" "l1: crit" 0 has "$T/errs" "l1: crit"
t "!=: emerg is taken out" "" 1 has "$T/errs" "l1: emerg"
t "!=: alert is taken out" "" 1 has "$T/errs" "l1: alert"
t "err: not warning" "" 1 has "$T/errs" "l1: warning"
t "!warning: notice stays" "l2: notice" 0 has "$T/notwarn" "l2: notice"
t "!warning: debug stays" "l2: debug" 0 has "$T/notwarn" "l2: debug"
t "!warning: warning goes" "" 1 has "$T/notwarn" "l2: warning"
t "mail.none: nothing" "0" 0 sh -c 'wc -l < "$1" | tr -d " "' sh "$T/never"
t "none after *: local5 not in newfile" "" 1 has "$T/newfile" "l5: none"
t "*.alert: local6" "l6: alert" 0 has "$T/newfile" "l6: alert"

# the time stamp and the host are the daemon's, the text is the sender's
"$T/dgram" "$T/log" "<14>Jan  1 00:00:00 prog[7]: old time"
"$T/dgram" "$T/log" "<14>prog: no stamp"
"$T/dgram" "$T/log" "no priority at all"
"$T/dgram" "$T/log" "<999>prog: bad priority"
"$T/dgram" "$T/log" "<14>prog: with a newline
"
"$T/dgram" -n "$T/log" "<14>prog: with a NUL"
sync_
t "a time stamp sent is not used" "prog[7]: old time" 0 has "$T/all" "prog[7]: old time"
ok "the line has the time and host of the daemon" stamped_ok
t "no stamp" "prog: no stamp" 0 has "$T/all" "prog: no stamp"
t "no priority: user.notice (messages)" "no priority at all" 0 has "$T/messages" "no priority at all"
t "a bad priority is user.notice" "<999>prog: bad priority" 0 has "$T/messages" "<999>prog: bad priority"
t "a trailing newline is dropped" "prog: with a newline" 0 has "$T/all" "prog: with a newline"
t "a trailing NUL is dropped" "prog: with a NUL" 0 has "$T/all" "prog: with a NUL"

# a message cannot act on the terminal or fake a line
"$T/dgram" "$T/log" "$(printf '<14>c: a\033[31mb\001c\177d')"
"$T/dgram" "$T/log" "<14>c: l1
l2 <14>c: fake"
x=$(awk 'BEGIN { for (i = 0; i < 3000; i++) printf "x" }')
"$T/dgram" "$T/log" "<14>$x"
sync_
t "control characters shown as ^X" "c: a^[[31mb^Ac^?d" 0 has "$T/all" 'c: a^[[31mb^Ac^?d'
t "a newline inside is one line" "c: l1^Jl2 <14>c: fake" 0 has "$T/all" 'c: l1^Jl2 <14>c: fake'
t "a long message is cut at 1024" "1024" 0 longlen

# repeated lines
for i in 1 2 3 4 5; do "$LOGGER" -u "$T/log" -p local3.info -t r "same"; done
"$LOGGER" -u "$T/log" -p local3.info -t r "other"
sync_
t "five of a line: one, and the count" "r: same
last message repeated 4 times
r: other" 0 strip "$T/rep"
"$LOGGER" -u "$T/log" -p local3.info -t r "twice"
"$LOGGER" -u "$T/log" -p local3.info -t r "twice"
"$LOGGER" -u "$T/log" -p local3.info -t r "end"
sync_
t "two of a line: once" "r: twice
last message repeated 1 time
r: end" 0 fromtwice

# the kernel: /dev/klog
printf 'hello kernel\n<4>warn message\n<7>debug message\n<6>part' >&3
printf 'ial\nesc \033[2J done\n' >&3
sync_
t "klog: a line is kern.crit /bsd:" "/bsd: hello kernel" 0 has "$T/kern" "/bsd: hello kernel"
t "klog: it is in messages" "/bsd: hello kernel" 0 has "$T/messages" "/bsd: hello kernel"
t "klog: <4> is kern.warning" "/bsd: warn message" 0 has "$T/messages" "/bsd: warn message"
t "klog: <7> is debug: not in messages" "" 1 has "$T/messages" "/bsd: debug message"
t "klog: <7> is in kern" "/bsd: debug message" 0 has "$T/kern" "/bsd: debug message"
t "klog: a line in two reads is one" "/bsd: partial" 0 has "$T/kern" "/bsd: partial"
t "klog: escapes are shown" "/bsd: esc ^[[2J done" 0 has "$T/kern" '/bsd: esc ^[[2J done'

# SIGHUP: the configuration again, the files opened again
mv "$T/all" "$T/all.0"
cat > "$T/conf" <<EOF
*.debug				$T/all
local4.debug			$T/new
EOF
kill -HUP "$(cat "$T/pid")"
ok "restarted" waitfor "$T/all" "syslogd: restarted"
"$LOGGER" -u "$T/log" -p local4.info -t l4 "after hup"
"$LOGGER" -u "$T/log" -p local3.info -t r "not in rep now"
sync_
t "a new file after SIGHUP" "l4: after hup" 0 has "$T/new" "l4: after hup"
t "the old file got 'restart'" "syslogd: restart" 0 has "$T/all.0" "syslogd: restart"
t "the new file did not get it" "" 1 has "$T/all" "syslogd: restart"
t "a removed entry no longer gets lines" "" 1 has "$T/rep" "r: not in rep now"

# the end
pid=$(cat "$T/pid")
kill -TERM "$pid"
ok "SIGTERM: the pid file is gone" waitgone "$T/pid"
ok "SIGTERM: the socket is gone" waitgone "$T/log"
t "SIGTERM: it was logged" "syslogd: exiting on signal 15" 0 has "$T/all" "syslogd: exiting on signal 15"
ok "SIGTERM: the process ended" sh -c '! kill -0 "$1" 2>/dev/null' sh "$pid"

# a configuration with errors: what is wrong is told, the rest is used
cat > "$T/conf" <<EOF
# errors
nosuch.err	/x
user.nosuchlevel	/x
user.err
mail.err	@loghost
!ntpd
user.err	|cmd
*.err	$T/good
EOF
sd 2> "$T/err"; st=$?
t "errors: it starts" "0" 0 echo $st
t "errors: each one is told, with its line" "syslogd: $T/conf: line 2: unknown facility name \"nosuch\"
syslogd: $T/conf: line 3: unknown priority name \"nosuchlevel\"
syslogd: $T/conf: line 4: no action
syslogd: $T/conf: line 5: forwarding, pipes and unsynced files are not supported
syslogd: $T/conf: line 6: program and host blocks are not supported
syslogd: $T/conf: line 7: forwarding, pipes and unsynced files are not supported" 0 cat "$T/err"
"$LOGGER" -u "$T/log" -p user.err -t e "to good"
ok "errors: the good line works" waitfor "$T/good" "e: to good"
kill "$(cat "$T/pid")"; waitgone "$T/pid"

# a file that cannot be written is told, and does not stop the others
printf '*.err\t/nonexistent-dir/log\n*.err\t%s/good2\n' "$T" > "$T/conf"
sd 2> "$T/err"
t "unwritable: told" "syslogd: /nonexistent-dir/log: No such file or directory" 0 cat "$T/err"
"$LOGGER" -u "$T/log" -p user.err -t e "second"
ok "unwritable: the other file works" waitfor "$T/good2" "e: second"
kill "$(cat "$T/pid")"; waitgone "$T/pid"

# no configuration file: the default (no complaint)
rm -f "$T/conf"
sd 2> "$T/err"; st=$?
# (/dev/console may not be writable to whoever runs this)
t "no configuration: it starts" "0 " 0 sh -c 'echo "$1 $(grep -v "^syslogd: /dev/console:" "$2/err")"' sh $st "$T"
ok "no configuration: running" sh -c 'kill -0 "$(cat "$1/pid")"' sh "$T"
kill "$(cat "$T/pid")"; waitgone "$T/pid"

# -d: in the foreground, and the lines on the standard error
printf '*.debug\t%s/dall\n' "$T" > "$T/conf"
sd -d 2> "$T/dbg" &
bgpid=$!
ok "-d: the socket is made" waitfor "$T/dbg" "syslogd: start"
"$LOGGER" -u "$T/log" -p user.err -t d "foreground"
ok "-d: the line is on stderr" waitfor "$T/dbg" "d: foreground"
ok "-d: and in the file" waitfor "$T/dall" "d: foreground"
ok "-d: the daemon is the process started" kill -0 "$bgpid"
kill "$bgpid"; wait "$bgpid" 2>/dev/null; bgpid=
done_tests syslogd
