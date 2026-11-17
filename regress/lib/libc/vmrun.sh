#!/bin/sh
# vmrun.sh - run the libc tests (run.sh's) on the FreeBSD kernel in QEMU.
#   sh vmrun.sh [-d] [-s smp] [-n] [TEST...]
# The library is built for the FreeBSD ABI (make in lib/libc), the tests with clang
# (FreeBSD target, lld) against it (static, or -d: dynamic, libc.so is the interpreter),
# the references with the host's compiler as run.sh does; the tests run in the VM
# (vminit.c is its /sbin/init) and their output is compared here:
#   t_*.c  the output must equal the reference's (its length and hash: the serial
#          line cannot carry megabytes)
#   u_*.c  run with -c and the reference's output as input: status 0
#   s_*.c  status 0 and no FAIL line
# out/vm/ has everything: img, log, the outputs.  -n builds only.
cd "$(dirname "$0")" || exit 1
here=$(pwd)
top=$(cd ../../.. && pwd)
mode=static smp=1 build_only= dbg=
while getopts ds:ng opt; do
	case $opt in d) mode=dynamic ;; s) smp=$OPTARG ;; n) build_only=1 ;; g) dbg=$here/segvdbg.c ;; *) exit 2 ;; esac
done
shift $((OPTIND - 1))
want=$*
out=$here/out/vm-$mode
B=${LIBC_BUILD:-$out/libc}
X=${XCC:-clang}
refcc=${REFCC:-clang}
rm -rf "$out/t" "$out/ref"
mkdir -p "$out"
clang -O2 -o "$out/fnv" "$here/fnv.c" || exit 1
mkdir -p "$out/t" "$out/ref" "$out/ov/sbin" "$out/ov/tmp" "$out/ov/t" "$out/ov/var/run"
(cd "$top/lib/libc" && make -s -j12 B="$B" all) > "$out/libc.log" 2>&1 || { tail "$out/libc.log"; exit 1; }
cc="sh $top/lib/libc/tools/libc-cc $X"
export LIBC_LIB=$B/lib LIBC_LINK=$mode
[ $mode = dynamic ] && LIBC_INTERP=/lib/libc.so && export LIBC_INTERP
std=-std=gnu17
pic=; [ $mode = dynamic ] && pic=-fPIE
# init is static in both modes: a dynamic test that fails cannot take the harness with it
LIBC_LINK=static $cc -O2 -w -o "$out/ov/sbin/init" vminit.c || exit 1
# the tests that run commands (system, popen, posix_spawn ...) need /bin/sh: ours, built
# on this library (yacc and lex of the tree, built for the host, make its parsers)
if [ ! -x "$out/sh/sh" ] || [ "$B/lib/libc.a" -nt "$out/sh/sh" ]; then
	rm -rf "$out/sh" "$out/yl"; mkdir -p "$out/sh" "$out/yl"
	cp -R "$top/bin/sh/." "$out/sh/"
	for d in yacc lex; do cp -R "$top/usr.bin/$d" "$out/yl/$d"; (cd "$out/yl/$d" && make -s CC=clang) > /dev/null 2>&1; done
	(cd "$out/sh" && make -s YACC="$out/yl/yacc/yacc" LEX="$out/yl/lex/lex" CC="$cc" CFLAGS="-O2 -w -D_DEFAULT_SOURCE $pic") > "$out/sh.log" 2>&1 || { tail "$out/sh.log"; exit 1; }
fi
mkdir -p "$out/ov/bin"; cp "$out/sh/sh" "$out/ov/bin/sh"
# and the few commands the tests run through it (our own, built the same way)
for p in rm cat mkdir rmdir echo kill sleep test printf true false env pwd chmod; do
	src=$top/bin/$p; [ -d "$src" ] || src=$top/usr.bin/$p
	rm -rf "$out/cmd-$p"; cp -R "$src" "$out/cmd-$p"
	(cd "$out/cmd-$p" && make -s CC="$cc" CFLAGS="-O2 -w -D_DEFAULT_SOURCE $pic") > "$out/cmd-$p.log" 2>&1 &&
		cp "$out/cmd-$p/$p" "$out/ov/bin/$p" || echo "vmrun: $p not built (see $out/cmd-$p.log)"
