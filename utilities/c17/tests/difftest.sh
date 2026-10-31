#!/bin/sh
# difftest.sh - compares c17 with other implementations (make difftest)
#
#   the programs of tests/run: the output of c17's and of $REFCC's (cc)
#   the calling convention: tests/abigen.tl, each half built by c17 or by
#     $REFCC and linked by $REFCC, in all four combinations ($ABI_SEEDS)
#   the assembler: tests/asm/insns.s assembled by c17 and by the GNU
#     assembler ($REFAS, as), the bytes and relocations compared
#   if CTESTSUITE names a checkout of c-testsuite
#     (https://github.com/c-testsuite/c-testsuite): its single-file tests
#   if CSMITH names the installation prefix of Csmith: $CSMITH_COUNT random
#     programs, the checksums of c17's and $REFCC's builds compared
#
# Both compilers use their default C library (for c17, its profile).
cd "$(dirname "$0")/.." || exit 2
D=$PWD
U=${U:-$D/c17}
C17_LIBDIR=${C17_LIBDIR:-$D}; export C17_LIBDIR
REFCC=${REFCC:-cc}
REFAS=${REFAS:-as}
pass=0 fail=0 skip=0
T=${TMPDIR:-/tmp}/difftest_c17.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T" "$T.tl"' EXIT
TL=$(cd ../testlib && pwd) || exit 2
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl
cd "$T" || exit 2
good() { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "FAIL $1"; }

# the programs of tests/run
for f in "$D"/tests/run/*.c; do
    n=$(basename "$f" .c)
    if ! $REFCC -w -o "ref_$n" "$f" -lm -lpthread -latomic 2>/dev/null &&
       ! $REFCC -w -o "ref_$n" "$f" -lm -lpthread; then
        skip=$((skip+1)); continue
    fi
    ./"ref_$n" > "ref_$n.out" 2>&1
    if "$U" -w -o "c17_$n" "$f" -lm -lpthread > "c17_$n.err" 2>&1 &&
       ./"c17_$n" > "c17_$n.out" 2>&1 && cmp -s "ref_$n.out" "c17_$n.out"; then
        good
    else
        bad "program $n"; diff "ref_$n.out" "c17_$n.out" | head -n 5
    fi
done

# the calling convention
seeds=${ABI_SEEDS:-1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20}
for s in $seeds; do
    "$TLBIN" "$D/tests/abigen.tl" "$s" 40 || exit 2
    $REFCC -w -c -o ce_r.o callee.c && $REFCC -w -c -o cr_r.o caller.c || { skip=$((skip+1)); continue; }
    if ! "$U" -w -c -o ce_c.o callee.c || ! "$U" -w -c -o cr_c.o caller.c; then
        bad "calling convention, seed $s: c17 failed"; continue
    fi
    $REFCC -o rr cr_r.o ce_r.o && ./rr > rr.out
    ok=1
    for combo in "cr_c.o ce_r.o" "cr_r.o ce_c.o" "cr_c.o ce_c.o"; do
        $REFCC -o t $combo && ./t > t.out 2>&1
        cmp -s rr.out t.out || { ok=0; echo "  seed $s: ($combo) differs"; diff rr.out t.out | head -n 4; }
    done
    if [ $ok = 1 ]; then good; else bad "calling convention, seed $s"; cp callee.c "$D/abi-callee-$s.c"; cp caller.c "$D/abi-caller-$s.c"; fi
done

# the assembler
asmdump() {
    for s in .text .data .rodata .data.rel.ro; do echo "$s $("$TLBIN" "$D/tests/elfinfo.tl" -x $s "$1")"; done
    for s in .text .data .rodata .data.rel.ro; do echo "rela$s"; "$TLBIN" "$D/tests/elfinfo.tl" -r $s "$1"; done
}
if $REFAS -o ref_insns.o "$D/tests/asm/insns.s" 2>/dev/null; then
    "$U" -c -o c17_insns.o "$D/tests/asm/insns.s"
    asmdump ref_insns.o > ref.dump
    asmdump c17_insns.o > c17.dump
    if cmp -s ref.dump c17.dump; then good; else bad "assembler: tests/asm/insns.s"; diff ref.dump c17.dump | head -n 5; fi
    # c17's own output, assembled both ways: the same code (jumps aside,
    # which $REFAS makes short where it can) is checked by running it
    for f in "$D"/tests/run/*.c; do
        n=$(basename "$f" .c)
        "$U" -w -S -o "$n.s" "$f" && $REFAS -o "$n.ref.o" "$n.s" && "$U" -o "$n.refas" "$n.ref.o" -lm -lpthread &&
        ./"$n.refas" > "$n.refas.out" 2>&1
        if cmp -s "$n.refas.out" "$D/tests/run/$n.expected"; then good; else bad "assembly of $n.c by $REFAS, linked by c17"; fi
    done
else
    skip=$((skip+1))
fi

# c-testsuite
if [ -n "$CTESTSUITE" ]; then
    for f in "$CTESTSUITE"/tests/single-exec/*.c; do
        n=$(basename "$f" .c)
        exp="$f.expected"
        if "$U" -w -o "cts_$n" "$f" -lm > /dev/null 2>&1 && ./"cts_$n" > "cts_$n.out" 2>&1 &&
           { [ ! -f "$exp" ] || cmp -s "$exp" "cts_$n.out"; }; then
            good
        else
            bad "c-testsuite $n"
        fi
    done
fi

# Csmith
if [ -n "$CSMITH" ]; then
    inc=$(ls -d "$CSMITH"/include/csmith*/ 2>/dev/null | head -n 1)
    [ -n "$inc" ] || inc=$CSMITH/include
    i=0
    while [ $i -lt "${CSMITH_COUNT:-100}" ]; do
        i=$((i+1)); seed=$(( ${CSMITH_SEED:-1} + i ))
        "$CSMITH/bin/csmith" --seed $seed --no-packed-struct > t.c 2>/dev/null || { skip=$((skip+1)); continue; }
        $REFCC -w -O0 -I"$inc" -o ref t.c 2>/dev/null || { skip=$((skip+1)); continue; }
        "$TLBIN" -e 'var r = run(["./ref"], {timeout: 5}); writefile("ref.out", r.out); exit(r.timedout ? 124 : 0);'
        [ $? = 124 ] && { skip=$((skip+1)); continue; }
        if "$U" -w -I"$inc" -o c t.c > /dev/null 2>&1 &&
           "$TLBIN" -e 'var r = run(["./c"], {timeout: 10}); writefile("c.out", r.out);' &&
           cmp -s ref.out c.out; then
            good
        else
            bad "csmith seed $seed"; cp t.c "$D/csmith-$seed.c"
        fi
    done
fi

echo "c17 difftest: $pass passed, $fail failed, $skip skipped"
[ $fail = 0 ]
