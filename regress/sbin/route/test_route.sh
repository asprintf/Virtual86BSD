#!/bin/sh
# test_route.sh - tests for route (V86BSD).  Run: sh test_route.sh
# On V86BSD as root with a network card routes are added and removed (an
# alias of the card gives them a network to be on) and the table is put back;
# elsewhere, or not as root, only the usage and what any user may do is tested.
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../sbin/route}; export PROG_DIR
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
R=${R:-$PROG_DIR/route}
U="usage: route [-nqv] command [[-net | -host] destination [gateway] [-netmask mask]"
usage1() { _o=$("$@" 2>&1); _r=$?; echo "$_o" | grep '^usage:' | head -n 1; return $_r; }
t "no command" "$U" 1 usage1 "$R"
t "an unknown command" "$U" 1 usage1 "$R" frob
t "an unknown option" "$U" 1 usage1 "$R" -x show
if [ "$(uname -s)" != V86BSD ]; then
    done_tests route
    exit
fi
t "show with an operand" "$U" 1 usage1 "$R" show x
t "get without a destination" "$U" 1 usage1 "$R" get
t "get a bad destination" "route: invalid destination: nowhere" 1 sh -c '"$1" get nowhere 2>&1' sh "$R"
t "the table's heading" "Routing tables" 0 sh -c '"$1" show | head -n 1' sh "$R"
t "the columns" "Destination        Gateway            Flags Metric  Iface" 0 sh -c '"$1" show | sed -n 4p' sh "$R"
t "the loopback network" "127/8 link#1 U 0 lo0" 0 sh -c '"$1" -n show | awk '"'"'$1 == "127/8" { print $1, $2, $3, $4, $5 }'"'"'' sh "$R"
t "-n and -v are accepted" "$("$R" show)" 0 "$R" -nv show
ok "the card's network" sh -c '"$1" show | grep -E "^[0-9.]+/[0-9]+ +link#2 +U +0 +[a-z]+0\$" >/dev/null' sh "$R"
ok "get shows the route" sh -c '"$1" get 127.0.0.1 | grep -F "interface: lo0" >/dev/null' sh "$R"
t "get 127.0.0.1" "   route to: 127.0.0.1
destination: 127.0.0.0
       mask: 255.0.0.0
  interface: lo0
      flags: <UP>" 0 "$R" get 127.0.0.1
if [ "$(id -u)" != 0 ]; then
    done_tests route
    exit
fi
cd /tmp || exit 2
I=$PROG_DIR/../ifconfig/ifconfig
[ -x "$I" ] || I=ifconfig
NIC=$("$I" -l | tr ' ' '\n' | sed -n 2p)
[ -n "$NIC" ] || { done_tests route; exit; }
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
T=/var/tmp/test_route.$$
mkdir -m 755 "$T" || exit 2
trap 'rm -rf "$T"; "$I" "$NIC" -alias 10.55.0.1 >/dev/null 2>&1' EXIT
GW=$("$R" show | awk '$1 == "default" { print $2 }')
has() { "$R" show | grep -E -- "$1" >/dev/null; }

"$I" "$NIC" 10.55.0.1/24 alias
t "a network through a gateway" "add net 10.56.0.0/16: gateway 10.55.0.9" 0 "$R" add 10.56.0.0/16 10.55.0.9
ok "it is in the table" has '^10\.56/16 +10\.55\.0\.9 +UG +0 +'"$NIC"'$'
t "get through it" "   route to: 10.56.3.4
destination: 10.56.0.0
       mask: 255.255.0.0
    gateway: 10.55.0.9
  interface: $NIC
      flags: <UP,GATEWAY>" 0 "$R" get 10.56.3.4
