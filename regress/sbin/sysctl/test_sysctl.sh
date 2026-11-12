#!/bin/sh
# test_sysctl.sh - tests for sysctl (V86BSD).  Run: sh test_sysctl.sh
# sysctl(2) is a native call: on another system the tests are skipped.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/sysctl}; export PROG_DIR
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# te NAME EXPECTED-OUTPUT EXPECTED-STATUS COMMAND...  (stdout and stderr together)
te() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>&1); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$PROG_DIR/sysctl}
T=${TMPDIR:-/tmp}/test_sysctl.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
export LC_ALL=C
if ! "$U" -n kern.ostype > /dev/null 2>&1; then
    echo "sysctl: 0 passed, 0 failed (skipped: no sysctl(2) here)"
    exit 0
fi
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'

t "a string" "kern.ostype=V86BSD" 0 "$U" kern.ostype
t "-n: the value alone" "V86BSD" 0 "$U" -n kern.ostype
t "the release is uname's" "$(uname -r)" 0 "$U" -n kern.osrelease
t "the version is uname's" "$(uname -v)" 0 "$U" -n kern.osversion
t "the machine" "hw.machine=amd64" 0 "$U" hw.machine
t "an int" "1" 0 "$U" -n hw.ncpu
t "byte order" "1234" 0 "$U" -n hw.byteorder
t "the page size is getconf's" "$(getconf PAGE_SIZE)" 0 "$U" -n hw.pagesize
t "a quad" "ok" 0 sh -c 'v=$("$1" -n hw.physmem) && [ "$v" -ge 67108864 ] && echo ok' sh "$U"
t "the model is not empty" "ok" 0 sh -c 'v=$("$1" -n hw.model) && [ -n "$v" ] && echo ok' sh "$U"
t "the disks" "ok" 0 sh -c 'v=$("$1" -n hw.disknames) && case $v in disk0:*) echo ok;; esac' sh "$U"
t "clockrate" "kern.clockrate=tick = 1000, hz = 1000, profhz = 1000, stathz = 1000" 0 "$U" kern.clockrate
t "boottime" "ok" 0 sh -c '"$1" kern.boottime | grep "^kern.boottime=[A-Z][a-z][a-z] [A-Z][a-z][a-z]" > /dev/null && echo ok' sh "$U"
t "boottime -n: seconds" "ok" 0 sh -c 'v=$("$1" -n kern.boottime) && [ "$v" -gt 1700000000 ] && [ "$v" -le "$(date +%s)" ] && echo ok' sh "$U"
t "loadavg" "ok" 0 sh -c '"$1" vm.loadavg | grep "^vm.loadavg=[0-9][0-9]*\.[0-9][0-9] [0-9][0-9]*\.[0-9][0-9] [0-9][0-9]*\.[0-9][0-9]$" > /dev/null && echo ok' sh "$U"
t "two names" "kern.ostype=V86BSD${nl}hw.machine=amd64" 0 "$U" kern.ostype hw.machine

# a node: everything below it
ok "a node lists its leaves" sh -c '"$1" hw | grep "^hw.machine=amd64$" > /dev/null && "$1" hw | grep "^hw.pagesize=" > /dev/null' sh "$U"
ok "a node lists only its own" sh -c '! "$1" hw | grep -v "^hw\." > /dev/null' sh "$U"
ok "kern lists ostype" sh -c '"$1" kern | grep "^kern.ostype=V86BSD$" > /dev/null' sh "$U"
t "net.inet.icmp lists its two" "net.inet.icmp.bmcastecho=0${nl}net.inet.icmp.errppslimit=100" 0 "$U" net.inet.icmp
t "net.inet.tcp lists its one" "net.inet.tcp.rstppslimit=100" 0 "$U" net.inet.tcp
te "an icmp name the kernel does not have" "sysctl: net.inet.icmp.maskrepl: value is not available" 1 "$U" net.inet.icmp.maskrepl
ok "net.inet.ip lists nothing (reserved)" sh -c '"$1" net.inet.ip > "$2"; [ ! -s "$2" ]' sh "$U" "$T/o"

# everything
ok "-a: kern.ostype" sh -c '"$1" -a | grep "^kern.ostype=V86BSD$" > /dev/null' sh "$U"
ok "-a: hw.model, vm.loadavg" sh -c '"$1" -a | grep "^hw.model=" > /dev/null && "$1" -a | grep "^vm.loadavg=" > /dev/null' sh "$U"
ok "-a: no gaps, no unanswered names" sh -c '! "$1" -a | grep "gap\|kern.proc=\|net\.inet\.ip\." > /dev/null' sh "$U"
t "-a: the network knobs" "net.inet.icmp.bmcastecho=0${nl}net.inet.icmp.errppslimit=100${nl}net.inet.tcp.rstppslimit=100" 0 sh -c '"$1" -a | grep "^net\."' sh "$U"
ok "-a: every line is name=value (kern.version has more lines)" sh -c '! "$1" -a | grep -v "^[a-z0-9_.-]*=\|^ \|^$" > /dev/null' sh "$U"
ok "no arguments: the same as -a" sh -c '[ "$("$1")" = "$("$1" -a)" ]' sh "$U"
ok "-A is -a" sh -c '[ "$("$1" -A | head -n 3)" = "$("$1" -a | head -n 3)" ]' sh "$U"
t "-a -n: values alone" "ok" 0 sh -c '"$1" -a -n | grep "^V86BSD$" > /dev/null && echo ok' sh "$U"

