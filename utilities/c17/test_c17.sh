#!/bin/sh
# test_c17.sh - tests for c17 (POSIX.1-2024).  Run: sh test_c17.sh
#
# The compiler tested is ./c17 (or $U), with this directory as its own
# (headers, libc17rt.a) unless C17_LIBDIR is set.  Parts:
#   the command line (options, operands, exit status, diagnostics)
#   programs whose output is known (tests/run), at -O 0 and -O 1
#   the chibicc test suite (tests/chibicc), constraint diagnostics
#   (tests/diag), the preprocessor (tests/pp)
#   the assembler: encodings and relocations (tests/asm), local labels,
#     directives, errors
#   the link editor: archives, symbol resolution, TLS, GOT, the layout of
#     the executable
#   the calling convention between the two code generators (-O 0, -O 1)
#   self-hosting: c17 compiles itself twice with the same result
cd "$(dirname "$0")" || exit 2
D=$PWD
pass=0 fail=0
# t NAME EXPECTED-STDOUT EXPECTED-STATUS COMMAND...  (stdout compared without trailing newlines)
t() {
    _n=$1 _e=$2 _s=$3; shift 3
    _o=$("$@" 2>/dev/null); _r=$?
    if [ "$_o" = "$_e" ] && [ "$_r" = "$_s" ]; then pass=$((pass+1))
    else fail=$((fail+1)); printf 'FAIL %s\n  got:  [%s] status %s\n  want: [%s] status %s\n' "$_n" "$_o" "$_r" "$_e" "$_s"; fi
}
# ok NAME CONDITION-COMMAND...
ok() {
    _n=$1; shift
    if "$@"; then pass=$((pass+1)); else fail=$((fail+1)); printf 'FAIL %s\n' "$_n"; fi
}
done_tests() { echo "$1: $pass passed, $fail failed"; [ "$fail" = 0 ]; }
U=${U:-$D/c17}
C17_LIBDIR=${C17_LIBDIR:-$D}; export C17_LIBDIR
T=${TMPDIR:-/tmp}/test_c17.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../tl/tl.c; ../tl/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../tl/tl.c" || { echo "cannot compile ../tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
$cc -o "$T/fstat" "$TL/fstat.c" || { echo "cannot compile ../testlib/fstat.c"; exit 2; }
ELF="$TLBIN $D/tests/elfinfo.tl"
cd "$T" || exit 2
export LC_ALL=C
nl='
'
c17() { "$U" "$@"; }

