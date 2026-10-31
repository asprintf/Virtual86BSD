#!/bin/sh
# nmdiff.sh NM: nm compared with GNU nm (-P, in the C locale) on the
# object files, archives and executables of the system and some made
# here (32-bit too, when the compiler makes them), with -A, -g, -u, -v
# and -t d, o, x.  GNU nm leaves out the value and size of undefined
# symbols, and the size when it is 0: they are written as 0 for the
# comparison.  With -v, symbols of the same value are compared as sets.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
command -v nm >/dev/null || { echo "nmdiff: no nm"; exit 2; }
G=$(command -v nm)
T=${TMPDIR:-/tmp}/nmdiff.$$
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
int api(int x) { helper(); return x + shared_var + bss_var + ro; }
__attribute__((weak)) int weakfn(void) { return 0; }
int main(void) { printf("%d\n", api(1)); return 0; }
X
files=
if $CC -c -o "$T/p.o" "$T/p.c" 2>/dev/null; then
    files="$files $T/p.o"
    $CC -o "$T/p" "$T/p.o" 2>/dev/null && files="$files $T/p"
    $CC -m32 -c -o "$T/p32.o" "$T/p.c" 2>/dev/null && files="$files $T/p32.o"
    $CC -shared -fPIC -o "$T/p.so" "$T/p.c" 2>/dev/null && files="$files $T/p.so"
fi
for f in /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/x86_64-linux-gnu/crti.o /usr/lib/x86_64-linux-gnu/libc_nonshared.a \
         /usr/lib/x86_64-linux-gnu/libmvec.a /usr/lib/x86_64-linux-gnu/libbz2.a /usr/lib/x86_64-linux-gnu/libexpat.a /usr/lib/x86_64-linux-gnu/libc.a /usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o; do
    [ -f "$f" ] && files="$files $f"
done
norm() { awk '{ if (NF == 2) print $1, $2, 0, 0; else if (NF == 3) print $1, $2, $3, 0; else if (NF == 4) print; else print }'; }
normA() { awk '{ n = NF; if ($n == "U" || $(n-1) == "U") { } print }' | sed 's/ *$//' | awk '{
    # lines of -A: "FILE: name type value size" or with the member
    for (i = 1; i <= NF; i++) if ($i ~ /:$/) { pre = ""; for (k = 1; k <= i; k++) pre = pre $k " "; rest = NF - i; break }
    if (rest == 2) print pre $(NF-1), $NF, 0, 0; else if (rest == 3) print pre $(NF-2), $(NF-1), $NF, 0; else print }'; }
fail=0 n=0
for f in $files; do
    for o in "-P" "-P -g" "-P -u" "-P -t d" "-P -t o" "-P -A" "-P -v"; do
        n=$((n + 1))
        a=$("$U" $o "$f" 2>&1 | grep -v ': no symbols$')
        case $o in
        *-A*) b=$("$G" $o "$f" 2>&1 | grep -v ': no symbols$' | normA) ;;
        *) b=$("$G" $o "$f" 2>&1 | grep -v ': no symbols$' | norm) ;;
        esac
        case $o in *-v*) a=$(printf '%s\n' "$a" | sort); b=$(printf '%s\n' "$b" | sort) ;; esac
        if [ "$a" != "$b" ]; then
            fail=$((fail + 1)); echo "FAIL nm $o $f"
            printf '%s\n' "$a" > "$T/a"; printf '%s\n' "$b" > "$T/b"; diff "$T/a" "$T/b" | head -6
        fi
    done
done
echo "nmdiff: $n compared, $fail failed"
[ $fail = 0 ]
