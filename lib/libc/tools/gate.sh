#!/bin/sh
# gate.sh - os-test and the Open POSIX Test Suite on the FreeBSD kernel.
#   sh gate.sh [-t seconds] [-s smp] ost|pts WORKDIR [SRC]
# The tests are built on the host with clang against this libc (dynamic, libc.so
# as the interpreter), put in a disk image and run in QEMU one by one by
# guestrun.c (its init), with a time limit each; gateparse.c turns the serial
# log into os-test's out/v86bsd/*.out or the lines "NAME STATUS" of
# pts.sh.  A run cut short continues where it stopped: run it again.
#   ost: SRC is regress/os-test (default); WORKDIR/ost/html/index.html at the end
#   pts: SRC is LTP's testcases/open_posix_testsuite (default obj/ltp/...);
#        WORKDIR/pts-results (NAME STATUS) at the end
tmo=60 smp=1
while getopts t:s: opt; do
	case $opt in t) tmo=$OPTARG ;; s) smp=$OPTARG ;; *) exit 2 ;; esac
done
shift $((OPTIND - 1))
[ $# -ge 2 ] || { echo "usage: sh gate.sh [-t seconds] [-s smp] ost|pts WORKDIR [SRC]" >&2; exit 2; }
what=$1 work=$2 src=${3:-}
tools=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$tools/../../.." && pwd)
mkdir -p "$work" && work=$(cd "$work" && pwd) || exit 1
B=$work/libc ov=$work/ov
export LC_ALL=C

echo "gate: libc"
(cd "$top/lib/libc" && make -s -j12 B="$B" all) > "$work/libc.log" 2>&1 || { tail "$work/libc.log"; exit 1; }
cc="sh $tools/libc-cc clang"
export LIBC_LIB=$B/lib LIBC_LINK=dynamic LIBC_INTERP=/lib/libc.so
clang -O2 -Wall -o "$work/gateparse" "$tools/gateparse.c" || exit 1

# the guest: init, libc.so, sh and the few commands the tests run, the files they read
mkdir -p "$ov/sbin" "$ov/bin" "$ov/lib" "$ov/tmp" "$ov/var/run" "$ov/etc" "$ov/g" "$ov/usr/share"
LIBC_LINK=static $cc -O2 -w -o "$ov/sbin/init" "$tools/guestrun.c" || exit 1
cp "$B/lib/libc.so" "$ov/lib/libc.so"
pic=-fPIE
if [ ! -x "$work/sh/sh" ] || [ "$B/lib/libc.so" -nt "$work/sh/sh" ]; then
	rm -rf "$work/sh" "$work/yl"; mkdir -p "$work/sh" "$work/yl"
	cp -R "$top/bin/sh/." "$work/sh/"
	for d in yacc lex; do cp -R "$top/usr.bin/$d" "$work/yl/$d"; (cd "$work/yl/$d" && make -s CC=clang) > /dev/null 2>&1; done
	(cd "$work/sh" && make -s YACC="$work/yl/yacc/yacc" LEX="$work/yl/lex/lex" CC="$cc" CFLAGS="-O2 -w -D_DEFAULT_SOURCE $pic") > "$work/sh.log" 2>&1 || { tail "$work/sh.log"; exit 1; }
fi
cp "$work/sh/sh" "$ov/bin/sh"
mkdir -p "$work/t" && ln -sfn "$top/lib" "$work/t/lib"
for p in rm cat mkdir rmdir echo kill sleep test printf true false env gencat ls; do
	src_=$top/bin/$p; [ -d "$src_" ] || src_=$top/usr.bin/$p
	cd_=$work/t/$(basename "$(dirname "$src_")")/$p
	[ -x "$cd_/$p" ] && { cp "$cd_/$p" "$ov/bin/$p"; continue; }
	rm -rf "$cd_"; mkdir -p "$(dirname "$cd_")"; cp -R "$src_" "$cd_"
	(cd "$cd_" && make -s CC="$cc" CFLAGS="-O2 -w -D_DEFAULT_SOURCE $pic") > "$work/cmd-$p.log" 2>&1 &&
		cp "$cd_/$p" "$ov/bin/$p" || echo "gate: $p not built"
done
cp /etc/passwd /etc/group /etc/hosts /etc/services /etc/protocols "$ov/etc/"
cp -L /etc/localtime "$ov/etc/localtime" 2>/dev/null
[ -d "$ov/usr/share/zoneinfo" ] || cp -RL /usr/share/zoneinfo "$ov/usr/share/zoneinfo"
printf 'PATH=/bin\nHOME=/tmp\n' > "$ov/g/env"

# boot the VM with the tests of $work/todo ("ID DIR PROG TIMEOUT" lines) until all have a result
# usage: runvm RESULT-ARG ; calls the parser; unfinished tests are recorded by crash_$what
runvm() {
	boots=0
	while [ -s "$work/todo" ] && [ $boots -lt 8 ]; do
		boots=$((boots + 1))
		echo "gate: boot $boots, $(wc -l < "$work/todo") tests to run"
		cp "$work/todo" "$ov/g/list"
		find "$ov/g" -type l -exec rm {} +
		sh "$top/distrib/mkimage-freebsd.sh" -s "${IMGMB:-512}" -o "$work/img" -d "$ov" > "$work/mkimage.log" 2>&1 || { tail "$work/mkimage.log"; exit 1; }
		rm -f "$work/log"
		timeout "${VM_TIMEOUT:-5400}" qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 1024 -smp "$smp" \
			-drive file="$work/img",format=raw,if=none,id=d0 -device virtio-blk-pci,drive=d0 \
			-display none -monitor none -serial file:"$work/log" -no-reboot \
			-nic user,model=virtio-net-pci
		cp "$work/log" "$work/log.$boots"
		"$work/gateparse" "$what" "$work/log" "$resarg" > "$work/parse.out" 2> "$work/parse.err"
		cat "$work/parse.err"
		unf=$(sed -n 's/^UNFINISHED //p' "$work/parse.err")
		if [ -n "$unf" ]; then crash "$unf"; echo "$unf" >> "$resarg.ids"; fi
		grep -q '^UNAME ' "$work/parse.err" && sed -n 's/^UNAME //p' "$work/parse.err" | head -n 1 > "$work/uname"
		awk 'NR == FNR {d[$1]; next} !($1 in d)' "$resarg.ids" "$work/todo" > "$work/todo.new"
		if cmp -s "$work/todo" "$work/todo.new"; then echo "gate: no progress, stopping"; break; fi
		mv "$work/todo.new" "$work/todo"
	done
	[ -s "$work/todo" ] && echo "gate: $(wc -l < "$work/todo") tests have no result"
}

case $what in
ost)
	[ -n "$src" ] || src=$top/regress/os-test
	d=$work/ost
	[ -d "$d" ] || cp -R "$src" "$d"
	mkdir -p "$d/out/v86bsd"
	clang -O2 -o "$d/misc/namespace" "$d/misc/namespace.c" -lm 2> /dev/null || clang -O2 -o "$d/misc/namespace" "$d/misc/namespace.c" || exit 1
	cat > "$work/ostcc" <<EOF