# The C libraries: the default one, and each configuration (profile and
# linking) that works here; the programs run in all of them.
printf 'int main(void) { return 0; }\n' > nop.c
deflibc=$("$U" -v -o nop nop.c 2>&1 | sed -n 's/^libc: \([^ ]*\) .*/\1/p')
configs=
for p in "$C17_LIBDIR"/libc/*; do
    for m in static dynamic; do
        C17_LIBC=$(basename "$p") "$U" -B $m -o nop nop.c > /dev/null 2>&1 && ./nop &&
            configs="$configs $(basename "$p"):$m"
    done
done
echo "c17: default C library $deflibc; configurations:$configs"

# ------------------------------------------------------------ command line
mkdir -p sub inc1 inc2 lib1 lib2 sys mytmp
cat > sub/a.c <<'X'
int fa(void) { return 1; }
X
cat > b.c <<'X'
int fa(void);
int main(void) { return fa() == 1 ? 0 : 1; }
X
c17 -c sub/a.c b.c
ok "-c: a.o and b.o in the current directory" test -f a.o -a -f b.o -a ! -f sub/a.o
t "a.out by default; .o operands link-edited" "" 0 sh -c '"$1" a.o b.o && ./a.out' sh "$U"
t "a.out: the permissions 0777 less the umask" "$(printf %o $((0777 & ~$(umask))))" 0 "$T/fstat" %a a.out
c17 -c -o custom.o sub/a.c
ok "-c -o: the object's name" test -f custom.o
t "-c -o with two operands: an error" "" 1 "$U" -c -o x.o sub/a.c b.c
t "options among the operands" "" 0 sh -c '"$1" b.c -o prog1 sub/a.c && ./prog1' sh "$U"
cat > d.c <<'X'
#include <stdio.h>
int main(void) {
#ifdef UNDEFD
  return 9;
#endif
  printf("%d %d %s\n", ONE, VAL, STR);
  return 0;
}
X
t "-D name is 1, -D name=value" "1 42 s t" 0 sh -c '"$1" -DONE -D VAL=42 "-DSTR=\"s t\"" -DUNDEFD -o d d.c -UUNDEFD && ./d' sh "$U"
t "-U before -D: -U still wins" "1 1 " 0 sh -c '"$1" -UUNDEFD -DONE -DVAL=1 "-DSTR=\"\"" -DUNDEFD -o d2 d.c && ./d2' sh "$U"
echo '#define WHICH 1' > inc1/w.h
echo '#define WHICH 2' > inc2/w.h
echo '#define WHICH 3' > sub/w.h
echo '#include "w.h"' > sub/inc.h
printf '#include "sub/inc.h"\nint main(void) { return WHICH; }\n' > i.c
t "\"\": the including file's directory first" "" 3 sh -c '"$1" -Iinc1 -Iinc2 -o i i.c; ./i' sh "$U"
printf '#include <w.h>\nint main(void) { return WHICH; }\n' > i2.c
t "-I: the directories in order" "" 2 sh -c '"$1" -I inc2 -I inc1 -o i2 i2.c; ./i2' sh "$U"
echo 'int from_header(void);' >> inc1/w.h
c17 -E -Iinc1 i2.c > i2.i
ok "-E: line markers for included files" grep -q '^# [0-9]* "inc1/w.h"' i2.i
ok "-E: macros replaced" sh -c '! grep -q WHICH i2.i && grep -q "return 1" i2.i'
ok "-E: no object file" test ! -f i2.o
t ".i operands are compiled without preprocessing" "" 1 sh -c '"$1" -o i3 i2.i; ./i3' sh "$U"
t "-E with -o" "1" 0 sh -c '"$1" -E -o i4.i -Iinc1 i2.c && grep -c "return 1" i4.i' sh "$U"

printf 'int libfn(void) { return 11; }\n' > l1.c
printf 'int libfn(void) { return 22; }\n' > l2.c
printf 'int libfn(void);\nint main(void) { return libfn(); }\n' > lm.c
c17 -c l1.c && ar -rc lib1/libfoo.a l1.o
c17 -c l2.c && ar -rc lib2/libfoo.a l2.o
t "-l: the first -L directory that has the library" "" 11 sh -c '"$1" -o lm1 lm.c -L lib1 -L lib2 -l foo; ./lm1' sh "$U"
t "-L after -l does not apply to it" "" 22 sh -c '"$1" -o lm2 lm.c -L lib2 -lfoo -L lib1; ./lm2' sh "$U"
t "-l before any -L: not found" "" 1 "$U" -o lm3 lm.c -lfoo -Llib1
t ".a operands" "" 11 sh -c '"$1" -o lm4 lm.c lib1/libfoo.a; ./lm4' sh "$U"
t "a library before the object that needs it is not searched again" "" 1 "$U" -o lm5 lib1/libfoo.a lm.c
cat > m.c <<'X'
#include <math.h>
#include <stdio.h>
int main(void) { volatile double x = 2.0; printf("%.4f\n", sqrt(x) + sin(x*0)); return 0; }
X
t "-l m" "1.4142" 0 sh -c '"$1" -o m m.c -l m && ./m' sh "$U"
t "-l c, -l pthread, -l rt, -l xnet: part of the C library" "1.4142" 0 sh -c '"$1" -o m2 m.c -l c -l pthread -l rt -l xnet -lm && ./m2' sh "$U"
for f in "$D"/../yacc/liby/*.c; do c17 -c -o "sys/y_$(basename "$f" .c).o" "$f"; done
ar -rc sys/liby.a sys/y_*.o
for f in "$D"/../lex/libl/*.c; do c17 -c -o "sys/l_$(basename "$f" .c).o" "$f"; done
ar -rc sys/libl.a sys/l_*.o
printf '#include <stdio.h>\nint yyparse(void) { puts("parsed"); return 0; }\n' > y.c
t "-l y (yacc's): main()" "parsed" 0 sh -c '"$1" -o y y.c -L sys -l y && ./y' sh "$U"
cat > ll.c <<'X'
#include <stdio.h>
int yywrap(void);
static int n;
int yylex(void) { if (n++ < 3) { printf("tok%d\n", n); return 1; } return yywrap() ? 0 : 1; }
X
t "-l l (lex's): main() and yywrap()" "tok1${nl}tok2${nl}tok3" 0 sh -c '"$1" -o ll ll.c -L sys -l l && ./ll' sh "$U"

c17 -s -o stripped b.c sub/a.c
ok "-s: no symbol table" sh -c "! $ELF stripped | grep -q '^section .symtab '"
c17 -o notstripped b.c sub/a.c
ok "without -s: a symbol table" sh -c "$ELF notstripped | grep -q '^symbol main FUNC GLOBAL .text '"
c17 -g -c -o dbg.o b.c
ok "-g: line information in the object" sh -c "$ELF dbg.o | grep -q '^section .debug_line '"
c17 -g -o dbg b.c sub/a.c
ok "-g: line information in the executable" sh -c "$ELF dbg | grep -q '^section .debug_line '"
c17 -g -s -o dbgs b.c sub/a.c
ok "-g with -s: stripped" sh -c "! $ELF dbgs | grep -q debug_line"
t "-O 0 and -O1" "" 0 sh -c '"$1" -O 0 -o o0 b.c sub/a.c && "$1" -O1 -o o1 b.c sub/a.c && ./o0 && ./o1' sh "$U"
t "-O with a bad level: an error" "" 1 "$U" -O bogus b.c sub/a.c
case " $configs " in *" $deflibc:static "*)
t "-B static: no dynamic linker" "" 0 sh -c '"$1" -B static -o bs b.c sub/a.c && ./bs && ! $2 bs | grep -q "^interp"' sh "$U" "$ELF";;
esac
case " $configs " in *" $deflibc:dynamic "*)
t "-B dynamic: the dynamic linker and the C library" "interp needed" 0 sh -c '"$1" -B dynamic -o bd b.c sub/a.c && ./bd && $2 bd | cut -d" " -f1 | grep "^interp\|^needed" | sort -u | tr "\n" " " | sed "s/ $//"' sh "$U" "$ELF"
t "-R: DT_RUNPATH, in order" "runpath /opt/one:/opt/two" 0 sh -c '"$1" -B dynamic -R /opt/one -R /opt/two -o br b.c sub/a.c && ./br && $2 br | grep "^runpath"' sh "$U" "$ELF"
t "-B static after -B dynamic: the last one" "" 0 sh -c '"$1" -B dynamic -B static -o bs2 b.c sub/a.c 2>/dev/null && ! $2 bs2 | grep -q "^interp"' sh "$U" "$ELF";;
esac
t "-R with static linking: a warning" "c17: warning: -R ignored: static linking" 0 sh -c '"$1" -B static -R /lib -o br2 b.c sub/a.c 2>&1 | grep -v "^c17: undefined\|^c17: cannot"' sh "$U"
t "-B other: an error" "" 1 "$U" -B shared b.c sub/a.c

cat > err.c <<'X'
int main(void) { return undeclared_identifier; }
X
printf 'int good(void) { return 0; }\n' > good.c
rm -f a.out good.o
c17 -c err.c good.c 2> err.txt; st=$?
ok "an error in compiling: status > 0" test $st != 0
ok "the other operands are still compiled" test -f good.o
ok "the diagnostic names the file and line" grep -q "err.c:1" err.txt
c17 err.c good.c 2>/dev/null
ok "after an error, no link editing" test ! -f a.out
printf 'int main(void) { int *p = 5; return p != 0 ? 0 : 1; }\n' > warn.c
c17 -o warn warn.c 2> warn.txt; st=$?
ok "warnings: status 0, a message" test $st = 0 -a -s warn.txt
t "a missing operand: an error" "" 1 "$U" -o nolink missing.o
printf 'int notmain(void) { return 0; }\n' > noentry.c
t "an undefined symbol: named" "c17: undefined symbol 'main'" 0 sh -c '"$1" -o noentry noentry.c 2>&1 | sed "s/ (first.*//"' sh "$U"
t "an undefined symbol: status 1" "" 1 "$U" -o noentry noentry.c
ok "no output file after a link error" test ! -f noentry
printf 'int x = 1; int main(void) { return 0; }\n' > d1.c
printf 'int x = 2;\n' > d2.c
t "a multiple definition: an error naming the operands" "c17: multiple definition of 'x' (in d1.c and d2.c)" 1 sh -c '"$1" -o dd d1.c d2.c 2>&1; [ ! -f dd ] || echo made; "$1" -o dd d1.c d2.c 2>/dev/null' sh "$U"
cp b.c ./-weird.c
t "-- ends the options" "" 0 sh -c '"$1" -o dashdash -- -weird.c sub/a.c && ./dashdash' sh "$U"
t "TMPDIR: temporary files there, and removed" "" 0 sh -c 'TMPDIR="$PWD/mytmp" "$1" -o tmpd b.c sub/a.c && ./tmpd && [ -z "$(ls mytmp)" ]' sh "$U"
t "no operands: usage, status 2" "" 2 "$U"
t "an unknown option: status 2" "" 2 "$U" -q b.c
mkdir -p nolibc/include nolibc/lib
t "C17_LIBC=dir: the C library from another directory" "c17: cannot find crt1.o (C library $T/nolibc, in $T/nolibc/lib)" 1 sh -c 'C17_LIBC="$PWD/nolibc" "$1" -o x b.c sub/a.c 2>&1; C17_LIBC="$PWD/nolibc" "$1" -o x b.c sub/a.c 2>/dev/null' sh "$U"
t "C17_LIBC=name: an unknown profile" "1" 1 sh -c 'C17_LIBC=nosuchlibc "$1" -o x b.c sub/a.c 2>&1 | grep -c "cannot read the C library profile"; C17_LIBC=nosuchlibc "$1" -o x b.c 2>/dev/null' sh "$U"
printf 'include %s\nlibdir %s\nbogus line\n' "$T" "$T" > badprofile
t "a profile with an unknown line" "c17: $T/badprofile:3: unknown line 'bogus line'" 1 sh -c 'C17_LIBC="$PWD/badprofile" "$1" -o x b.c sub/a.c 2>&1; C17_LIBC="$PWD/badprofile" "$1" -o x b.c 2>/dev/null' sh "$U"
printf '\t.globl seven\nseven:\tmovl $7, %%eax\n\tret\n' > seven.s
printf 'int seven(void);\nint main(void) { return seven(); }\n' > usesev.c
t ".s operands are assembled" "" 7 sh -c '"$1" -o sev usesev.c seven.s; ./sev' sh "$U"
t "-c with a .s operand" "" 0 sh -c '"$1" -c seven.s && [ -f seven.o ]' sh "$U"

