#!/bin/sh
# run.sh CCNAME - build and run the tests against build/CCNAME/lib.
#   t_*.c  differential: built against the system C library (with cc) and
#          against this one; the outputs must be identical
#   u_*.c  differential within a tolerance: the system's build writes its
#          results, this one's reads and compares them ("-c")
#   s_*.c  self-checking: against this library only; they print FAIL lines
# LIBC_LIB, as for tools/libc-cc, selects another build of the library.
cd "$(dirname "$0")" || exit 1
ccname=${1:-gcc}
refcc=${REFCC:-gcc}
out=out/$ccname${LIBC_LINK:+-$LIBC_LINK}
mkdir -p "$out"
cc="../tools/libc-cc $ccname"
fail=0 n=0
# locales for t_locale.c, made with the localedef found (glibc's or that
# of the POSIX utilities: both write glibc's format), if there is one
loc=out/locales
if [ ! -d "$loc" ] && command -v localedef >/dev/null 2>&1; then
	mkdir -p "$loc"
	for spec in nl_NL:UTF-8 de_DE:ISO-8859-1 tr_TR:UTF-8 ru_RU:KOI8-R \
	    ja_JP:EUC-JP ja_JP:SHIFT_JIS ko_KR:EUC-KR zh_CN:GB18030 zh_CN:GBK \
	    zh_CN:GB2312 zh_TW:BIG5 ja_JP:UTF-8 th_TH:UTF-8; do
		l=${spec%%:*} c=${spec#*:}
		# (SHIFT_JIS gives warnings, and status 1, and a usable locale)
		localedef -f "$c" -i "$l" "$PWD/$loc/$l.$c" >/dev/null 2>&1 ||
			[ -f "$loc/$l.$c/LC_CTYPE" ] || rm -rf "$loc/$l.$c"
	done
	# a locale of its own for collation (collate.def)
	for c in UTF-8 ISO-8859-1; do
		localedef -f "$c" -i ./collate.def "$PWD/$loc/xx_XX.$c" >/dev/null 2>&1 || rm -rf "$loc/xx_XX.$c"
	done
fi
TEST_LOCALES=$(ls "$loc" 2>/dev/null | tr '\n' ' ')
LOCPATH=$PWD/$loc
export TEST_LOCALES LOCPATH
# linked dynamically: shared objects for s_dlopen, by the same compiler
unset TEST_DL_DIR
if [ "$LIBC_LINK" = dynamic ]; then
	mkdir -p "$out/dl"
	for s in dl/*.c; do
		o=$(basename "$s" .c)
		case $ccname in
		c17*)
			extra=
			[ "$o" = dlb ] && extra="-L $out/dl -l dla -R \$ORIGIN"
			$cc -G -o "$out/dl/lib$o.so" "$s" $extra;;
		*)
			extra=
			[ "$o" = dlb ] && extra="-L$out/dl -ldla -Wl,-rpath,\$ORIGIN"
			LIBC_LINK= $cc -shared -fPIC -O2 -w -o "$out/dl/lib$o.so" "$s" $extra;;
		esac || echo "$s: build failed"
	done
	TEST_DL_DIR=$PWD/$out/dl
	export TEST_DL_DIR
fi
for t in t_*.c; do
	b=${t%.c}; n=$((n + 1))
	# extra libraries of the reference build (and empty ones in ours)
	libs=$(sed -n 's/^ \* REFLIBS: \(.*\) \*\/$/\1/p' "$t")
	if ! $refcc -O2 -w -o "$out/$b.ref" "$t" $libs || ! $cc -O2 -w -o "$out/$b.lib" "$t" $libs; then
		echo "$b: build failed"; fail=$((fail + 1)); continue
	fi
	"$out/$b.ref" > "$out/$b.ref.txt" 2>&1
	"$out/$b.lib" > "$out/$b.lib.txt" 2>&1
	if cmp -s "$out/$b.ref.txt" "$out/$b.lib.txt"; then echo "$b: ok"
	else echo "$b: FAILED (outputs differ: $out/$b.ref.txt $out/$b.lib.txt)"; fail=$((fail + 1)); fi
done
for t in u_*.c; do
	b=${t%.c}; n=$((n + 1))
	if ! $refcc -O2 -w -o "$out/$b.ref" "$t" -lm || ! $cc -O2 -w -o "$out/$b.lib" "$t" -lm; then
		echo "$b: build failed"; fail=$((fail + 1)); continue
	fi
	"$out/$b.ref" > "$out/$b.ref.txt" 2>&1
	if "$out/$b.lib" -c < "$out/$b.ref.txt" > "$out/$b.txt" 2>&1; then echo "$b: ok"
	else echo "$b: FAILED (see $out/$b.txt)"; grep FAIL "$out/$b.txt" | head -5; fail=$((fail + 1)); fi
done
for t in s_*.c; do
	b=${t%.c}; n=$((n + 1))
	if ! $cc -O2 -w -o "$out/$b.lib" "$t"; then
		echo "$b: build failed"; fail=$((fail + 1)); continue
	fi
	"$out/$b.lib" > "$out/$b.txt" 2>&1; st=$?
	if [ $st = 0 ] && ! grep -q FAIL "$out/$b.txt"; then echo "$b: ok"
	else echo "$b: FAILED (status $st, see $out/$b.txt)"; grep FAIL "$out/$b.txt" | head -5; fail=$((fail + 1)); fi
done
echo "tests: $((n - fail)) passed, $fail failed"
[ $fail = 0 ]
