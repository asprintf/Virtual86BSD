#!/bin/sh
# test_ifconfig.sh - tests for ifconfig (V86BSD).  Run: sh test_ifconfig.sh
# On V86BSD as root with a network card the interface is changed (an alias,
# the mtu, the flags, then its own address) and put back; elsewhere, or not
# as root, only the usage and what any user may do is tested.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/ifconfig}; export PROG_DIR
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
I=${I:-$PROG_DIR/ifconfig}
U="usage: ifconfig [-a] [-l] [interface [inet] [parameters]]"
usage1() { _o=$("$@" 2>&1); _r=$?; echo "$_o" | grep '^usage:'; return $_r; }
t "an unknown option" "$U" 1 usage1 "$I" -x
t "-a with an interface" "$U" 1 usage1 "$I" -a lo0
t "-l with an interface" "$U" 1 usage1 "$I" -l lo0
if [ "$(uname -s)" != V86BSD ]; then
    done_tests ifconfig
    exit
fi
t "an interface that is not there" "ifconfig: interface nosuch0 does not exist" 1 sh -c '"$1" nosuch0 2>&1' sh "$I"
t "an interface that is not there, with parameters" "ifconfig: interface nosuch0 does not exist" 1 sh -c '"$1" nosuch0 up 2>&1' sh "$I"
t "the names" "lo0" 0 sh -c '"$1" -l | cut -d" " -f1' sh "$I"
t "the loopback" "lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> mtu 65536" 0 sh -c '"$1" lo0 | head -n 1' sh "$I"
t "the loopback's index" "	index 1" 0 sh -c '"$1" lo0 | sed -n 2p' sh "$I"
t "the loopback's address" "	inet 127.0.0.1 netmask 0xff000000" 0 sh -c '"$1" lo0 | sed -n 3p' sh "$I"
t "-a starts with lo0" "lo0:" 0 sh -c '"$1" -a | head -n 1 | cut -d" " -f1' sh "$I"
t "no arguments is -a" "$("$I" -a)" 0 "$I"
t "an interface and inet shows it" "$("$I" lo0)" 0 "$I" lo0 inet
NIC=$("$I" -l | tr ' ' '\n' | sed -n 2p)
if [ -z "$NIC" ]; then
    done_tests ifconfig
    exit
fi
sh_nic() { "$I" "$NIC"; }
ok "the card is up with an Ethernet's flags" sh -c 'sh_nic() { "$1" "$2"; }; sh_nic "$1" "$2" | head -n 1 | grep -E "^$2: flags=8843<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> mtu 1500\$" >/dev/null' sh "$I" "$NIC"
ok "it has a link level address" sh -c '"$1" "$2" | grep -E "^	lladdr ([0-9a-f]{2}:){5}[0-9a-f]{2}\$" >/dev/null' sh "$I" "$NIC"
ok "it has its index" sh -c '"$1" "$2" | grep -E "^	index 2\$" >/dev/null' sh "$I" "$NIC"
ok "it has an address with the mask and broadcast address" sh -c '"$1" "$2" | grep -E "^	inet [0-9.]+ netmask 0x[0-9a-f]{8} broadcast [0-9.]+\$" >/dev/null' sh "$I" "$NIC"
if [ "$(id -u)" != 0 ]; then
    done_tests ifconfig
    exit