# ------------------------------------------------------------ programs
# at -O 0 and -O 1 with the default C library, and at -O 1 with every
# other configuration
for f in "$D"/tests/run/*.c; do
    n=$(basename "$f" .c)
    for cf in "-:0" "-:1" $configs; do
        case $cf in -:*) lib=$deflibc mode= o=${cf#-:};; *) lib=${cf%:*} mode="-B ${cf#*:}" o=1;; esac
        if C17_LIBC=$lib "$U" -O $o $mode -o "run_$n" "$f" -lm -lpthread > "run_$n.err" 2>&1 &&
           ./"run_$n" > "run_$n.out" 2>&1 && cmp -s "run_$n.out" "$D/tests/run/$n.expected"; then
            pass=$((pass+1))
        else
            fail=$((fail+1)); echo "FAIL program $n (-O $o, $lib $mode)"; head -n 3 "run_$n.err"
            diff "$D/tests/run/$n.expected" "run_$n.out" | head -n 5
        fi
    done
done

# ------------------------------------------------------------ chibicc suite
cp "$D/tests/chibicc/common" common.c
c17 -w -c -o common.o common.c
for f in "$D"/tests/chibicc/*.c; do
    n=$(basename "$f" .c)
    if (cd "$D/tests/chibicc" && "$U" -w -I. -o "$T/ch_$n" "$n.c" "$T/common.o" -lpthread) > "ch_$n.log" 2>&1 &&
       ./"ch_$n" >> "ch_$n.log" 2>&1; then
        pass=$((pass+1))
    else
        fail=$((fail+1)); echo "FAIL chibicc $n: $(grep -v '=> ' "ch_$n.log" | head -n 2) $(tail -n 1 "ch_$n.log")"
    fi
done

# ------------------------------------------------------------ diagnostics
# each tests/diag/*.c breaks a syntax rule or constraint (C17 5.1.1.3); its
# first line is /* expect error: text */ or /* expect warning: text */
for f in "$D"/tests/diag/*.c; do
    n=$(basename "$f" .c)
    hdr=$(head -n 1 "$f")
    kind=$(echo "$hdr" | sed -n 's|^/\* expect \([a-z]*\): .*|\1|p')
    text=$(echo "$hdr" | sed -n 's|^/\* expect [a-z]*: \(.*\) \*/$|\1|p')
    c17 -c -o "diag_$n.o" "$f" > "diag_$n.err" 2>&1; st=$?
    if [ "$kind" = error ]; then
        if [ $st != 0 ] && grep -q "error: " "diag_$n.err" && grep -qF -- "$text" "diag_$n.err"; then
            pass=$((pass+1))
        else
            fail=$((fail+1)); echo "FAIL diag $n (want error '$text', status $st): $(head -n 1 "diag_$n.err")"
        fi
    else
        if [ $st = 0 ] && grep -q "warning" "diag_$n.err" && grep -qF -- "$text" "diag_$n.err"; then
            pass=$((pass+1))
        else
            fail=$((fail+1)); echo "FAIL diag $n (want warning '$text', status $st): $(head -n 1 "diag_$n.err")"
        fi
    fi
