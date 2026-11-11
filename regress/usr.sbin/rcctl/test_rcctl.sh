#!/bin/sh
# test_rcctl.sh - tests for rcctl, and through it for etc/rc.d/rc.subr (V86BSD).
# Run: sh test_rcctl.sh
# The services are made here (a directory of scripts that use the rc.subr of the
# tree) and run a daemon of their own, a program that does nothing but wait and
# makes a file when it gets SIGHUP; rc.conf and rc.d are those of the test
# (RC_CONF, RCTL_DIR): nothing of the running system is touched.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.sbin/rcctl}; export PROG_DIR
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
R=${R:-$PROG_DIR/rcctl.sh}
SUBR=$(cd ../../../etc/rc.d && pwd)/rc.subr
T=${TMPDIR:-/tmp}/test_rcctl.$$
mkdir -m 755 "$T" || exit 2
mkdir "$T/rc.d"
cleanup() {
    for s in ctl nr; do sh "$T/rc.d/$s" stop > /dev/null 2>&1; done
    rm -rf "$T"
}
trap cleanup EXIT
RC_CONF=$T/rc.conf RCTL_DIR=$T/rc.d; export RC_CONF RCTL_DIR

cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
cat > "$T/fd.c" <<'EOT'
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
static const char *file;
static void hup(int s) { int fd = open(file, O_WRONLY | O_CREAT, 0644); (void)s; if (fd >= 0) close(fd); }
int main(int argc, char *argv[]) { file = argc > 1 ? argv[1] : "/dev/null"; signal(SIGHUP, hup); for (;;) pause(); }
EOT
$cc -o "$T/fd" "$T/fd.c" || { echo "rcctl: cannot build the test daemon"; exit 2; }

svc() {   # name daemon flags extra-line
    cat > "$T/rc.d/$1" <<EOT
#!/bin/sh
daemon="$2"
daemon_flags="$3"
. $SUBR
$4
rc_cmd \$1
EOT
}
svc ctl "$T/fd" "$T/hup" "rc_bg=YES"
svc nr "$T/fd" "$T/nr.hup" "rc_bg=YES
rc_reload=NO"
svc missing /nonexistent/prog "" ""
cp "$SUBR" "$T/rc.d/rc.subr"
cat > "$T/rc.conf" <<EOT
# the test's rc.conf
ctl_flags=
nr_flags=NO
EOT
cp "$T/rc.conf" "$T/rc.conf.orig"

usage1="usage: rcctl ls all|on|off|started|stopped|failed"
t "no arguments" "$usage1" 0 sh -c '"$1" 2>&1 | head -n 1' sh "$R"
ok "no arguments: status 1" sh -c '! "$1" 2>/dev/null' sh "$R"
t "a command that is none" "$usage1" 0 sh -c '"$1" bogus 2>&1 | grep "^usage: rcctl ls"' sh "$R"
t "ls with a bad argument" "$usage1" 0 sh -c '"$1" ls bogus 2>&1 | grep "^usage: rcctl ls"' sh "$R"

t "ls all (rc.subr is not a service)" "ctl
missing
nr" 0 "$R" ls all
t "ls on" "ctl" 0 "$R" ls on
t "ls off" "missing
nr" 0 "$R" ls off
t "get: the flags line" "ctl_flags=" 0 "$R" get ctl
t "get: a service that is off" "nr_flags=NO" 0 "$R" get nr
t "get: one that rc.conf does not mention" "missing_flags=NO" 0 "$R" get missing
t "get flags" "NO" 0 "$R" get nr flags
t "get status: on" "on" 0 "$R" get ctl status
t "get status: off" "off" 0 "$R" get nr status
t "get: no such service" "rcctl: service nosuch does not exist" 2 sh -c '"$1" get nosuch 2>&1' sh "$R"
t "get: rc.subr is not a service" "rcctl: service rc.subr does not exist" 2 sh -c '"$1" get rc.subr 2>&1' sh "$R"
t "get: a name with a slash" "rcctl: service ../x does not exist" 2 sh -c '"$1" get ../x 2>&1' sh "$R"
t "get: a name that is an option" "rcctl: service -x does not exist" 2 sh -c '"$1" get -x 2>&1' sh "$R"

