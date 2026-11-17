#!/bin/sh
# build-freebsd.sh [-s SRCDIR] [-o OBJDIR] [--] make.py-words...: run FreeBSD's make.py
# on this host. The source root FreeBSD expects is made of hard links to sys/,
# stand/ and freebsd/ in SRCDIR. The word "loader" builds stand/ (loader, gptboot,
# pmbr, loader.efi) instead of a make.py target.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
top=$(dirname "$here")
srcdir=$top
objdir=$top/obj/freebsd
while getopts s:o: c; do
	case $c in
	s) srcdir=$(cd "$OPTARG" && pwd) ;;
	o) objdir=$OPTARG ;;
	*) exit 2 ;;
	esac
done
shift $((OPTIND - 1))

mkdir -p "$objdir/obj"
root=$objdir/src
if [ ! -f "$root/.farm" ] || [ "$srcdir/freebsd/IMPORTED" -nt "$root/.farm" ]; then
	rm -rf "$root"
	mkdir -p "$root"
	cp -al "$srcdir/sys" "$srcdir/stand" "$root/"
	cp -al "$srcdir"/freebsd/. "$root/"
	touch "$root/.farm"
else
	rsync -a --delete --link-dest="$srcdir/sys/" "$srcdir/sys/" "$root/sys/"
	rsync -a --delete --link-dest="$srcdir/stand/" "$srcdir/stand/" "$root/stand/"
fi

cd "$root"
export MAKEOBJDIRPREFIX=$objdir/obj
mk() {
	python3 -I tools/build/make.py --cross-bindir=/usr/bin \
	    TARGET=amd64 TARGET_ARCH=amd64 "$@"
}
if [ "${1:-}" = loader ]; then
	shift
	# only the headers stand/ needs; a full _includes walks every library
	cat > "$objdir/stand-build.sh" <<EOT
#!/bin/sh
set -e
for d in include sys lib/libc lib/libsys; do
	(cd "$root/\$d" && bmake obj includes)
done
cd "$root/stand"
# -k: stand/userboot/test needs a hosted libc and is not wanted
bmake -k -j$(nproc) obj all
EOT
	mk "$@" WITHOUT_CLANG=yes "DESTDIR=$objdir/obj$root/amd64.amd64/tmp" buildenv BUILDENV_DIR="$root" "BUILDENV_SHELL=sh $objdir/stand-build.sh" || true
	exit 0
fi
exec python3 -I tools/build/make.py --cross-bindir=/usr/bin \
	TARGET=amd64 TARGET_ARCH=amd64 "$@"