done

# a null pointer constant compares with any pointer, function pointers too
printf "%s\n" "#include <stddef.h>" "typedef int (*fp)(void);" "int f(fp g, void (*h)(int)) { return g == NULL || NULL != h || g == 0; }" > npc.c
c17 -c -o npc.o npc.c > npc.err 2>&1
ok "function pointer == NULL: no diagnostic" test ! -s npc.err

# a structure with a flexible array member may be a member of a union, in
# any place (only structures and arrays may not have it)
printf "%s\n" "struct s { int n; char a[]; };" "union u { struct s s; char space[16]; };" "union v { char space[16]; struct s s; };" "int f(union u *p, union v *q) { return p->s.n + q->s.n; }" > famu.c
c17 -c -o famu.o famu.c > famu.err 2>&1
ok "flexible-array structure in a union: no diagnostic" test ! -s famu.err
printf "%s\n" "struct s { int n; char a[]; };" "struct t { struct s s; int x; };" "int f(struct t *p) { return p->x; }" > fams.c
c17 -c -o fams.o fams.c > fams.err 2>&1
ok "flexible-array structure in a structure: a warning" grep -q "flexible array member" fams.err

# ------------------------------------------------------------ preprocessor
for f in "$D"/tests/pp/*.c; do
    n=$(basename "$f" .c)
    (cd "$D/tests/pp" && "$U" -E "$n.c") 2> "pp_$n.err" | awk -f "$D/tests/ppnorm.awk" > "pp_$n.out"
    awk -f "$D/tests/ppnorm.awk" "$D/tests/pp/$n.expected" > "pp_$n.exp"
    if cmp -s "pp_$n.out" "pp_$n.exp" && ! grep -q "warning\|error" "pp_$n.err"; then
        pass=$((pass+1))
    else
        fail=$((fail+1)); echo "FAIL preprocessor $n"; head -n 3 "pp_$n.err"
    fi
done

# ------------------------------------------------------------ assembler
asmdump() {
    for s in .text .data .rodata .data.rel.ro; do echo "$s $($ELF -x $s "$1")"; done
    for s in .text .data .rodata .data.rel.ro; do echo "rela$s"; $ELF -r $s "$1"; done
}
c17 -c -o insns.o "$D/tests/asm/insns.s"
asmdump insns.o > insns.dump
ok "assembler: encodings and relocations (tests/asm/insns.s)" cmp -s insns.dump "$D/tests/asm/insns.expected"
cmp -s insns.dump "$D/tests/asm/insns.expected" || diff "$D/tests/asm/insns.expected" insns.dump | head -n 5
c17 -c -o noteo.o b.c
ok "objects: relocatable, a non-executable stack" sh -c "$ELF noteo.o | grep -q '^type REL' && $ELF noteo.o | grep -q '^section .note.GNU-stack PROGBITS'"
cat > lab.s <<'X'
# local labels, forward and backward jumps, data in .rodata, expressions
	.text
	.globl count
	.type count, @function
count:				/* rdi: limit; returns the sum 1..limit */
	xorl %eax, %eax
	xorl %ecx, %ecx