# enable and disable write rc.conf.local, never rc.conf
t "enable: a line in rc.conf.local" "nr_flags=" 0 sh -c '"$1" enable nr && cat "$2.local"' sh "$R" "$RC_CONF"
t "enable: it is on" "on" 0 "$R" get nr status
ok "enable: rc.conf is as it was" cmp -s "$T/rc.conf" "$T/rc.conf.orig"
t "disable: the line goes (rc.conf says NO)" "" 0 sh -c '"$1" disable nr && cat "$2.local"' sh "$R" "$RC_CONF"
t "disable: it is off" "off" 0 "$R" get nr status
t "disable a service that is on: NO" "ctl_flags=NO" 0 sh -c '"$1" disable ctl && cat "$2.local"' sh "$R" "$RC_CONF"
t "disable: it is off" "off" 0 "$R" get ctl status
t "enable it again: the line goes (rc.conf says on)" "" 0 sh -c '"$1" enable ctl && cat "$2.local"' sh "$R" "$RC_CONF"
t "enable: on" "on" 0 "$R" get ctl status
t "enable twice: nothing more" "" 0 sh -c '"$1" enable ctl && "$1" enable ctl && cat "$2.local"' sh "$R" "$RC_CONF"
t "enable two services" "missing_flags=
nr_flags=" 0 sh -c '"$1" enable missing nr && sort "$2.local"' sh "$R" "$RC_CONF"
t "enable: a service that is not" "rcctl: service nosuch does not exist" 2 sh -c '"$1" enable nr nosuch 2>&1' sh "$R"
t "disable both" "" 0 sh -c '"$1" disable missing nr && cat "$2.local"' sh "$R" "$RC_CONF"
ok "rc.conf.local is mode 644" sh -c 'ls -l "$1.local" | cut -c1-10 | grep -x -- "-rw-r--r--" > /dev/null' sh "$RC_CONF"

# flags
t "set flags: quoted in the file" "ctl_flags='-a -b'" 0 sh -c '"$1" set ctl flags -a -b && cat "$2.local"' sh "$R" "$RC_CONF"
t "set flags: get" "-a -b" 0 "$R" get ctl flags
t "set flags: get shows it" "ctl_flags=-a -b" 0 "$R" get ctl
t "set flags: a line is replaced, not added" "ctl_flags='-c'" 0 sh -c '"$1" set ctl flags -c && cat "$2.local"' sh "$R" "$RC_CONF"
t "set flags with nothing: the default flags" "ctl_flags=" 0 sh -c '"$1" set ctl flags && cat "$2.local"' sh "$R" "$RC_CONF"
t "set flags NO: off" "off" 0 sh -c '"$1" set ctl flags NO && "$1" get ctl status' sh "$R"
t "set flags turns it on" "on" 0 sh -c '"$1" set nr flags -z && "$1" get nr status' sh "$R"
t "a single quote is refused" "rcctl: flags must not contain a single quote" 1 sh -c '"$1" set ctl flags "it'"'"'s" 2>&1' sh "$R"
t "a control character is refused" "rcctl: flags must not contain control characters" 1 sh -c '"$1" set ctl flags "$(printf "a\tb")" 2>&1' sh "$R"
t "a newline is refused" "rcctl: flags must not contain control characters" 1 sh -c '"$1" set ctl flags "a
b=c" 2>&1' sh "$R"
t "a refused one changes nothing" "ctl_flags=NO
nr_flags='-z'" 0 sh -c 'sort "$1.local"' sh "$RC_CONF"
t "a value cannot run: the shell metacharacters stay quoted" "ctl_flags=\$(touch $T/injected) \`id\` ; x" 0 sh -c '"$1" set ctl flags "\$(touch $2/injected) \`id\` ; x" && "$1" get ctl' sh "$R" "$T"
ok "and nothing was run" sh -c '[ ! -e "$1/injected" ]' sh "$T"
t "set status off" "off" 0 sh -c '"$1" set ctl status off && "$1" get ctl status' sh "$R"
t "set status on" "on" 0 sh -c '"$1" set ctl status on && "$1" get ctl status' sh "$R"
t "set status: a bad word" "0" 0 sh -c '"$1" set ctl status maybe >/dev/null 2>&1; echo $((($?)-1))' sh "$R"
# back to the start
rm -f "$T/rc.conf.local"