#!/bin/sh
for a do shift; [ "\$a" = -pthread ] || set -- "\$@" "\$a"; done
exec sh $tools/libc-cc clang -fPIC "\$@"
EOF
	chmod +x "$work/ostcc"
	suites=${SUITES:-$(grep -v 'legacy\|xcurses' "$d/misc/suites.list")}
	echo "gate: building os-test ($(echo $suites | wc -w) suites)"
	for s in $suites; do
		(cd "$d" && OS=V86BSD OST_CC="$work/ostcc" sh "$tools/ost/ost.sh" build "$s" > "$work/build-$s.log" 2>&1) &
	done
	wait
	: > "$work/todo"
	for s in $suites; do
		case $s in include|namespace|legacy-include|xcurses-include) continue ;; esac
		(cd "$d/$s" && find . -name '*.c' | sed 's,^\./,,; s,\.c$,,' | sort) | while read -r t; do
			[ -f "$d/out/v86bsd/$s/$t.out" ] && continue
			[ -x "$d/$s/$t" ] || continue
			echo "$s/$t /g/ost/$s ./$t $tmo"
		done >> "$work/todo"
		rm -rf "$ov/g/ost/$s"; mkdir -p "$ov/g/ost/$s"
		(cd "$d/$s" && tar cf - --exclude='*.o' .) | (cd "$ov/g/ost/$s" && tar xf -)
	done
	resarg=$d/out/v86bsd
	crash() { mkdir -p "$(dirname "$resarg/$1")"; printf 'the system stopped\nexit: 9998\n' > "$resarg/$1.out"; }
	runvm
	[ -s "$work/uname" ] && sed 's/^FreeBSD /FreeBSD /' "$work/uname" > "$d/out/v86bsd/uname.out"
	cd "$d" || exit 1
	[ -x misc/html ] && [ misc/html -nt misc/html.c ] || clang -O2 -o misc/html misc/html.c || exit 1
	oses=$(cd out && for x in *; do [ -d "$x" ] && printf '%s ' "$x"; done)
	misc/html --suites-list="$(echo $suites)" --os-directory=out --os-list="$oses" > "$work/html.log" 2>&1
	misc/html --suites-list="$(echo $suites)" --os-directory=out --os-list="$oses" --format=json --output=os-test.json >> "$work/html.log" 2>&1
	echo "gate: $d/html/index.html, $d/os-test.json (log $work/html.log)" ;;
