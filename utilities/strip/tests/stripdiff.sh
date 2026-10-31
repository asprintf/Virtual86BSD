#!/bin/sh
# stripdiff.sh STRIP: strip compared with GNU strip (a functional
# comparison: the files are not the same byte for byte; relocatable
# files, which this strip keeps linkable, are compared with GNU strip
# --strip-unneeded, the others with GNU strip).  For objects
# and executables made here (with -g; 32-bit too, when the compiler makes
# them) and for object files and archives of the system, both are
# stripped, then compared:
#  - the names of the sections (readelf -S), in order;
#  - the global symbols (GNU nm -g -P), and the dynamic ones (nm -D -P);
#  - for executables: no symbol table is left, the outputs of running
#    the two are the same;
#  - for objects: the stripped object still links, and the program runs.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
for c in strip nm readelf; do command -v $c >/dev/null || { echo "stripdiff: no $c"; exit 2; }; done
G=$(command -v strip)
T=${TMPDIR:-/tmp}/stripdiff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
export LC_ALL=C
CC=${CC:-cc}
cat > "$T/p.c" <<'X'
#include <stdio.h>
static int hidden = 1;
int shared_var = 2, bss_var;
const int ro = 3;
static void helper(void) { hidden++; }
int api(int x) { helper(); return x + shared_var + bss_var + ro + hidden; }
__attribute__((weak)) int weakfn(void) { return 0; }
int main(void) { printf("%d %d\n", api(1), weakfn()); return 0; }
X
files=
if $CC -g -c -o "$T/p.o" "$T/p.c" 2>/dev/null; then
    files="$files $T/p.o"
    $CC -g -o "$T/p" "$T/p.o" 2>/dev/null && files="$files $T/p"
    $CC -g -static -o "$T/ps" "$T/p.o" 2>/dev/null && files="$files $T/ps"
    $CC -g -ffunction-sections -c -o "$T/pf.o" "$T/p.c" 2>/dev/null && files="$files $T/pf.o"
    $CC -g -m32 -c -o "$T/p32.o" "$T/p.c" 2>/dev/null && files="$files $T/p32.o"
    $CC -g -m32 -o "$T/p32" "$T/p.c" 2>/dev/null && files="$files $T/p32"
    $CC -g -shared -fPIC -o "$T/p.so" "$T/p.c" 2>/dev/null && files="$files $T/p.so"
fi
L=/usr/lib/x86_64-linux-gnu
for f in $L/crt1.o $L/crti.o $L/libc_nonshared.a $L/libbz2.a $L/libexpat.a $L/libz.a /usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o; do
    [ -f "$f" ] && files="$files $f"
done
secs() { readelf -SW "$1" 2>/dev/null | sed -n 's/^ *\[ *[0-9]*\] \([^ ]*\) .*/\1/p' | grep -v '^NULL$'; }
fail=0 n=0
bad() { fail=$((fail + 1)); echo "FAIL $*"; }
for f in $files; do
    n=$((n + 1))
    b=$(basename "$f")
    cp "$f" "$T/u.$b"; cp "$f" "$T/g.$b"
    "$U" "$T/u.$b" || { bad "strip $f: status"; continue; }
    case $b in *.o|*.a) "$G" --strip-unneeded "$T/g.$b" 2>/dev/null ;; *) "$G" "$T/g.$b" 2>/dev/null ;; esac
    [ "$(secs "$T/u.$b")" = "$(secs "$T/g.$b")" ] || { bad "sections $f"; diff "$T/u.$b" "$T/g.$b" >/dev/null; secs "$T/u.$b" > "$T/a"; secs "$T/g.$b" > "$T/c"; diff "$T/a" "$T/c" | head -n 6; }
    [ "$(nm -g -P "$T/u.$b" 2>&1 | sed "s|$T/[ug]\.||")" = "$(nm -g -P "$T/g.$b" 2>&1 | sed "s|$T/[ug]\.||")" ] || bad "global symbols $f"
    [ "$(nm -D -P "$T/u.$b" 2>&1 | sed "s|$T/[ug]\.||")" = "$(nm -D -P "$T/g.$b" 2>&1 | sed "s|$T/[ug]\.||")" ] || bad "dynamic symbols $f"
    case $b in
    p|ps|p32)
        nm "$T/u.$b" 2>&1 | grep -q 'no symbols' || bad "symbol table left in $f"
        [ "$("$T/u.$b")" = "$("$T/g.$b")" ] && [ -n "$("$T/u.$b")" ] || bad "run $f" ;;
    p.o|pf.o)
        $CC -o "$T/l.$b" "$T/u.$b" 2>/dev/null && [ "$("$T/l.$b")" = "$("$T/p")" ] || bad "link $f" ;;
    p32.o)
        $CC -m32 -o "$T/l.$b" "$T/u.$b" 2>/dev/null && [ "$("$T/l.$b")" = "$("$T/p32")" ] || bad "link $f" ;;
    esac
done
echo "stripdiff: $n compared, $fail failed"
[ $fail = 0 ]