# errors
te "an unknown top level" "sysctl: level name nosuch in nosuch.x is invalid" 1 "$U" nosuch.x
te "an unknown name" "sysctl: level name nosuch in kern.nosuch is invalid" 1 "$U" kern.nosuch
te "a name past a leaf" "sysctl: name x in kern.ostype.x is unknown" 1 "$U" kern.ostype.x
te "a gap is not a name" "sysctl: level name gap in kern.gap is invalid" 1 "$U" kern.gap
te "a reserved id" "sysctl: kern.proc: value is not available" 1 "$U" kern.proc
te "net.inet.ip.forwarding is reserved" "sysctl: net.inet.ip.forwarding: value is not available" 1 "$U" net.inet.ip.forwarding
te "a class with no names known" "sysctl: debug: class is not implemented" 1 "$U" debug
te "a node cannot be set" "sysctl: hw: a node cannot be set" 1 "$U" hw=1
te "a read-only string" "sysctl: kern.ostype: Operation not permitted" 1 "$U" kern.ostype=x
te "a read-only int" "sysctl: kern.maxproc: Operation not permitted" 1 "$U" kern.maxproc=5
te "an invalid int" "sysctl: kern.maxproc: value is invalid" 1 "$U" kern.maxproc=abc
te "an empty int" "sysctl: kern.maxproc: value is invalid" 1 "$U" kern.maxproc=
te "an int too big" "sysctl: kern.maxproc: value is invalid" 1 "$U" kern.maxproc=99999999999
te "a structure cannot be set" "sysctl: kern.clockrate: a structure cannot be set" 1 "$U" kern.clockrate=1
te "a missing file" "sysctl: $T/nofile: No such file or directory" 1 "$U" -f "$T/nofile"
t "a bad option: status 1, nothing on stdout" "" 1 "$U" -x
ok "a bad option: the usage line" sh -c '"$1" -x 2>&1 | grep "^usage: sysctl \[-Aanq\]" > /dev/null' sh "$U"
ok "an error does not stop the other names" sh -c 'out=$("$1" kern.nosuch hw.machine 2>/dev/null); [ "$out" = "hw.machine=amd64" ]' sh "$U"

if $root; then
    old=$("$U" -n kern.hostname)
    te "set the host name" "kern.hostname: $old -> sysctl-test" 0 "$U" kern.hostname=sysctl-test
    t "it is set" "kern.hostname=sysctl-test" 0 "$U" kern.hostname
    t "uname sees it" "sysctl-test" 0 uname -n
    t "hostname sees it, if there is one" "sysctl-test" 0 sh -c 'command -v hostname > /dev/null 2>&1 && hostname || echo sysctl-test'
    t "-n set: the new value alone" "sysctl-n" 0 "$U" -n kern.hostname=sysctl-n
    t "-q set: nothing printed" "" 0 "$U" -q kern.hostname=sysctl-q
    t "-q: but it is set" "sysctl-q" 0 "$U" -n kern.hostname
    t "a value with spaces after the =" "kern.hostname: sysctl-q -> spaced" 0 "$U" 'kern.hostname=   spaced'
    te "a name of 65 bytes is too long" "sysctl: kern.hostname: Invalid argument" 1 "$U" kern.hostname=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
    t "and nothing changed" "spaced" 0 "$U" -n kern.hostname
    t "the domain name" "kern.domainname: $("$U" -n kern.domainname) -> test.example" 0 "$U" kern.domainname=test.example
    # a file
    cat > "$T/conf" <<CONF
# a comment
   
kern.hostname=from-file   # another comment
kern.domainname=file.example
CONF
    t "-f: sets what the file says" "kern.hostname: spaced -> from-file${nl}kern.domainname: test.example -> file.example" 0 "$U" -f "$T/conf"
    t "-f -q: read back" "from-file file.example" 0 sh -c 'echo $("$1" -n kern.hostname) $("$1" -n kern.domainname)' sh "$U"
    printf 'kern.nosuch=1\nkern.hostname=after-error\n' > "$T/conf2"
    te "-f: an error in the file: the rest is read, the status is 1" "sysctl: level name nosuch in kern.nosuch is invalid
kern.hostname: from-file -> after-error" 1 "$U" -f "$T/conf2"
    "$U" -q kern.hostname="$old" kern.domainname="(none)"
    # the network knobs
    t "set bmcastecho" "net.inet.icmp.bmcastecho: 0 -> 1" 0 "$U" net.inet.icmp.bmcastecho=1
    t "it is set" "net.inet.icmp.bmcastecho=1" 0 "$U" net.inet.icmp.bmcastecho
    te "bmcastecho is 0 or 1" "sysctl: net.inet.icmp.bmcastecho: Invalid argument" 1 "$U" net.inet.icmp.bmcastecho=2
    t "back to 0" "net.inet.icmp.bmcastecho: 1 -> 0" 0 "$U" net.inet.icmp.bmcastecho=0
    t "errppslimit: no limit" "net.inet.icmp.errppslimit: 100 -> -1" 0 "$U" net.inet.icmp.errppslimit=-1
    te "errppslimit under -1" "sysctl: net.inet.icmp.errppslimit: Invalid argument" 1 "$U" net.inet.icmp.errppslimit=-2
    t "errppslimit back" "net.inet.icmp.errppslimit: -1 -> 100" 0 "$U" net.inet.icmp.errppslimit=100
    t "rstppslimit" "net.inet.tcp.rstppslimit: 100 -> 400" 0 "$U" net.inet.tcp.rstppslimit=400
    t "rstppslimit back" "net.inet.tcp.rstppslimit: 400 -> 100" 0 "$U" net.inet.tcp.rstppslimit=100
else
    te "not root: set is refused" "sysctl: kern.hostname: Operation not permitted" 1 "$U" kern.hostname=evil
    te "not root: nor a network knob" "sysctl: net.inet.icmp.bmcastecho: Operation not permitted" 1 "$U" net.inet.icmp.bmcastecho=1
fi
done_tests sysctl
