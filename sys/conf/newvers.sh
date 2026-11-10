#!/bin/sh
# newvers.sh - write vers.c, the identity of a kernel: ostype, osrelease,
# osversion and the banner that the kernel prints first.
#
#   sh newvers.sh [CONF]        (in the compile directory; CONF: GENERIC)
#
# The banner names who built the kernel, as OpenBSD's does:
#
#   Virtual/86 BSD 0.9.1 (GENERIC) #1: Tue Nov 10 00:00:00 UTC 2026
#       user@host:/usr/src/sys/arch/amd64/compile/GENERIC
#
# user@host is that of the machine building ($(id -un)@$(uname -n)); BUILDER
# names another (the reproducibility checks pass the same one to every
# build).  The date is the one in RELDATE, fixed for the release: the
# clock never gets into a kernel.  The directory is always the one under
# /usr/src, where the kernel is built inside the system.
#
# The release is the one number set here: it is osrelease, uname -r.

fullname="Virtual/86 BSD"
ost=V86BSD
osr=0.9.1
build=1
machine=amd64

here=$(dirname "$0")
conf=${1:-GENERIC}
reldate=$(cat "$here/RELDATE") || exit 1
who=${BUILDER:-$(id -un)@$(uname -n)}
# the banner is a C string: only what a user and a host name can hold
who=$(printf '%s' "$who" | tr -cd 'A-Za-z0-9@._-')
[ -n "$who" ] || who=nobody@localhost
dir=/usr/src/sys/arch/$machine/compile/$conf

cat > vers.c.new <<EOT
/* vers.c: made by newvers.sh; not to be edited */
const char ostype[] = "$ost";
const char osrelease[] = "$osr";
const char osversion[] = "$conf#$build";
const char version[] = "$fullname $osr ($conf) #$build: $reldate\\n    $who:$dir\\n";
EOT
mv vers.c.new vers.c
