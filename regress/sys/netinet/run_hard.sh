#!/bin/sh
# run_hard.sh - the network hardening tests: boots a V86BSD guest in QEMU
# with a second network card wired to nethard (this directory), which plays
# a hostile peer on it (forged sources, source routes, overlapping
# fragments, SYN floods, blind resets ...).  Run from the top of the tree
# after `make release` (it uses obj/root); on the host, as a user.
#
#   sh regress/sys/netinet/run_hard.sh [-k kernel] [-m] [boot.sh flags]
#
#   -k kernel  boot this kernel instead of obj/root/bsd
#   -m         boot with the virtio card as the first one (-v), no other flags
#
# The guest gets 10.9.0.2 on em1 from /etc/hostname.em1 (so that /etc/netstart
# is part of the run), hardsrv answers there, and the log of the boot is
# printed when it is over.  The result is nethard's: "N passed, M failed".
set -u
cd "$(dirname "$0")/../../.." || exit 2
TOP=$(pwd)
ROOT=${OBJDIR:-obj}/root
kernel=
while getopts k: opt; do
	case $opt in
	k) kernel=$OPTARG ;;
	*) echo "usage: sh regress/sys/netinet/run_hard.sh [-k kernel] [boot.sh flags]" >&2; exit 2 ;;
	esac
done
shift $((OPTIND - 1))
[ -d "$ROOT" ] || { echo "run_hard: no $ROOT: make release first" >&2; exit 2; }
T=$(mktemp -d "${TMPDIR:-/tmp}/run_hard.XXXXXX") || exit 2
trap 'kill $nh 2>/dev/null; rm -rf "$T"' EXIT
cc -O1 -o "$T/nethard" regress/sys/netinet/nethard.c || exit 2
cp -a "$ROOT" "$T/root"
[ -n "$kernel" ] && cp "$kernel" "$T/root/bsd"
mkdir -p "$T/root/usr/src/regress/sys/netinet"
cp regress/sys/netinet/hardsrv.c "$T/root/usr/src/regress/sys/netinet/"
printf 'inet 10.9.0.2 netmask 255.255.255.0\n' > "$T/root/etc/hostname.em1"
cat > "$T/root/etc/rc.local" <<'EOT'
cd /usr/src/regress/sys/netinet && c17 -o /var/tmp/hardsrv hardsrv.c && /var/tmp/hardsrv
/sbin/halt -p
EOT
sh mkdisk.sh -s 2048 "$T/root" "$T/img" > "$T/mkdisk.log" 2>&1 || { cat "$T/mkdisk.log"; exit 2; }
port=$((20000 + $$ % 20000))
"$T/nethard" $port > "$T/result" 2>&1 &
nh=$!
sleep 1
sh distrib/boot.sh "$@" -k "$T/root/bsd" "$T/img" -- \
	-netdev socket,id=h0,connect=127.0.0.1:$port -device e1000,netdev=h0,mac=52:54:00:12:34:99 \
	< /dev/null > "$T/boot.log" 2>&1
wait $nh
st=$?
cat "$T/result"
[ -n "${HARD_LOG:-}" ] && cp "$T/boot.log" "$HARD_LOG"
[ $st = 0 ] || { echo "--- the guest's console:"; tr -d '\r' < "$T/boot.log" | tail -40; }
exit $st