done
LOCPATH=/t/loc; TEST_LOCALES=$(ls out/locales 2>/dev/null | tr '\n' ' ')
{
	echo "LOCPATH=/t/loc"
	echo "TEST_LOCALES=$TEST_LOCALES"
	[ $mode = dynamic ] && echo "TEST_DL_DIR=/t/dl"
	echo "T_CRYPT_EXTDES=0"
} > "$out/ov/t/env"
[ -d out/locales ] && cp -R out/locales "$out/ov/t/loc"
# the directories t_pwd.c lists, and the user and group files it reads
rm -rf "$out/fixture"; mkdir -p "$out/fixture/d1" "$out/fixture/d2" "$out/fixture/d3"
i=1; while [ $i -le 75 ]; do n=$(printf 'f%02d' $i); : > "$out/fixture/d1/$n"; : > "$out/fixture/d2/.$n"; : > "$out/fixture/d2/File$n.c"; i=$((i + 1)); done
mkdir -p "$out/ov/etc"; cp -R "$out/fixture" "$out/ov/t/fixture"
cp /etc/passwd /etc/group /etc/hosts /etc/services /etc/protocols "$out/ov/etc/"
cp -L /etc/localtime "$out/ov/etc/localtime" 2>/dev/null
mkdir -p "$out/ov/usr/share"; cp -RL /usr/share/zoneinfo "$out/ov/usr/share/zoneinfo"
[ -n "$VM_DUMP" ] && echo "$VM_DUMP" > "$out/ov/t/dump"
echo "TEST_ROOT=/t/fixture" >> "$out/ov/t/env"
echo "PATH=/bin" >> "$out/ov/t/env"
if [ $mode = dynamic ]; then
	mkdir -p "$out/ov/lib" "$out/ov/t/dl"
	cp "$B/lib/libc.so" "$out/ov/lib/libc.so"
	for s in dl/*.c; do
		o=$(basename "$s" .c); extra=
		[ "$o" = dlb ] && extra="-L$out/ov/t/dl -ldla -Wl,-rpath,\$ORIGIN"
		LIBC_LINK= $cc -shared -fPIC -O2 -w -o "$out/ov/t/dl/lib$o.so" "$s" $extra
	done
fi
: > "$out/ov/t/list"
n=0
for t in t_*.c u_*.c s_*.c; do
	b=${t%.c}
	case " ${want:-$b} " in *" $b "*) ;; *) continue ;; esac
	libs=$(sed -n 's/^ \* REFLIBS: \(.*\) \*\/$/\1/p' "$t")
	case $b in u_*) libs="$libs -lm" ;; esac
	if ! $cc $std $pic -O2 -w -o "$out/ov/t/$b" "$t" $dbg $libs; then echo "$b: build failed"; continue; fi
	case $b in
	t_*|u_*)
		$refcc $std -O2 -w -o "$out/ref/$b" "$t" $libs || { echo "$b: ref build failed"; continue; }
		(cd "$here" && LOCPATH=$here/out/locales TEST_LOCALES="$TEST_LOCALES" T_CRYPT_EXTDES=0 TEST_ROOT="$out/fixture" "$out/ref/$b" > "$out/ref/$b.txt" 2>&1)
		;;
	esac
	case $b in
	u_*) cp "$out/ref/$b.txt" "$out/ov/t/$b.in"; echo "$b $b.in -c" >> "$out/ov/t/list" ;;
	*) echo "$b" >> "$out/ov/t/list" ;;
	esac
	n=$((n + 1))
done
echo "vmrun: $n tests built ($mode)"
[ -n "$build_only" ] && exit 0
sh "$top/distrib/mkimage-freebsd.sh" -s 256 -o "$out/img" -d "$out/ov" > "$out/mkimage.log" 2>&1 || { tail "$out/mkimage.log"; exit 1; }
rm -f "$out/log"
timeout ${VM_TIMEOUT:-900} qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 1024 -smp "$smp" \
	-drive file="$out/img",format=raw,if=none,id=d0 -device virtio-blk-pci,drive=d0 \
	-display none -monitor none -serial file:"$out/log" -no-reboot -nic none
tr -d '\r' < "$out/log" > "$out/log.txt"
grep -q '^@@@ DONE' "$out/log.txt" || echo "vmrun: the VM did not finish (see $out/log.txt)"
# split the log into the tests' outputs
rm -rf "$out/res"; mkdir -p "$out/res"
awk -v d="$out/res" '/^@@@ BEGIN /{f=d"/"$3".txt"; printf "" > f; next} /^@@@ END /{print $3, $5, $7, $9 > d"/status"; close(f); f=""; next} f{print > f}' "$out/log.txt"
pass=0 fail=0
for b in $(awk '{print $1}' "$out/ov/t/list"); do
	r=$out/res/$b.txt
	[ -f "$r" ] || { echo "$b: FAILED (no output: crashed or never ran)"; fail=$((fail + 1)); continue; }
	set -- $(awk -v n="$b" '$1 == n {print $2, $3, $4}' "$out/res/status")
	st=$1 len=$2 hash=$3
	case $b in
	t_*)
		set -- $("$out/fnv" "$out/ref/$b.txt")
		if [ "$len $hash" = "$1 $2" ]; then echo "$b: ok"; pass=$((pass + 1))
		else
			"$out/fnv" -b "$out/ref/$b.txt" | awk '{print $2, $3}' | sort -n > "$out/ref/$b.blk"
			grep -a '^@@@ BLK' "$out/log.txt" > /dev/null
			first=$(awk -v n="$b" '/^@@@ BEGIN /{on = ($3 == n)} on && /^@@@ BLK /{print $3, $4}' "$out/log.txt" | sort -n | diff - "$out/ref/$b.blk" | awk '/^[<>]/{print $2; exit}')
			echo "$b: FAILED (output $len/$hash, reference $1/$2; first differing block of 200 lines: ${first:-?}; see $r, $out/ref/$b.txt)"; fail=$((fail + 1))
		fi ;;
	*)
		if [ "$st" = 0 ] && ! grep -q FAIL "$r"; then echo "$b: ok"; pass=$((pass + 1))
		else echo "$b: FAILED (status $st, $r)"; grep FAIL "$r" | head -5; fail=$((fail + 1)); fi ;;
	esac
done
if [ -n "$dbg" ]; then
	grep -a -h "^SEGV\|^STACK" "$out"/res/*.txt 2>/dev/null | head -20
	for r in "$out"/res/*.txt; do b=$(basename "$r" .txt); grep -aq '^SEGV' "$r" || continue
		echo "== $b"; for a in $(grep -a '^STACK' "$r" | cut -d' ' -f2-) $(grep -a '^SEGV' "$r" | sed 's/.*rip=\([0-9a-fx]*\).*/\1/'); do
			llvm-addr2line -f -e "$out/ov/t/$b" $a | tr '\n' ' '; echo; done
	done
fi
echo "tests: $pass passed, $fail failed"
[ $fail = 0 ]