t "again" "route: add net 10.56.0.0/16: gateway 10.55.0.9: File exists" 1 sh -c '"$1" add 10.56.0.0/16 10.55.0.9 2>&1' sh "$R"
t "-q is quiet" "" 0 "$R" -q add 10.57.0.0/16 10.55.0.9
t "-netmask" "add net 10.58.0.0: gateway 10.55.0.9" 0 "$R" add -net 10.58.0.0 -netmask 255.255.0.0 10.55.0.9
ok "its mask" has '^10\.58/16 +10\.55\.0\.9 +UG '
t "-netmask in hex" "add net 10.59.0.0: gateway 10.55.0.9" 0 "$R" add -net 10.59.0.0 -netmask 0xffff0000 10.55.0.9
t "-netmask as a prefix" "add net 10.60.0.0: gateway 10.55.0.9" 0 "$R" add -net 10.60.0.0 -netmask /16 10.55.0.9
ok "-net alone gets the class' mask" sh -c '"$1" add -net 11.0.0.0 10.55.0.9 >/dev/null && "$1" show | grep -E "^11/8 +10\.55\.0\.9 +UG " >/dev/null' sh "$R"
t "a host" "add host 192.0.2.9: gateway 10.55.0.9" 0 "$R" add 192.0.2.9 10.55.0.9
ok "it has the H flag" has '^192\.0\.2\.9 +10\.55\.0\.9 +UGH '
t "-host" "add host 192.0.2.10: gateway 10.55.0.9" 0 "$R" add -host 192.0.2.10 10.55.0.9
t "a priority" "add net 10.61.0.0/16: gateway 10.55.0.9" 0 "$R" add -priority 5 10.61.0.0/16 10.55.0.9
ok "the metric shows" has '^10\.61/16 +10\.55\.0\.9 +UG +5 '
t "an interface route" "add net 10.62.0.0/16: gateway $NIC" 0 "$R" add -net 10.62.0.0/16 -iface "$NIC"
ok "on the link" has '^10\.62/16 +link#2 +U +0 '
t "the card's address as the interface" "add net 10.63.0.0/16: gateway 10.55.0.1" 0 "$R" add 10.63.0.0/16 -iface 10.55.0.1
ok "(the same card)" has '^10\.63/16 +link#2 +U '
t "-ifp" "add net 10.64.0.0/16: gateway 10.55.0.9" 0 "$R" add 10.64.0.0/16 10.55.0.9 -ifp "$NIC"
t "an interface that is not there" "route: no such interface or address: nope0" 1 sh -c '"$1" add 10.65.0.0/16 -iface nope0 2>&1' sh "$R"
t "-ifp that is not there" "route: add net 10.65.0.0/16: gateway 10.55.0.9: No such device" 1 sh -c '"$1" add 10.65.0.0/16 10.55.0.9 -ifp nope0 2>&1' sh "$R"
t "a gateway that is not on a network of the card" "route: add net 10.66.0.0/16: gateway 10.200.0.1: Network is unreachable" 1 sh -c '"$1" add 10.66.0.0/16 10.200.0.1 2>&1' sh "$R"
t "a bad gateway" "route: invalid gateway: gw" 1 sh -c '"$1" add 10.67.0.0/16 gw 2>&1' sh "$R"
t "a bad destination" "route: invalid destination: 10.67.0" 1 sh -c '"$1" add 10.67.0 10.55.0.9 2>&1' sh "$R"
t "a bad mask" "route: invalid netmask: 255.0" 1 sh -c '"$1" add -net 10.67.0.0 -netmask 255.0 10.55.0.9 2>&1' sh "$R"
t "a mask that is not a prefix" "route: add net 10.67.0.0: gateway 10.55.0.9: Invalid argument" 1 sh -c '"$1" add -net 10.67.0.0 -netmask 255.0.255.0 10.55.0.9 2>&1' sh "$R"
t "-host with a mask" "$U" 1 usage1 "$R" add -host 10.67.0.1 -netmask 255.255.0.0 10.55.0.9
t "no gateway" "$U" 1 usage1 "$R" add 10.67.0.0/16
t "a bad priority" "route: invalid priority: -1" 1 sh -c '"$1" add 10.67.0.0/16 10.55.0.9 -priority -1 2>&1' sh "$R"
t "get finds the longest prefix" "10.56.0.0" 0 sh -c '"$1" get 10.56.9.9 | sed -n "s/^destination: //p"' sh "$R"
t "delete" "delete net 10.56.0.0/16" 0 "$R" delete 10.56.0.0/16
ok "it is gone" sh -c '! "$1" show | grep -E "^10\.56/16 " >/dev/null' sh "$R"
t "delete again" "route: delete net 10.56.0.0/16: No such process" 1 sh -c '"$1" delete 10.56.0.0/16 2>&1' sh "$R"
t "delete a host" "delete host 192.0.2.9" 0 "$R" delete 192.0.2.9
t "delete with -netmask" "delete net 10.58.0.0" 0 "$R" delete -net 10.58.0.0 -netmask 255.255.0.0
t "delete is quiet with -q" "" 0 "$R" -q delete 10.57.0.0/16
t "del" "delete net 10.59.0.0" 0 "$R" del -net 10.59.0.0 -netmask /16
# the default route
"$R" -q delete default 2>/dev/null
t "no default: get fails" "route: get host 10.99.9.9: Network is unreachable" 1 sh -c '"$1" get 10.99.9.9 2>&1' sh "$R"
t "a default route" "add net default: gateway 10.55.0.9" 0 "$R" add default 10.55.0.9
ok "it shows" has '^default +10\.55\.0\.9 +UG '
t "a second one" "route: add net default: gateway 10.55.0.8: File exists" 1 sh -c '"$1" add default 10.55.0.8 2>&1' sh "$R"
t "get through it" "10.55.0.9" 0 sh -c '"$1" get 10.99.9.9 | sed -n "s/^    gateway: //p"' sh "$R"
t "the default route's mask" "default" 0 sh -c '"$1" get 10.99.9.9 | sed -n "s/^       mask: //p"' sh "$R"
t "delete it" "delete net default" 0 "$R" delete default

