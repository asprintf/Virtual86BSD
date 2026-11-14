#!/bin/sh
# build.sh - build the x86_64-v86bsd cross toolchain (GNU binutils and gcc,
# C and C++) on the host (see HOWTO.md beside it).
#
# Usage: sh build.sh [-o objdir] [-j jobs] [-t]
#
#   -o objdir   where the world is built and the toolchain goes (default
#               obj/ beside distrib/); the world, objdir/world, is the sysroot:
#               make buildworld first
#   -j jobs     parallel jobs (default: the number of processors)
#   -t          also cross-compile the programs of tests/ into objdir/toolchain/tests
#
# On the host: a C++ compiler, make, patch, tar with xz, curl or wget, the
# GMP, MPFR and MPC headers and libraries.  The sources are downloaded to
# objdir/toolchain/dist (and their checksums checked), patched with the two
# .diff files here and built in objdir/toolchain/build-*; the tools land in
# objdir/toolchain/prefix/bin as x86_64-v86bsd-gcc, -g++, -ld, -ar and so on.

GCC=16.2.0 BINUTILS=2.47
GCC_SHA=e6738e29597f733270731aa90600f37ffdc045079dfc27ec7e8192cc81085c3e
BINUTILS_SHA=154ab23b60070e8f27013c22977f1129425d67d1e8acd6e13010e617811e4cff
TARGET=x86_64-v86bsd

usage() { echo "usage: sh build.sh [-o objdir] [-j jobs] [-t]" >&2; exit 2; }
fail() { echo "build.sh: $*" >&2; exit 1; }

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
obj=$root/obj jobs= test=no
while getopts o:j:t opt; do
	case $opt in
	o) obj=$OPTARG ;;
	j) jobs=$OPTARG ;;
	t) test=yes ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ $# -eq 0 ] || usage
case $obj in /*) ;; *) obj=$(pwd)/$obj ;; esac
[ -n "$jobs" ] || jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
work=$obj/toolchain dist=$work/dist src=$work/src prefix=$work/prefix sysroot=$obj/world
[ -d "$sysroot/usr/include" ] && [ -f "$sysroot/usr/lib/libc.a" ] ||
	fail "no world in $obj: make buildworld first (it is the sysroot)"
mkdir -p "$dist" "$src" || exit 1
unset MAKEFLAGS MFLAGS

sum() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }
fetch() { # fetch URL FILE SHA256
	[ -f "$dist/$2" ] || {
		echo "== fetching $2"
		if command -v curl >/dev/null 2>&1; then curl -fsSL -o "$dist/$2" "$1"; else wget -q -O "$dist/$2" "$1"; fi ||
			fail "cannot fetch $1"
	}
	[ "$(sum "$dist/$2")" = "$3" ] || fail "$2: wrong checksum (remove it from $dist)"
}
unpack() { # unpack TARBALL DIR DIFF
	[ -f "$src/$2/.v86bsd" ] && return 0
	echo "== unpacking and patching $2"
	rm -rf "${src:?}/$2"
	tar -C "$src" -xf "$dist/$1" || fail "cannot unpack $1"
	(cd "$src/$2" && patch -s -p1 < "$here/$3") || fail "$3 does not apply"
	: > "$src/$2/.v86bsd"
}

fetch "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS.tar.xz" "binutils-$BINUTILS.tar.xz" "$BINUTILS_SHA"
fetch "https://ftp.gnu.org/gnu/gcc/gcc-$GCC/gcc-$GCC.tar.xz" "gcc-$GCC.tar.xz" "$GCC_SHA"
unpack "binutils-$BINUTILS.tar.xz" "binutils-$BINUTILS" binutils-$BINUTILS.diff
unpack "gcc-$GCC.tar.xz" "gcc-$GCC" gcc-$GCC.diff

PATH=$prefix/bin:$PATH
export PATH

step() { # step DIR LOGNAME COMMAND...
	d=$1 l=$2; shift 2
	echo "== $l"
	(cd "$d" && "$@") > "$d/$l.log" 2>&1 || fail "$l failed (see $d/$l.log)"
}

mkdir -p "$work/build-binutils" "$work/build-gcc"
[ -f "$work/build-binutils/Makefile" ] ||
	step "$work/build-binutils" configure-binutils "$src/binutils-$BINUTILS/configure" \
		--target=$TARGET --prefix="$prefix" --with-sysroot="$sysroot" --disable-nls --disable-werror \
		--disable-gprofng --disable-multilib --enable-deterministic-archives
step "$work/build-binutils" make-binutils make -j"$jobs"
step "$work/build-binutils" install-binutils make install

[ -f "$work/build-gcc/Makefile" ] ||
	step "$work/build-gcc" configure-gcc "$src/gcc-$GCC/configure" \
		--target=$TARGET --prefix="$prefix" --with-sysroot="$sysroot" \
		--with-native-system-header-dir=/usr/include --disable-multilib --disable-nls \
		--disable-bootstrap --enable-languages=c,c++ --disable-shared --enable-threads=posix \
		--disable-libssp --disable-libgomp --disable-libquadmath --disable-libatomic \
		--disable-libsanitizer --disable-libitm --disable-libvtv --disable-lto --disable-plugin \
		--enable-initfini-array --with-gmp=/usr --with-mpfr=/usr --with-mpc=/usr
step "$work/build-gcc" make-gcc make -j"$jobs" all-gcc
step "$work/build-gcc" make-libgcc make -j"$jobs" all-target-libgcc
step "$work/build-gcc" make-libstdc++ make -j"$jobs" all-target-libstdc++-v3
step "$work/build-gcc" install-gcc make install-gcc install-target-libgcc install-target-libstdc++-v3

if [ "$test" = yes ]; then
	t=$work/tests
	mkdir -p "$t" || exit 1
	echo "== tests: cross-compiling into $t"
	for m in static dynamic; do
		f=; [ $m = static ] && f=-static
		$TARGET-gcc -std=c17 -D_DEFAULT_SOURCE= -O2 -Wall -Wextra $f -o "$t/hello-$m" "$here/tests/hello.c" || fail "hello.c"
		$TARGET-gcc -std=c17 -D_DEFAULT_SOURCE= -O2 -Wall -Wextra $f -o "$t/rt-$m" "$here/tests/rt.c" -lpthread || fail "rt.c"
		$TARGET-g++ -std=c++17 -O2 -Wall $f -o "$t/rt-cxx-$m" "$here/tests/rt.cc" || fail "rt.cc"
	done
	echo "== run them inside V86BSD (HOWTO.md, 'Test'); their output is in tests/expect"
fi
echo "== done: $prefix/bin/$TARGET-gcc"
