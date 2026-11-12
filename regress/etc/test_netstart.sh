#!/bin/sh
# test_netstart.sh - tests for /etc/netstart (V86BSD).  Run as root, inside
# V86BSD, with at least two network interfaces besides lo0 (boot.sh -- -nic
# user,model=e1000 gives a second one): the second one is configured, the
# first (the kernel's DHCP) only for the "dhcp" and "replace in place" tests.
#   sh test_netstart.sh        (NETSTART=/path to test another copy)
# Everything is put back.
cd "$(dirname "$0")" || exit 2
pass=0 fail=0
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>&1); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
export LC_ALL=C
NS=${NETSTART:-/etc/netstart}
if [ "$(uname -s)" != V86BSD ] || [ "$(id -u)" != 0 ]; then
    echo "netstart: 0 passed, 0 failed (skipped: needs root on V86BSD)"
    exit 0
fi
FIRST=$(ifconfig -l | tr ' ' '\n' | sed -n 2p)
NIC=$(ifconfig -l | tr ' ' '\n' | sed -n 3p)
if [ -z "$NIC" ]; then
    echo "netstart: 0 passed, 0 failed (skipped: needs a second network interface)"
    exit 0
fi
IDX=$(ifconfig "$NIC" | sed -n 's/^[ 	]index //p')
GW=$(route -n show | awk '$1 == "default" { print $2 }')
has() { route -n show | grep -E -- "$1" > /dev/null; }
conf() { ifconfig "$NIC" | sed -n 's/^[ 	]*\(inet .*\)$/\1/p'; }

TD=/var/tmp/test_netstart.$$
mkdir -m 700 "$TD" || exit 2
[ -e /etc/mygate ] && cp -p /etc/mygate "$TD/mygate"
firstinet=$(ifconfig "$FIRST" | sed -n 's/^[ 	]*inet \([0-9.]*\) netmask \(0x[0-9a-f]*\).*/\1 \2/p' | head -n 1)
reset() {
    rm -f /etc/hostname.* /etc/mygate
    for a in 1 2 3 4; do ifconfig "$NIC" inet delete 2> /dev/null; done
    ifconfig "$NIC" down 2> /dev/null
    ifconfig "$NIC" mtu 1500 2> /dev/null
}
cleanup() {
    reset
    [ -e "$TD/mygate" ] && cp -p "$TD/mygate" /etc/mygate
    if [ -n "$firstinet" ]; then
        set -- $firstinet
        ifconfig "$FIRST" inet "$1" netmask "$2"
    fi
    route -q delete default 2> /dev/null
    [ -n "$GW" ] && route -q add default "$GW"
    rm -rf "$TD"
}
trap cleanup EXIT
reset

# nothing to do: no output, success
t "no files, no gateway: nothing" "" 0 sh "$NS"

# an address, a mask, and the interface comes up
printf '# the second card\n\ninet 192.0.2.10 netmask 255.255.255.0   # a comment\n' > "/etc/hostname.$NIC"
t "one interface" "configuring network interfaces: $NIC." 0 sh "$NS"
t "its address and mask" "inet 192.0.2.10 netmask 0xffffff00 broadcast 192.0.2.255" 0 conf
ok "it is up" sh -c 'ifconfig "$1" | grep "^$1: flags=[0-9a-f]*<UP," > /dev/null' sh "$NIC"
ok "its network is a route" has "^192\\.0\\.2/24 +link#$IDX +U +0 +$NIC\$"

# again: nothing changes (the first address is set in place)
t "a second run" "configuring network interfaces: $NIC." 0 sh "$NS"
t "is the same" "inet 192.0.2.10 netmask 0xffffff00 broadcast 192.0.2.255" 0 conf

# aliases, mtu, a prefix length, another address
printf 'inet 192.0.2.20/25\ninet alias 198.51.100.5 netmask 255.255.255.0\nmtu 1400\n' > "/etc/hostname.$NIC"
t "aliases and mtu" "configuring network interfaces: $NIC." 0 sh "$NS"
t "the first address moved in place" "inet 192.0.2.20 netmask 0xffffff80 broadcast 192.0.2.127
inet 198.51.100.5 netmask 0xffffff00 broadcast 198.51.100.255" 0 conf
ok "the mtu" sh -c 'ifconfig "$1" | grep " mtu 1400$" > /dev/null' sh "$NIC"

# a line "down" keeps it down
reset
printf 'inet 192.0.2.30/24\ndown\n' > "/etc/hostname.$NIC"
t "down" "configuring network interfaces: $NIC." 0 sh "$NS"
ok "the address is there, the interface is not up" sh -c 'ifconfig "$1" | grep "inet 192.0.2.30 " > /dev/null && ! ifconfig "$1" | grep "^$1: flags=[0-9a-f]*<UP," > /dev/null' sh "$NIC"