# down interface: its routes are not used
"$R" -q add 10.68.0.0/16 10.55.0.9
"$I" "$NIC" down
t "a route through a card that is down is not found" "route: get host 10.68.0.1: Network is unreachable" 1 sh -c '"$1" get 10.68.0.1 2>&1' sh "$R"
"$I" "$NIC" up
t "...and found again" "10.55.0.9" 0 sh -c '"$1" get 10.68.0.1 | sed -n "s/^    gateway: //p"' sh "$R"

# flush
"$R" -q add 10.69.0.0/16 10.55.0.9
"$R" -q add 192.0.2.77 10.55.0.9
"$R" -q add default 10.55.0.9
ok "flush says what it removes" sh -c '"$1" flush | grep -F "delete net 10.69/16: gateway 10.55.0.9" >/dev/null' sh "$R"
ok "nothing through a gateway is left" sh -c '! "$1" show | grep -E "^[^ ]+ +[0-9.]+ +UG" >/dev/null' sh "$R"
ok "the networks of the card are" has '^10\.55\.0/24 +link#2 +U '
ok "and the loopback's" has '^127/8 +link#1 +U '
t "flush with nothing to flush" "" 0 "$R" flush
"$I" "$NIC" -alias 10.55.0.1
if [ -n "$GW" ]; then
    "$R" -q add default "$GW"
    ok "the default route is back" has "^default +$GW +UG "
fi

# who may do what
if $cc -o "$T/asuser" ../../lib/testlib/asuser.c 2>/dev/null; then
    t "anyone may look" "$("$R" show)" 0 "$T/asuser" 65534 65534 "$R" show
    t "anyone may ask" "$("$R" get 127.0.0.1)" 0 "$T/asuser" 65534 65534 "$R" get 127.0.0.1
    t "only root may add" "route: add net 10.70.0.0/16: gateway $GW: Operation not permitted" 1 sh -c '"$1" 65534 65534 "$2" add 10.70.0.0/16 "$3" 2>&1' sh "$T/asuser" "$R" "$GW"
fi
done_tests route