1:	incq %rcx
	cmpq %rdi, %rcx
	jg 2f
	addq %rcx, %rax
	jmp 1b
2:	ret
	.size count, .-count
	.globl table_sum
table_sum:
	leaq table(%rip), %rsi
	movq $(table_end - table) / 8, %rcx
	xorl %eax, %eax
1:	addq -8(%rsi,%rcx,8), %rax ; decq %rcx ; jnz 1b
	ret
	.globl pick
pick:				/* switch through a jump table */
	leaq jt(%rip), %rdx
	movslq (%rdx,%rdi,4), %rax
	addq %rdx, %rax
	jmp *%rax
c0:	movl $100, %eax ; ret
c1:	movl $101, %eax ; ret
c2:	movl $102, %eax ; ret
	.section .rodata
	.balign 8
table:	.quad 1, 2, 3, 4, 0x10
table_end:
jt:	.long c0 - jt, c1 - jt, c2 - jt
	.globl msg
msg:	.asciz "from \"assembly\"\n"
	.data
	.globl counter
counter: .quad 5
	.set doubled, 2 * 21
	.globl answer
answer:	.long doubled
	.comm shared, 64, 32
	.lcomm private, 16
X
cat > labmain.c <<'X'
#include <stdio.h>
long count(long), table_sum(void), pick(long);
extern char msg[];
extern long counter, shared[8];
extern int answer;
int main(void) {
  shared[7] = 9;
  printf("%ld %ld %ld %ld %ld %ld %d %ld %d%s", count(10), count(0), table_sum(), pick(0),
         pick(2), counter, answer, shared[7], (int)((unsigned long)shared % 32), msg);
  return 0;
}
X
t "assembler: labels, jumps, jump tables, data, .comm" "55 0 26 100 102 5 42 9 0from \"assembly\"" 0 sh -c '"$1" -o lab labmain.c lab.s && ./lab' sh "$U"
printf '\tfrobnicate %%rax\n' > bad1.s
t "assembler: an unknown instruction, with the line" "bad1.s:1: error: invalid instruction or operands: '	frobnicate %rax'" 1 sh -c '"$1" -c bad1.s 2>&1; "$1" -c bad1.s 2>/dev/null' sh "$U"
printf '\tmovl $1, %%eax\n\tjmp nowhere_local\n.Lx: ret\n\tjmp .Lmissing\n' > bad2.s
t "assembler: an undefined local label" "1" 1 sh -c '"$1" -c bad2.s 2>&1 | grep -c "undefined local symbol .\.Lmissing"; "$1" -c bad2.s 2>/dev/null' sh "$U"
printf 'x: ret\nx: ret\n' > bad3.s
t "assembler: a label defined twice" "bad3.s:2: error: symbol 'x' is already defined" 1 sh -c '"$1" -c bad3.s 2>&1; "$1" -c bad3.s 2>/dev/null' sh "$U"
printf '\t.bogus 1\n' > bad4.s
t "assembler: an unknown directive" "bad4.s:1: error: unknown directive '.bogus'" 1 sh -c '"$1" -c bad4.s 2>&1; "$1" -c bad4.s 2>/dev/null' sh "$U"
printf '\taddl %%eax, %%ebx, %%ecx\n' > bad5.s
t "assembler: wrong operands" "1" 1 sh -c '"$1" -c bad5.s 2>&1 | grep -c "invalid instruction or operands"; "$1" -c bad5.s 2>/dev/null' sh "$U"

