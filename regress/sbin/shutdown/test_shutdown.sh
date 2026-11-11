#!/bin/sh
# test_shutdown.sh - tests for shutdown (V86BSD).  Run as root, inside V86BSD:
#   sh test_shutdown.sh
# A shutdown is never carried out here (it would stop the machine): each one is
# +60 minutes and cancelled.  The users' terminal is a pseudo-terminal that
# ptydrive reads, entered in utmpx by utmpadd.  The end is checked by a boot
# that runs `shutdown -p now'.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/shutdown}; export PROG_DIR
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
S=${S:-$PROG_DIR/shutdown}
usage="usage: shutdown [-hpr] time [warning-message ...]"
t "no arguments" "$usage" 1 sh -c '"$1" 2>&1' sh "$S"
t "a bad option" "$usage" 0 sh -c '"$1" -x now 2>&1 | grep "^usage"' sh "$S"
t "a time that is not one" "$usage" 1 sh -c '"$1" tomorrow 2>&1' sh "$S"
t "+ alone" "$usage" 1 sh -c '"$1" + 2>&1' sh "$S"
t "+ and letters" "$usage" 1 sh -c '"$1" +x 2>&1' sh "$S"
t "a negative delay" "$usage" 1 sh -c '"$1" +-5 2>&1' sh "$S"
t "a delay of more than a week" "$usage" 1 sh -c '"$1" +20000 2>&1' sh "$S"
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    t "not root" "shutdown: you must be root" 1 sh -c '"$1" now 2>&1' sh "$S"
    done_tests shutdown
    exit
fi
T=/var/tmp/test_shutdown.$$
mkdir -m 755 "$T" || exit 2
[ -e /etc/nologin ] && cp -p /etc/nologin "$T/nologin.save"
[ -e /var/run/utmp ] || { : > /var/run/utmp; chmod 644 /var/run/utmp; }
cp -p /var/run/utmp "$T/utmp.save"
cleanup() {
    rm -f /etc/nologin
    [ -e "$T/nologin.save" ] && cp -p "$T/nologin.save" /etc/nologin
    cp -p "$T/utmp.save" /var/run/utmp
    rm -rf "$T"
}
trap cleanup EXIT
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
for p in ptydrive utmpadd asuser; do
    $cc -o "$T/$p" ../../lib/testlib/$p.c || { echo "cannot compile $p.c"; exit 2; }
done
mv "$T/ptydrive" "$T/drive"
D="$T/drive"
has() { case $1 in *"$2"*) return 0;; esac; return 1; }
hasnt() { case $1 in *"$2"*) return 1;; esac; return 0; }
rm -f /etc/nologin
t "not root: refused" "shutdown: you must be root" 1 sh -c '"$1" 65534 65534 "$2" now 2>&1' sh "$T/asuser" "$S"
ok "...and no nologin was made" test ! -e /etc/nologin

# a shutdown in an hour, seen from a terminal that has a user on it, and cancelled
cat > "$T/wrap" <<'WRAP'
#!/bin/sh
t=$(tty); l=${t#/dev/}
"$1" "$l" tuser
"$2" "$3" +60 the "test message" > "$4/out"
cat "$4/out"
pid=$(sed 's/.*\[pid \([0-9]*\)\].*/\1/' "$4/out")
sleep 1
echo "NOLOGIN:"; cat /etc/nologin
echo "ALIVE=$(kill -s 0 "$pid" 2>/dev/null && echo yes || echo no)"
kill "$pid"
sleep 2
if [ -e /etc/nologin ]; then echo NOLOGIN-STILL; else echo NOLOGIN-GONE; fi
echo "ALIVE=$(kill -s 0 "$pid" 2>/dev/null && echo yes || echo no)"
WRAP
chmod 755 "$T/wrap"
o=$("$D" -T 40 "w:" -- "$T/wrap" "$T/utmpadd" "$S" -r "$T")
ok "the pid is printed" has "$o" "shutdown: [pid "
ok "the user is warned: system going down at ..., in 60 minutes" has "$o" "System going down at "
ok "...with the time left" has "$o" "(in 60 minutes)"
ok "...and the message" has "$o" "the test message"
ok "nologin holds the time and the message" has "$o" "System going down at "
ok "...the message is in it" sh -c 'case "$1" in *"NOLOGIN:"*"the test message"*) exit 0;; esac; exit 1' sh "$o"
ok "the shutdown waits in the background" has "$o" "ALIVE=yes"
ok "cancelling it: the users are told" has "$o" "Shutdown cancelled"
ok "...nologin is gone" has "$o" "NOLOGIN-GONE"
ok "...and it has ended" sh -c 'case "$1" in *"NOLOGIN-GONE"*"ALIVE=no"*) exit 0;; esac; exit 1' sh "$o"
ok "a message of words is joined" has "$o" "the test message"

# a time of 1 minute is announced as 1 minute (singular)
cat > "$T/wrap1" <<'WRAP'
#!/bin/sh
t=$(tty); l=${t#/dev/}
"$1" "$l" tuser
"$2" "$3" +1 > "$4/out"
pid=$(sed 's/.*\[pid \([0-9]*\)\].*/\1/' "$4/out")
sleep 1
kill "$pid"
sleep 1
WRAP
chmod 755 "$T/wrap1"
o=$("$D" -T 40 "w:" -- "$T/wrap1" "$T/utmpadd" "$S" -r "$T")
ok "+1: one minute, singular" has "$o" "(in 1 minute)"
ok "-h, -p, -r are accepted (a halt, a power off, a reboot after the delay)" sh -c '
    for f in -h -p -r; do
        "$1" "$2" tuser > /dev/null 2>&1
        out=$("$3" $f +60 2>&1); pid=${out##*pid }; pid=${pid%%]*}
        kill "$pid" 2>/dev/null; sleep 1
        [ -e /etc/nologin ] && exit 1
        case $out in *"[pid "*) ;; *) exit 1;; esac
    done; exit 0' sh "$T/utmpadd" "x" "$S"
rm -f /etc/nologin
done_tests shutdown