fi
cd /tmp || exit 2
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
T=/var/tmp/test_ifconfig.$$
mkdir -m 755 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
OWN=$("$I" "$NIC" | sed -n 's/^	inet \([0-9.]*\) netmask \(0x[0-9a-f]*\).*/\1 \2/p' | head -n 1)
OWNA=${OWN% *} OWNM=${OWN#* }
GW=$(route show 2>/dev/null | awk '$1 == "default" { print $2 }')
shows() { "$I" "$NIC" | grep -F -- "$1" >/dev/null; }

t "an address" "" 0 "$I" "$NIC" 10.55.0.1/24 alias
ok "an alias with a prefix" sh -c '"$1" "$2" | grep -F "	inet 10.55.0.1 netmask 0xffffff00 broadcast 10.55.0.255" >/dev/null' sh "$I" "$NIC"
ok "the first address is still first" sh -c '"$1" "$2" | grep "inet " | head -n 1 | grep -F "inet $3" >/dev/null' sh "$I" "$NIC" "$OWNA"
t "the same alias again" "" 0 "$I" "$NIC" alias 10.55.0.1/24
t "an alias with a netmask" "" 0 "$I" "$NIC" inet 10.56.7.7 netmask 255.255.0.0 alias
ok "its mask" sh -c '"$1" "$2" | grep -F "	inet 10.56.7.7 netmask 0xffff0000 broadcast 10.56.255.255" >/dev/null' sh "$I" "$NIC"
t "an alias with a hex mask" "" 0 "$I" "$NIC" 10.57.0.1 netmask 0xffffff00 alias
ok "the hex mask" sh -c '"$1" "$2" | grep -F "	inet 10.57.0.1 netmask 0xffffff00" >/dev/null' sh "$I" "$NIC"
t "a fifth address: there are four slots" "ifconfig: SIOCAIFADDR: No space left on device" 1 sh -c '"$1" "$2" 10.58.0.1 alias 2>&1' sh "$I" "$NIC"
t "delete with the address" "" 0 "$I" "$NIC" delete 10.57.0.1
t "delete" "" 0 "$I" "$NIC" 10.56.7.7 delete
t "an alias without a mask: its class'" "" 0 "$I" "$NIC" 10.58.0.1 alias
ok "class A" sh -c '"$1" "$2" | grep -F "	inet 10.58.0.1 netmask 0xff000000" >/dev/null' sh "$I" "$NIC"
t "-alias" "" 0 "$I" "$NIC" -alias 10.58.0.1
ok "it is gone" sh -c '! "$1" "$2" | grep -F "10.58.0.1" >/dev/null' sh "$I" "$NIC"
t "-alias of what is not there" "ifconfig: SIOCDIFADDR: Cannot assign requested address" 1 sh -c '"$1" "$2" -alias 10.58.0.1 2>&1' sh "$I" "$NIC"
ok "(only the first and the 10.55 one are left)" sh -c '[ "$("$1" "$2" | grep -c "inet ")" = 2 ]' sh "$I" "$NIC"
t "an address on lo0 that the card has" "ifconfig: SIOCAIFADDR: Address already in use" 1 sh -c '"$1" lo0 10.55.0.1 alias 2>&1' sh "$I"
t "an address 127.x on the card" "ifconfig: SIOCSIFADDR: Invalid argument" 1 sh -c '"$1" "$2" 127.1.2.3 2>&1' sh "$I" "$NIC"
t "address 0.0.0.0" "ifconfig: SIOCSIFADDR: Invalid argument" 1 sh -c '"$1" "$2" 0.0.0.0 2>&1' sh "$I" "$NIC"
t "a bad mask" "ifconfig: SIOCSIFNETMASK: Invalid argument" 1 sh -c '"$1" "$2" netmask 255.0.255.0 2>&1' sh "$I" "$NIC"
t "a bad address" "ifconfig: 10.1.1: unknown parameter or invalid address" 1 sh -c '"$1" "$2" 10.1.1 2>&1' sh "$I" "$NIC"
t "a netmask without a value" "ifconfig: netmask: needs a value" 1 sh -c '"$1" "$2" netmask 2>&1' sh "$I" "$NIC"
t "a mtu that is too large" "ifconfig: SIOCSIFMTU: Invalid argument" 1 sh -c '"$1" "$2" mtu 1501 2>&1' sh "$I" "$NIC"
t "a mtu that is too small" "ifconfig: SIOCSIFMTU: Invalid argument" 1 sh -c '"$1" "$2" mtu 67 2>&1' sh "$I" "$NIC"
t "a mtu" "" 0 "$I" "$NIC" mtu 1400
ok "it shows" shows "mtu 1400"
t "the mtu back" "" 0 "$I" "$NIC" mtu 1500
t "a metric" "" 0 "$I" "$NIC" metric 3
ok "it shows" shows " metric 3 mtu 1500"
t "the metric back" "" 0 "$I" "$NIC" metric 0
ok "(nothing shown for 0)" sh -c '! "$1" "$2" | grep -F metric >/dev/null' sh "$I" "$NIC"
t "debug and link0" "" 0 "$I" "$NIC" debug link0
ok "they show" sh -c '"$1" "$2" | head -n 1 | grep -F "<UP,BROADCAST,DEBUG,RUNNING,SIMPLEX,LINK0,MULTICAST>" >/dev/null' sh "$I" "$NIC"
t "-debug -link0" "" 0 "$I" "$NIC" -debug -link0
ok "they are off" sh -c '"$1" "$2" | head -n 1 | grep -F "flags=8843<" >/dev/null' sh "$I" "$NIC"
t "a flag it does not have" "ifconfig: promisc: unknown parameter or invalid address" 1 sh -c '"$1" "$2" promisc 2>&1' sh "$I" "$NIC"
t "down" "" 0 "$I" "$NIC" down
ok "it is down" sh -c '"$1" "$2" | head -n 1 | grep -F "flags=8802<BROADCAST,SIMPLEX,MULTICAST>" >/dev/null' sh "$I" "$NIC"
ok "it keeps its address" sh -c '"$1" "$2" | grep -F "inet $3" >/dev/null' sh "$I" "$NIC" "$OWNA"
t "up" "" 0 "$I" "$NIC" up
ok "it is up" sh -c '"$1" "$2" | head -n 1 | grep -F "flags=8843<" >/dev/null' sh "$I" "$NIC"
t "the broadcast address it has" "" 0 "$I" "$NIC" broadcast "$(sh -c '"$1" "$2" | sed -n "s/.*broadcast //p" | head -n 1' sh "$I" "$NIC")"
t "another one" "ifconfig: SIOCSIFBRDADDR: Operation not supported" 1 sh -c '"$1" "$2" broadcast 1.2.3.4 2>&1' sh "$I" "$NIC"

# replacing the first address, and the mask of it
t "a new first address" "" 0 "$I" "$NIC" 10.60.0.2
ok "it is the first, with the old mask" sh -c '"$1" "$2" | grep "inet " | head -n 1 | grep -F "inet 10.60.0.2 netmask $3" >/dev/null' sh "$I" "$NIC" "$OWNM"
ok "the old one is gone" sh -c '! "$1" "$2" | grep -F "inet $3 " >/dev/null' sh "$I" "$NIC" "$OWNA"
t "a mask for it" "" 0 "$I" "$NIC" netmask /25
ok "the mask" shows "inet 10.60.0.2 netmask 0xffffff80 broadcast 10.60.0.127"
t "address and prefix at once" "" 0 "$I" "$NIC" 10.61.0.3/20
ok "both" shows "inet 10.61.0.3 netmask 0xfffff000 broadcast 10.61.15.255"
t "its own address back" "" 0 "$I" "$NIC" "$OWNA" netmask "$OWNM"
ok "it is as it was" sh -c '"$1" "$2" | grep "inet " | head -n 1 | grep -F "inet $3 netmask $4" >/dev/null' sh "$I" "$NIC" "$OWNA" "$OWNM"
ok "the 10.55 alias is still there" shows "inet 10.55.0.1 "
t "the last alias" "" 0 "$I" "$NIC" -alias 10.55.0.1
# (DHCP's default route went with the network it was on)
if [ -n "$GW" ]; then
    route add default "$GW" > /dev/null 2>&1
    ok "the default route is back" sh -c 'route show | grep -E "^default +$1 " >/dev/null' sh "$GW"
fi

# who may change what
if $cc -o "$T/asuser" ../../lib/testlib/asuser.c 2>/dev/null; then
    t "anyone may look" "$("$I" "$NIC")" 0 "$T/asuser" 65534 65534 "$I" "$NIC"
    t "only root may change" "ifconfig: SIOCSIFMTU: Operation not permitted" 1 sh -c '"$1" 65534 65534 "$2" "$3" mtu 1400 2>&1' sh "$T/asuser" "$I" "$NIC"
    t "not an address either" "ifconfig: SIOCAIFADDR: Operation not permitted" 1 sh -c '"$1" 65534 65534 "$2" "$3" 10.9.9.9 alias 2>&1' sh "$T/asuser" "$I" "$NIC"
    ok "nothing changed" shows "mtu 1500"
fi
done_tests ifconfig
