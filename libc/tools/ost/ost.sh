#!/bin/sh
# ost.sh - Sortix's os-test (https://gitlab.com/sortix/os-test; ISC
# licensed, used from outside the tree) with POSIX sh alone: its
# makefiles need GNU or BSD make.  It does what they do, with its own
# misc/compile.sh, misc/try-compile.sh and misc/namespace.c:
#   sh ost.sh build [suite...]   compile (include and namespace: the
#                                whole suite, they only compile)
#   sh ost.sh run [suite...]     run what was compiled
# Run it in the os-test directory.  Outcomes go to out/<os>/<suite>/
# <test>.out, as with "make test" (misc/html makes the report of them); a
# test whose .out (or, building, whose program) exists is not done again,
# so a run cut short continues where it stopped.
#   OST_CC   the compiler command (default: cc beside this script, which
#            turns gcc's options into c17's; not $CC, which Shall's
#            /etc/profile sets to c17 itself)
#   OS       the system's name (default: Shall on Shall, whose uname -s
#            says Linux, else uname -s); out/ in lower case
#   TIMEOUT  seconds per test (default 120)
#   SKIP     a file of suite/test lines not to run (they stop the system)
#   NO_GETIFADDRS  udp/udp.h finds the LAN with getifaddrs(), which is
#            not POSIX (Shall does not have it), except on some systems:
#            set (the default on Shall), udp.h is changed to take
#            NO_GETIFADDRS as one of them, once, and it is defined for the
#            udp suite
# For a diagnostic run only, OST_SHIM=1 compiles with shim.h, which gives
# the tests the usleep() and gettimeofday() that POSIX removed.
tools=$(cd "$(dirname "$0")" && pwd)
CC=${OST_CC:-$tools/cc}
case $(uname -r) in
*shall*) OS=${OS:-Shall} NO_GETIFADDRS=${NO_GETIFADDRS-1} ;;
*) OS=${OS:-$(uname -s)} ;;
esac
TIMEOUT=${TIMEOUT:-120}
os=$(echo "$OS" | tr '[:upper:]' '[:lower:]')
top=$(pwd)
CFLAGS="-pthread"
LDFLAGS="-lm -lpthread -lrt"
EXTRA=

phase=$1; shift
[ $# -gt 0 ] || set -- $(cat misc/suites.list)

tests() { find . -name '*.c' | sed 's,^\./,,; s,\.c$,,' | sort; }

build_suite() {
	s=$1 out=$top/out/$os/$1
	cd "$top/$s" || return
	case $s in
	include)
		for t in $(tests); do
			[ -f "$out/$t.out" ] && continue
			../misc/try-compile.sh "$CC $CFLAGS" "$t" "$LDFLAGS" "$EXTRA" "$OS" "$out"
		done ;;
	namespace)
		[ -x ../misc/namespace ] || $CC ../misc/namespace.c -o ../misc/namespace $LDFLAGS || return
		for t in $(tests); do
			[ -f "$out/$t.out" ] && continue
			for k in i dM; do
				f=E; [ $k = dM ] && f="E -dM"
				$CC $CFLAGS -D_POSIX_C_SOURCE=202405L -std=c17 -$f "$t.c" -o "$t.$k" 2>/dev/null ||
					echo missing_header > "$t.$k"
			done
			case $t in
			*-xsi) ../misc/namespace -o "$out/$t.out" -e "$out/$t.err" -I ../include \
				-i "${t%-xsi}" -x "$t.i" "$t.dM" ;;
			*) ../misc/namespace -o "$out/$t.out" -e "$out/$t.err" -I ../include \
				-i "$t" "$t.i" "$t.dM" ;;
			esac
		done ;;
	*)
		# udp/udp.h finds the LAN with getifaddrs() (not POSIX) except on
		# some systems; udp.h is patched to take NO_GETIFADDRS as one of them
		f=
		if [ $s = udp ] && [ -n "$NO_GETIFADDRS" ]; then
			f=-DNO_GETIFADDRS
			grep -q NO_GETIFADDRS udp.h ||
				{ sed 's/!defined(__redox__)$/& \&\& !defined(NO_GETIFADDRS)/' udp.h > udp.h.new &&
				  mv udp.h.new udp.h; } || return
		fi
		for t in $(tests); do
			[ -x "$t" ] && continue
			../misc/compile.sh "$CC $CFLAGS $f" "$t" "$LDFLAGS" "$EXTRA" "$OS" "$out"
		done
		if [ $s = basic ]; then
			for t in dlfcn/dlclose dlfcn/dlopen dlfcn/dlsym; do
				$CC -shared -fPIC $CFLAGS -DSHARED "$t.c" -o "$t.so" $LDFLAGS
			done
		fi ;;
	esac
}

run_suite() {
	s=$1 out=$top/out/$os/$1
	case $s in include|namespace) return ;; esac
	cd "$top/$s" || return
	for t in $(tests); do
		[ -f "$out/$t.out" ] && continue
		[ -x "$t" ] || continue
		mkdir -p "$(dirname "$out/$t.out")"
		# SKIP: a file of suite/test lines not to run (they stopped the
		# whole system, not just themselves)
		if [ -n "$SKIP" ] && grep -qx "$s/$t" "$SKIP" 2>/dev/null; then
			echo "skipped: stops the system" > "$out/$t.out"
			continue
		fi
		echo "$s/$t" > "$top/out/$os/.current"
		timeout -s KILL "$TIMEOUT" "./$t" > "$out/$t.out.new" 2>&1 < /dev/null
		code=$?
		if [ ! -s "$out/$t.out.new" ] || [ "$code" -ge 2 ]; then
			echo "exit: $code" >> "$out/$t.out.new"
		fi
		mv "$out/$t.out.new" "$out/$t.out"
	done
}

case $phase in
build)
	mkdir -p "out/$os"
	[ -f "out/$os/uname.out" ] || misc/uname.sh > "out/$os/uname.out"
	for s do echo "build $s"; build_suite "$s"; cd "$top"; done ;;
run)
	# the system the tests ran on (build may have been in a chroot)
	mkdir -p "out/$os" && misc/uname.sh > "out/$os/uname.out"
	for s do echo "run $s"; run_suite "$s"; cd "$top"; done
	rm -f "out/$os/.current" ;;
*) echo "usage: sh ost.sh build|run [suite...]" >&2; exit 2 ;;
esac