# ------------------------------------------------------------ link editor
cat > tls.s <<'X'
# thread-local storage, local-exec, initial-exec and general-dynamic
	.section .tdata,"awT",@progbits
	.globl tv
	.balign 8
tv:	.quad 1234
	.section .tbss,"awT",@nobits
	.globl tz
	.balign 16
tz:	.zero 16
	.text
	.globl get_le
get_le:	movq %fs:tv@tpoff, %rax
	ret
	.globl get_ie
get_ie:	movq tv@gottpoff(%rip), %rax
	movq %fs:(%rax), %rax
	ret
	.globl addr_gd
addr_gd:
	subq $8, %rsp
	.byte 0x66
	leaq tz@tlsgd(%rip), %rdi
	.value 0x6666
	rex64
	call __tls_get_addr@PLT
	addq $8, %rsp
	ret
	.globl get_got
get_got:			/* a variable through the GOT */
	movq counter2@GOTPCREL(%rip), %rax
	movq (%rax), %rax
	ret
X
cat > tlsmain.c <<'X'
#include <stdio.h>
long get_le(void), get_ie(void), get_got(void);
long *addr_gd(void);
extern _Thread_local long tv, tz[2];
long counter2 = 77;
int main(void) {
  tz[1] = 5;
  printf("%ld %ld %ld %d %ld\n", get_le(), get_ie(), *(addr_gd() + 1), addr_gd() == tz, get_got());
  return 0;
}
X
t "link: TLS (local-exec, initial-exec, general-dynamic rewritten), GOT" "1234 1234 5 1 77" 0 sh -c '"$1" -o tls tlsmain.c tls.s && ./tls' sh "$U"
printf 'int weakfn(void) __attribute__((weak));\nint main(void) { return weakfn ? weakfn() : 3; }\n' > wk1.c
printf 'int weakfn(void) { return 4; }\n' > wk2.c
t "link: an undefined weak function is null" "" 3 sh -c '"$1" -o wk wk1.c; ./wk' sh "$U"
t "link: a weak reference resolved by a definition" "" 4 sh -c '"$1" -o wk2 wk1.c wk2.c; ./wk2' sh "$U"
printf 'int v(void) __attribute__((weak));\nint v(void) { return 1; }\n' > wd1.c
printf 'int v(void) { return 2; }\nint main(void) { return v(); }\n' > wd2.c
t "link: a strong definition overrides a weak one" "" 2 sh -c '"$1" -o wd wd1.c wd2.c; ./wd' sh "$U"
t "link: and the other way round" "" 2 sh -c '"$1" -o wd3 wd2.c wd1.c; ./wd3' sh "$U"
printf 'int weakfn(void) { return 6; }\n' > wa.c
c17 -c wa.c && ar -rc libwa.a wa.o
t "link: a weak reference does not take a member from an archive" "" 3 sh -c '"$1" -o wk3 wk1.c libwa.a; ./wk3' sh "$U"
printf '\t.comm cbuf, 100, 8\n' > c1.s
printf '\t.comm cbuf, 400, 64\n' > c2.s
printf 'extern char cbuf[];\nint main(void) { cbuf[399] = 1; return ((unsigned long)cbuf %% 64 == 0) + cbuf[399]; }\n' > cm.c
t "link: common symbols merged, largest size and alignment" "" 2 sh -c '"$1" -o cm cm.c c1.s c2.s; ./cm' sh "$U"
printf 'int dep2(void);\nint dep1(void) { return dep2() + 1; }\n' > ar1.c
printf 'int dep2(void) { return 40; }\n' > ar2.c
printf 'int dep1(void);\nint main(void) { return dep1(); }\n' > armain.c
c17 -c ar1.c ar2.c && ar -rc libdeps.a ar2.o ar1.o
t "link: an archive searched until no member is needed" "" 41 sh -c '"$1" -o arx armain.c libdeps.a; ./arx' sh "$U"
c17 -c -o odd.o ar1.c
printf 'not an object\n' > junk.o
t "link: an input that is not an object" "c17: junk.o: not an object file or archive" 1 sh -c '"$1" -o j armain.c junk.o 2>&1; "$1" -o j armain.c junk.o 2>/dev/null' sh "$U"
$ELF notstripped > layout.txt
ok "executable: ET_EXEC" grep -q '^type EXEC$' layout.txt
ok "executable: read-only, executable and writable segments" sh -c "grep -q '^segment LOAD R$' layout.txt && grep -q '^segment LOAD R E$' layout.txt && grep -q '^segment LOAD R W$' layout.txt"
ok "executable: a non-executable stack" grep -q '^segment GNU_STACK R W$' layout.txt
ok "executable: the entry point is _start" sh -c "[ \"\$(sed -n 's/^entry //p' layout.txt)\" = \"\$(awk '\$2 == \"_start\" { print \$6 }' layout.txt)\" ]"
$ELF tls > tlslayout.txt
ok "executable: a TLS segment when there is thread-local storage" grep -q '^segment TLS R$' tlslayout.txt

