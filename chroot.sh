#!/bin/sh
# chroot.sh - enter a Shall system made by bootstrap.sh, with or without
# root privileges.
#
# Usage: sh chroot.sh [-m method] [root [command [argument...]]]
#
#   root       the system (default: root/ beside this script)
#   command    run it instead of an interactive sh
#   -m method  how to get in, instead of the first that works:
#                chroot   as root: chroot(1), with the host's /proc, /sys
#                         and /dev mounted in root while inside
#                unshare  as any user: a user namespace (unshare -r, from
#                         util-linux) in which you are root, with a mount
#                         and a process namespace of its own (its own
#                         /proc); nothing is visible outside it, and
#                         nothing is left mounted afterwards
#                bwrap    as any user: bubblewrap, if installed
#                proot    as any user: PRoot (ptrace; no namespaces, so it
#                         works where user namespaces are not allowed;
#                         slower), if installed
#
# Without privileges, build root as yourself (sh bootstrap.sh, no sudo):
# inside a user namespace your own files belong to root, other users'
# files to nobody.  Ubuntu 23.10 and later may refuse user namespaces to
# unprivileged programs (AppArmor); then bwrap or proot is used, or
#   sudo sysctl kernel.apparmor_restrict_unprivileged_userns=0
# allows unshare until the next boot.
#
# Inside, root is also a mount point (so that the mount table has "/":
# df); the host's /etc/resolv.conf (name lookups) and /etc/localtime are
# copied in.  The environment is a fresh one: HOME, TERM, PATH,
# C17_LIBC=shall, and ENV=/etc/profile, which sh reads.

usage() { echo "usage: sh chroot.sh [-m chroot|unshare|bwrap|proot] [root [command...]]" >&2; exit 2; }

method=
while getopts m: opt; do
	case $opt in
	m) method=$OPTARG ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
case $method in ""|chroot|unshare|bwrap|proot) ;; *) usage ;; esac

root=${1:-$(dirname "$0")/root}
[ $# -gt 0 ] && shift
[ -x "$root/usr/bin/sh" ] || { echo "chroot.sh: $root: no system there (sh bootstrap.sh $root)" >&2; exit 1; }
root=$(cd "$root" && pwd)

# ---- inside the namespaces of unshare: chroot.sh runs itself there, the
# command already made ----
if [ "${SHALL_INSIDE:-}" = unshare ]; then
	unset SHALL_INSIDE
	# root as a mount point, with the /proc that unshare mounted in it
	mount --rbind "$root" "$root" 2>/dev/null
	mount --rbind /dev "$root/dev" || echo "chroot.sh: warning: no /dev" >&2
	mount --rbind /sys "$root/sys" 2>/dev/null
	exec chroot "$root" "$@"
fi

# the command run inside: a fresh environment
if [ $# -gt 0 ]; then
	set -- /usr/bin/env -i HOME=/root TERM="${TERM:-vt100}" PATH=/usr/local/bin:/usr/bin:/bin \
		C17_LIBC=shall ENV=/etc/profile "$@"
else
	set -- /usr/bin/env -i HOME=/root TERM="${TERM:-vt100}" PATH=/usr/local/bin:/usr/bin:/bin \
		C17_LIBC=shall ENV=/etc/profile /bin/sh -i
fi

# the files the host lends
[ -f /etc/resolv.conf ] && cp /etc/resolv.conf "$root/etc/resolv.conf" 2>/dev/null
[ -f /etc/localtime ] && [ ! -e "$root/etc/localtime" ] && cp /etc/localtime "$root/etc/localtime" 2>/dev/null

uid=$(id -u)
have() { command -v "$1" >/dev/null 2>&1; }
# can unshare give us a user namespace with mounts in it?
unshare_works() {
	have unshare && unshare -r -m -p -f sh -c 'mount -t tmpfs none /tmp' >/dev/null 2>&1
}

if [ -z "$method" ]; then
	if [ "$uid" = 0 ]; then method=chroot
	elif unshare_works; then method=unshare
	elif have bwrap; then method=bwrap
	elif have proot; then method=proot
	else
		echo "chroot.sh: not root, user namespaces are not allowed (unshare -r failed)," >&2
		echo "and neither bwrap nor proot is installed; see the head of this script" >&2
		exit 1
	fi
fi
if [ "$uid" != 0 ] && [ ! -O "$root/usr/bin/sh" ]; then
	echo "chroot.sh: warning: $root belongs to another user; inside, its files belong to nobody" >&2
fi

case $method in
chroot)
	[ "$uid" = 0 ] || { echo "chroot.sh: only root can chroot (try -m unshare)" >&2; exit 1; }
	mounted=
	unmount() {
		for m in $mounted; do umount "$m" 2>/dev/null || umount -l "$m" 2>/dev/null; done
		mounted=
	}
	trap 'unmount' EXIT
	trap 'exit 130' INT TERM HUP
	mnt() {   # mnt TYPE-OR-bind SOURCE DIR
		mountpoint -q "$3" 2>/dev/null && return 0
		mkdir -p "$3"
		if [ "$1" = bind ]; then mount --bind "$2" "$3"; else mount -t "$1" "$2" "$3"; fi &&
			mounted="$3 $mounted"
	}
	mnt bind "$root" "$root" 2>/dev/null
	mnt proc proc "$root/proc" || echo "chroot.sh: warning: no /proc (ps, df, who need it)" >&2
	mnt bind /sys "$root/sys" 2>/dev/null
	mnt bind /dev "$root/dev" || echo "chroot.sh: warning: no /dev" >&2
	[ -d /dev/pts ] && mnt bind /dev/pts "$root/dev/pts" 2>/dev/null
	chroot "$root" "$@"
	;;
unshare)
	SHALL_INSIDE=unshare exec unshare -r -m -p -f --mount-proc="$root/proc" \
		sh "$0" "$root" "$@"
	;;
bwrap)
	have bwrap || { echo "chroot.sh: no bwrap" >&2; exit 1; }
	exec bwrap --bind "$root" / --dev /dev --proc /proc --ro-bind /sys /sys \
		--unshare-user --uid 0 --gid 0 --unshare-pid --unshare-ipc --unshare-uts \
		--die-with-parent --chdir /root "$@"
	;;
proot)
	have proot || { echo "chroot.sh: no proot" >&2; exit 1; }
	exec proot -0 -r "$root" -b /dev -b /proc -b /sys -w /root "$@"
	;;
esac