pts)
	[ -n "$src" ] || src=$top/obj/ltp/src/testcases/open_posix_testsuite
	[ -f "$src/lib/common.c" ] || { echo "gate: $src is not open_posix_testsuite" >&2; exit 1; }
	p=$work/pts logs=$work/ptslog
	resarg=$work/pts-results
	mkdir -p "$p" "$logs"; : >> "$resarg"; : >> "$resarg.ids"
	CF="-D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -I$src/include -fPIE"
	$cc $CF -c -o "$work/common.o" "$src/lib/common.c" || exit 1
	(cd "$src" && { find conformance -type f -name '[0-9]*-[0-9]*.c'; find functional -type f -name '*.c'; } | sort) > "$work/pts.list"
	cat > "$work/ptsbuild.sh" <<EOF
#!/bin/sh
t=\$1; dir=\$(dirname "\$t"); name=\$(basename "\$t" .c)
mkdir -p "$p/\$dir" "$logs/\$dir"
case \$name in
*buildonly*) if $cc $CF -I"$src/\$dir" -c -o "$logs/\$dir/\$name.o" "$src/\$t" > "$logs/\$t.blog" 2>&1; then echo "\$t PASS"; else echo "\$t BUILD"; fi ;;
*) $cc $CF -I"$src/\$dir" -o "$p/\$dir/\$name" "$src/\$t" "$work/common.o" -lpthread -lrt -lm > "$logs/\$t.blog" 2>&1 || echo "\$t BUILD" ;;
esac
EOF
	echo "gate: building $(wc -l < "$work/pts.list") Open POSIX tests"
	xargs -P 12 -n 1 sh "$work/ptsbuild.sh" < "$work/pts.list" > "$work/pts.prebuilt"
	# a result ("PASS" for build-only tests, "BUILD") before the VM run
	awk 'NR == FNR {d[$1]; next} !($1 in d)' "$resarg.ids" "$work/pts.prebuilt" | tee -a "$resarg" | awk '{print $1}' >> "$resarg.ids"
	: > "$work/todo"
	while read -r t; do
		grep -qxF "$t" "$resarg.ids" && continue
		dir=$(dirname "$t") name=$(basename "$t" .c)
		[ -x "$p/$dir/$name" ] && echo "$t /g/pts/$dir ./$name $tmo"
	done < "$work/pts.list" > "$work/todo"
	# ONLY=file: run just the tests named in it (one per line)
	[ -z "${ONLY:-}" ] || { awk 'NR == FNR {k[$1]; next} $1 in k' "$ONLY" "$work/todo" > "$work/todo.new" && mv "$work/todo.new" "$work/todo"; }
	rm -rf "$ov/g/pts"; mkdir -p "$ov/g/pts"; (cd "$p" && tar cf - .) | (cd "$ov/g/pts" && tar xf -)
	IMGMB=${IMGMB:-1024}
	crash() { echo "$1 SYSTEM-STOPPED" >> "$resarg"; }
	runvm
	awk '{ c[$2]++ } END { for (s in c) printf "%-14s %5d\n", s, c[s] }' "$resarg" | sort | tee "$work/pts-summary.txt"
	echo "gate: $resarg (NAME STATUS), summary $work/pts-summary.txt" ;;
*) echo "gate: ost or pts" >&2; exit 2 ;;
esac