# ------------------------------------------------------------ shared libraries (-G)
cat > shlib.c <<'END'
int counter = 5;                       /* exported, so a program may copy it */
static int calls;
__attribute__((visibility("hidden"))) int helper(int x) { return x * 3; }
int add(int a, int b) { calls++; return a + b + helper(0); }
static const char *names[] = { "zero", "one", "two" };   /* relocated pointers */
int (*fp)(int, int) = add;             /* a function's address in data */
int *pcounter = &counter;
const char *name(int i) { return names[i]; }
int bump(void) { return ++counter + calls; }
_Thread_local int tl = 7;              /* general-dynamic */
int *tlp(void) { return &tl; }
long double twice(long double x) { return x * 2; }
END
cat > shmain.c <<'END'
#include <stdio.h>
extern int counter, *pcounter;
extern int (*fp)(int, int);
int add(int, int); const char *name(int); int bump(void); int *tlp(void);
long double twice(long double);
int main(void) {
    int a = add(1, 2), eq = fp == add, b, c;
    counter = 100;                     /* the program's copy is the variable */
    b = bump();
    c = *pcounter;
    printf("%d %s %d %d %d %d %Lg\n", a, name(2), eq, b, c, *tlp(), twice(1.5L));
    return 0;
}
END
cat > shdl.c <<'END'
#include <dlfcn.h>
#include <stdio.h>
int main(void) {
    void *h = dlopen("./libsh.so", RTLD_NOW);
    int *(*f)(void) = h ? (int *(*)(void))dlsym(h, "tlp") : 0;
    printf("%d\n", f ? *f() : -1);
    return 0;
}
END
"$U" -G -c -o shlib.o shlib.c
$ELF -r .text shlib.o > shrel.txt
ok "-G -c: position-independent (through the GOT and the PLT)" sh -c "grep -q GOTPCREL shrel.txt && grep -q PLT32 shrel.txt && grep -q TLSGD shrel.txt && ! grep -q 'R_X86_64_32' shrel.txt"
case " $configs " in *" $deflibc:dynamic "*)
"$U" -G -o libsh.so shlib.c
$ELF libsh.so > shlayout.txt
ok "-G: a shared object (ET_DYN), its soname the name of the file" sh -c "grep -q '^type DYN$' shlayout.txt && grep -q '^soname libsh.so$' shlayout.txt && ! grep -q '^interp' shlayout.txt"
ok "-G: functions and variables exported" sh -c "grep -q '^dynsym add FUNC GLOBAL .text$' shlayout.txt && grep -q '^dynsym counter OBJECT GLOBAL .data$' shlayout.txt"
ok "-G: hidden and static symbols not exported" sh -c "! grep -q '^dynsym helper ' shlayout.txt && ! grep -q '^dynsym calls ' shlayout.txt"
t "-G: a program using it (copied variable, function addresses, TLS)" "3 two 1 102 101 7 3" 0 sh -c '"$1" -B dynamic -o shm shmain.c -L . -l sh -R "$PWD" && ./shm' sh "$U"
t "-G: dlopen, a thread-local variable through __tls_get_addr" "7" 0 sh -c '"$1" -B dynamic -o shd shdl.c -l dl && ./shd' sh "$U"
printf 'int v; int f(void) { return v; }\n' > nopic.c
"$U" -c -o nopic.o nopic.c
t "-G: an object not compiled with -G: an error" "c17: nopic.o: 'v' used directly (not through the GOT) in .text: not in a shared library (compile it with -G)" 1 sh -c '"$1" -G -o libnp.so nopic.o 2>&1; "$1" -G -o libnp.so nopic.o 2>/dev/null' sh "$U"
printf '__attribute__((visibility("hidden"))) int nowhere(void);\nint g(void) { return nowhere(); }\n' > hidundef.c
t "-G: an undefined hidden symbol: an error" "c17: undefined symbol 'nowhere' of hidden visibility (first referenced in hidundef.c)" 1 sh -c '"$1" -G -o libhu.so hidundef.c 2>&1 | sed "s/(first referenced in [^)]*)/(first referenced in hidundef.c)/"; "$1" -G -o libhu.so hidundef.c 2>/dev/null' sh "$U"
printf 'ENTRY(start_here)\n' > entry.lds
printf 'void start_here(void) {}\n' > entry.c
"$U" -G -o libent.so entry.lds entry.c
ok "-G: ENTRY(symbol) in a linker script: the entry point" sh -c "[ \"\$($ELF libent.so | sed -n 's/^entry //p')\" = \"\$($ELF libent.so | awk '\$2 == \"start_here\" { print \$6 }')\" ]";;
esac
printf '__attribute__((tls_model("initial-exec"))) _Thread_local int ie;\nint *p(void) { return &ie; }\n' > tlsie.c
"$U" -G -S -o tlsie.s tlsie.c
ok "-G: tls_model(\"initial-exec\"): the initial-exec sequence" sh -c "grep -q 'ie@gottpoff' tlsie.s && ! grep -q tlsgd tlsie.s"
printf '%s\n' '#include <stdio.h>' 'static void *ra(void) { return __builtin_return_address(0); }' 'static void *f1(void) { return __builtin_frame_address(1); }' 'int main(void) { void *a = ra(), *b = ra(); printf("%d %d\n", a != b && a != 0, f1() == __builtin_frame_address(0)); return 0; }' > frames.c
t "__builtin_return_address, __builtin_frame_address" "1 1" 0 sh -c '"$1" -o frames frames.c && ./frames' sh "$U"