# running a service
pids() { ps -A -o pid= -o args= | grep -F -- "$T/fd $1" | grep -v grep | wc -l | tr -d ' '; }
t "check: not running" "ctl(failed)" 1 "$R" check ctl
t "stop: not running" "ctl(failed)" 1 "$R" stop ctl
t "reload: not running" "ctl(failed)" 1 "$R" reload ctl
t "start" "ctl(ok)" 0 "$R" start ctl
t "the daemon runs, with the default flags" "1" 0 pids "$T/hup"
t "check" "ctl(ok)" 0 "$R" check ctl
t "start again: one daemon still" "ctl(ok)" 0 sh -c '"$1" start ctl' sh "$R"
t "one daemon" "1" 0 pids "$T/hup"
t "reload: SIGHUP reached it" "ctl(ok)" 0 "$R" reload ctl
i=0; while [ ! -e "$T/hup" ] && [ $i -lt 10 ]; do sleep 1; i=$((i+1)); done
ok "reload: it got the signal" test -e "$T/hup"
t "ls started" "ctl" 0 "$R" ls started
t "ls stopped" "missing
nr" 0 "$R" ls stopped
t "ls failed: on and not running" "" 0 "$R" ls failed
t "stop" "ctl(ok)" 0 "$R" stop ctl
t "stop: it is gone" "0" 0 pids "$T/hup"
t "ls failed now" "ctl" 0 "$R" ls failed
t "restart (it was off)" "ctl(ok)" 0 "$R" restart ctl
OLD=$(ps -A -o pid= -o args= | grep -F -- "$T/fd $T/hup" | grep -v grep | awk '{print $1}')
t "restart: another process" "ctl(ok)" 0 "$R" restart ctl
NEW=$(ps -A -o pid= -o args= | grep -F -- "$T/fd $T/hup" | grep -v grep | awk '{print $1}')
ok "restart: the pid changed" test -n "$OLD" -a -n "$NEW" -a "$OLD" != "$NEW"
t "one daemon after the restart" "1" 0 pids "$T/hup"
t "flags from rc.conf.local are used" "ctl(ok)" 0 sh -c '"$1" stop ctl >/dev/null && "$1" set ctl flags "$2/hup2" && "$1" start ctl' sh "$R" "$T"
t "the daemon has them" "1" 0 pids "$T/hup2"
t "check uses them" "ctl(ok)" 0 "$R" check ctl
t "the default flags are not the ones it runs with" "0" 0 pids "$T/hup "
"$R" stop ctl > /dev/null
rm -f "$T/rc.conf.local"
t "a service that is off starts when told" "nr(ok)" 0 "$R" start nr
t "reload: rc_reload=NO" "nr(failed)" 1 "$R" reload nr
t "stop two at once" "ctl(failed)
nr(ok)" 1 "$R" stop ctl nr
t "RC_QUIET: nothing printed" "" 0 sh -c 'RC_QUIET=1 "$1" start ctl' sh "$R"
t "RC_QUIET: and the status" "1" 0 sh -c 'RC_QUIET=1 "$1" check nr >/dev/null; echo $?' sh "$R"
t "a daemon that is not there" "missing(failed)" 1 "$R" start missing
t "told why" "missing: /nonexistent/prog: not found
missing(failed)" 1 sh -c '"$1" start missing 2>&1' sh "$R"
t "the script run by hand: usage" "usage: $T/rc.d/ctl start|stop|restart|reload|check" 1 sh -c 'sh "$1/rc.d/ctl" bogus 2>&1' sh "$T"
t "start a service that is not one" "rcctl: service nosuch does not exist" 2 sh -c '"$1" start nosuch 2>&1' sh "$R"
"$R" stop ctl > /dev/null 2>&1
t "start: every name is checked before one is started" "rcctl: service nosuch does not exist" 2 sh -c '"$1" start ctl nosuch 2>&1' sh "$R"
t "and so the first was not started" "ctl(failed)" 1 "$R" check ctl
done_tests rcctl