# the default gateway: mygate
reset
printf 'inet 192.0.2.40/24\n' > "/etc/hostname.$NIC"
echo "# the way out
192.0.2.1   # a router" > /etc/mygate
t "with a gateway" "configuring network interfaces: $NIC." 0 sh "$NS"
ok "a default route through it" has '^default +192\.0\.2\.1 +UG +0 +'"$NIC"'$'
ok "it is the only default" sh -c '[ "$(route -n show | awk "\$1 == \"default\"" | wc -l)" = 1 ]'
ok "the one of DHCP is gone" sh -c '[ -z "$1" ] || ! route -n show | grep -E "^default +$1 " > /dev/null' sh "$GW"
t "mygate again: replaced, not added" "configuring network interfaces: $NIC." 0 sh "$NS"
ok "still one default" sh -c '[ "$(route -n show | awk "\$1 == \"default\"" | wc -l)" = 1 ]'
# an interface named: no gateway work
route -q delete default
t "named: the gateway is left alone" "configuring network interfaces: $NIC." 0 sh "$NS" "$NIC"
ok "no default route" sh -c '! route -n show | grep "^default " > /dev/null'
# a gateway that cannot be reached: told, status 1
echo 203.0.113.1 > /etc/mygate
t "an unreachable gateway" "configuring network interfaces: $NIC.
route: add net default: gateway 203.0.113.1: Network is unreachable" 1 sh "$NS"
# a gateway without interfaces
reset
echo 192.0.2.1 > /etc/mygate
t "gateway on a network of an interface that is not configured" "route: add net default: gateway 192.0.2.1: Network is unreachable" 1 sh "$NS"
rm -f /etc/mygate

# what is not a file of an interface
reset
printf 'inet 192.0.2.50/24\n' > "/etc/hostname.$NIC"
printf 'inet 192.0.2.51/24\n' > "/etc/hostname.$NIC.bak"
mkdir /etc/hostname.dir0
t "only hostname.<name><unit>" "configuring network interfaces: $NIC." 0 sh "$NS"
rmdir /etc/hostname.dir0
rm -f "/etc/hostname.$NIC.bak"

# an interface that is not there, a bad line: told, status 1, the rest goes on
reset
printf 'inet 192.0.2.60/24\n' > "/etc/hostname.$NIC"
printf 'inet 10.99.0.1/24\n' > /etc/hostname.zz9
t "no such interface" "configuring network interfaces: $NIC zz9.
zz9: no such interface" 1 sh "$NS"
ok "the other was done" sh -c 'ifconfig "$1" | grep "inet 192.0.2.60 " > /dev/null' sh "$NIC"
rm -f /etc/hostname.zz9
printf 'inet 999.1.1.1\nmtu 1400\n' > "/etc/hostname.$NIC"
t "a bad address" "configuring network interfaces: $NIC.
ifconfig: 999.1.1.1: unknown parameter or invalid address" 1 sh "$NS"
printf 'inet *\n' > "/etc/hostname.$NIC"
ok "a pattern is not expanded" sh -c 'sh "$1" 2>&1 | grep "^ifconfig: \\*: unknown parameter or invalid address\$" > /dev/null' sh "$NS"

# named interfaces
reset
printf 'inet 192.0.2.70/24\n' > "/etc/hostname.$NIC"
t "named" "configuring network interfaces: $NIC." 0 sh "$NS" "$NIC"
t "named, without a file" "netstart: no /etc/hostname.zz9" 1 sh "$NS" zz9
t "named, one without a file stops it all" "netstart: no /etc/hostname.zz9" 1 sh "$NS" "$NIC" zz9

# dhcp: the kernel's, on the interface it configured
reset
printf 'dhcp\n' > "/etc/hostname.$NIC"
t "dhcp on an interface without one" "configuring network interfaces: $NIC.
$NIC: dhcp: the kernel runs DHCP on the first Ethernet interface only" 1 sh "$NS"
t "(it is up, without an address)" "" 0 conf
if [ -n "$firstinet" ]; then
    reset
    printf 'dhcp\nmtu 1400\n' > "/etc/hostname.$FIRST"
    t "dhcp on the first one: left as it is" "configuring network interfaces: $FIRST." 0 sh "$NS"
    t "(its address stays)" "$firstinet" 0 sh -c 'ifconfig "$1" | sed -n "s/^[ 	]*inet \\([0-9.]*\\) netmask \\(0x[0-9a-f]*\\).*/\\1 \\2/p" | head -n 1' sh "$FIRST"
    ifconfig "$FIRST" mtu 1500
    # a static file for the first one: its address goes in place
    reset
    printf 'inet 10.0.2.99 netmask 255.255.255.0\n' > "/etc/hostname.$FIRST"
    t "static, on the first one" "configuring network interfaces: $FIRST." 0 sh "$NS"
    t "the address was replaced" "10.0.2.99 0xffffff00" 0 sh -c 'ifconfig "$1" | sed -n "s/^[ 	]*inet \\([0-9.]*\\) netmask \\(0x[0-9a-f]*\\).*/\\1 \\2/p"' sh "$FIRST"
    ok "it is still up" sh -c 'ifconfig "$1" | grep "^$1: flags=[0-9a-f]*<UP," > /dev/null' sh "$FIRST"
fi
reset
done_tests netstart