# ------------------------------------------------------------ calling convention
# the general and the fast code paths (-O 0, -O 1) must agree on it
for seed in 1 2 3; do
    "$TLBIN" "$D/tests/abigen.tl" $seed 30 &&
    c17 -w -O 0 -c -o ce0.o callee.c && c17 -w -O 1 -c -o ce1.o callee.c &&
    c17 -w -O 0 -c -o cr0.o caller.c && c17 -w -O 1 -c -o cr1.o caller.c &&
    c17 -o abi11 cr1.o ce1.o && c17 -o abi01 cr0.o ce1.o && c17 -o abi10 cr1.o ce0.o &&
    ./abi11 > abi11.out && ./abi01 > abi01.out && ./abi10 > abi10.out
    ok "calling convention, -O 0 with -O 1 (seed $seed)" sh -c 'cmp -s abi11.out abi01.out && cmp -s abi11.out abi10.out && [ -s abi11.out ]'
done

# ------------------------------------------------------------ self-hosting
# c17 compiles itself (stage 2), which compiles itself (stage 3); all three
# write the same assembly for every source
# (stages 2 and 3 use the C library ./c17 has)
mkdir -p s2 s3
st=0
C17_LIBC=$deflibc; export C17_LIBC
for s in s2 s3; do
    comp=$U; [ $s = s3 ] && comp=$T/s2/c17
    for f in "$D"/*.c "$D"/../lib/cpp.c; do
        b=$(basename "$f" .c)
        case $b in rt_*) continue;; esac
        "$comp" -I "$D/../lib" -c -o "$s/$b.o" "$f" ||
            { st=1; echo "self-hosting: $s failed on $b.c"; }
    done
    "$comp" -o "$s/c17" "$s"/*.o || st=1
done
diffs=0
if [ $st = 0 ]; then
    for f in "$D"/*.c "$D"/../lib/cpp.c; do
        "$U" -I "$D/../lib" -S -o 1.s "$f" && s2/c17 -I "$D/../lib" -S -o 2.s "$f" &&
        s3/c17 -I "$D/../lib" -S -o 3.s "$f" &&
        cmp -s 1.s 2.s && cmp -s 2.s 3.s || { diffs=$((diffs+1)); echo "self-hosting: output differs for $f"; }
    done
fi
ok "self-hosting: stages 1, 2 and 3 write the same assembly" test $st = 0 -a $diffs = 0

done_tests c17
