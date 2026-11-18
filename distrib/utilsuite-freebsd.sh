#!/bin/sh
# utilsuite-freebsd.sh [-x] [-o DIR] [-t SECONDS] [-T SECONDS] [UTILITY...]  (-x: sh -x, the trace is in the log): run the utilities' tests (regress/{bin,sbin,libexec,usr.bin,usr.sbin})
# in QEMU on the FreeBSD kernel, as root, from /etc/rc.local.  Needs obj/boot (sh distrib/boot-freebsd.sh -n).
# The programs are those built there, placed as /usr/src/DIR/NAME/NAME, the tests in /usr/src/regress/DIR/NAME,
# which is the layout the tests expect (PROG_DIR); no compiler is needed.  Result: DIR/suite.log, DIR/summary.
out=obj/utilsuite tmo=3000 utmo=600 trace=
diag=
while getopts xe:o:t:T: opt; do case $opt in x) trace=-x ;; e) diag=$OPTARG ;; o) out=$OPTARG ;; t) tmo=$OPTARG ;; T) utmo=$OPTARG ;; *) exit 2 ;; esac; done
shift $((OPTIND - 1))
want=$*
top=$(cd "$(dirname "$0")/.." && pwd); cd "$top" || exit 1
[ -d obj/boot/root ] && [ -d obj/boot/src ] || { echo "utilsuite: run distrib/boot-freebsd.sh -n first" >&2; exit 1; }
rm -rf "$out"; mkdir -p "$out" && out=$(cd "$out" && pwd) || exit 1
root=$out/root
cp -a obj/boot/root "$root" || exit 1
for d in bin sbin libexec usr.bin usr.sbin; do
	for p in obj/boot/src/$d/*/; do
		u=$(basename "$p")
		mkdir -p "$root/usr/src/$d/$u"
		for f in "$p"*; do [ -f "$f" ] && [ -x "$f" ] && cp "$f" "$root/usr/src/$d/$u/"; done
		[ -d "regress/$d/$u" ] && { mkdir -p "$root/usr/src/regress/$d"; cp -R "regress/$d/$u" "$root/usr/src/regress/$d/"; }
	done
done
# the tests compile a few helper programs with c17, c99 or cc: no compiler here, so cc is a script that copies
# the helper built here (regress/lib/testlib/*.c, usr.bin/tl/tl.c) by the name of the source; the tests that
# compile code they generate (yacc, lex, ...) cannot run until the image has a compiler
mkdir -p "$root/var/tmp" "$root/usr/lib/testbin" "$root/usr/src/regress/lib" "$root/usr/src/usr.bin/tl"
chmod 1777 "$root/var/tmp" "$root/tmp"
[ -z "$diag" ] || cp "$diag" "$root/usr/lib/testbin/diag.sh"
cp -R regress/lib/testlib "$root/usr/src/regress/lib/"
cp usr.bin/tl/tl.c "$root/usr/src/usr.bin/tl/"
export LIBC_LIB=$top/obj/boot/libc/lib LIBC_LINK=dynamic LIBC_INTERP=/lib/libc.so
for f in regress/lib/testlib/*.c regress/usr.bin/who/tests/*.c usr.bin/tl/tl.c; do
	b=$(basename "$f" .c)
	sh "$top/lib/libc/tools/libc-cc" clang -O2 -w -D_DEFAULT_SOURCE -fPIE -o "$root/usr/lib/testbin/$b" "$f" -lm 2>> "$out/helpers.log" || echo "helper $b failed"
done
cat > "$root/usr/bin/cc" <<'EOT'
#!/bin/sh
out= src=
while [ $# -gt 0 ]; do
	case $1 in -o) out=$2; shift ;; *.c) [ -n "$src" ] || src=$1 ;; esac
	shift
done
b=$(basename "$src" .c)
[ -x "/usr/lib/testbin/$b" ] || { echo "cc: no compiler in this image, no prebuilt $b" >&2; exit 1; }
cp "/usr/lib/testbin/$b" "${out:-a.out}"
EOT
chmod 755 "$root/usr/bin/cc"
# the FreeBSD UFS tools where the tests of fsck and makefs look for programs
for t in newfs dumpfs fsck_ffs tunefs growfs; do mkdir -p "$root/usr/src/sbin/$t"; cp "$root/sbin/$t" "$root/usr/src/sbin/$t/"; done
{ echo "WANT='$want' UTMO=$utmo SHX=$trace"; cat <<'EOT'
exec > /dev/console 2>&1
echo SUITE-BEGIN
[ -f /usr/lib/testbin/diag.sh ] && sh /usr/lib/testbin/diag.sh
for d in bin sbin libexec usr.bin usr.sbin; do
	for u in $(ls /usr/src/regress/$d 2>/dev/null); do
		if [ -n "$WANT" ]; then case " $WANT " in *" $u "*) ;; *) continue ;; esac; fi
		[ -f /usr/src/regress/$d/$u/test_$u.sh ] || continue
		(cd /usr/src/regress/$d/$u && PROG_DIR=/usr/src/$d/$u timeout $UTMO sh $SHX ./test_$u.sh < /dev/null) > /tmp/o.$u 2>&1
		st=$?
		[ -z "$SHX" ] || tail -n 25 /tmp/o.$u
		echo "UTIL $d/$u status $st: $(tail -n 1 /tmp/o.$u)"
		grep -i 'fail' /tmp/o.$u | grep -v ' 0 failed' | head -n 12
	done
done
echo SUITE-END
halt -p
EOT
} > "$root/etc/rc.local"
sh distrib/mkimage-freebsd.sh -o "$out/suite.img" -s 768 -d "$root" > "$out/mk.log" 2>&1 || { tail "$out/mk.log"; exit 1; }
: > "$out/suite.log"
timeout "$tmo" qemu-system-x86_64 -machine q35 -accel kvm -cpu host -m 1024 -smp 2 -display none -serial file:"$out/suite.log" \
	-drive file="$out/suite.img",format=raw,if=virtio -nic user,model=virtio-net-pci < /dev/null > /dev/null 2>&1
tr -d '\r' < "$out/suite.log" | sed -n '/SUITE-BEGIN/,/SUITE-END/p' > "$out/summary"
grep -c '^UTIL ' "$out/summary"
