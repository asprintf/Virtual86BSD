#!/bin/sh
# test_cflow.sh - tests for cflow (POSIX.1-2024).  Run: sh test_cflow.sh
cd "$(dirname "$0")" || exit 2
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
U=${U:-$PWD/cflow}
T=${TMPDIR:-/tmp}/test_cflow.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# the example of the standard
printf 'int i;\nint f();\nint g();\nint h();\nint\nmain(void)\n{\n    f();\n    g();\n    f();\n}\nint\nf()\n{\n    i = h();\n}\n' > file.c
cat > a.c <<'X'
#include "inc.h"
static char *buf;
int table[10];
static void helper(void) { buf = 0; }
int _hidden(void) { return 0; }
void (*hook)(void);
int rec(int n) { return n ? rec(n - 1) : 0; }
int ping(int n);
int pong(int n) { return n ? ping(n - 1) : 0; }
int ping(int n) { return n ? pong(n - 1) : 0; }
int main(int argc, char **argv)
{
#ifdef EXTRA
    extra();
#endif
#if LEVEL == 2
    level2();
#endif
    CALL(zeta);
    helper();
    hook = helper;
    table[0] = _hidden() + rec(argc);
    alpha(argv[0]);
    return 0;
}
X
printf '#define CALL(f) called_##f()\n' > inc.h
cat > b.c <<'X'
static void helper(int x) { (void)x; }
void alpha(char *s) { helper(*s); }
X
mkdir inc
printf '#define ONLY() only_here()\n' > inc/only.h
printf '#include <only.h>\nint main(void) { return ONLY(); }\n' > inc2.c
t "the example of the standard (-i x)" '1 main: int(), <file.c 6>
2    f: int(), <file.c 13>
3        h: <>
4        i: int, <file.c 1>
5    g: <>' 0 "$U" -i x file.c
t "without -i x: functions only" '1 main: int(), <file.c 6>
2    f: int(), <file.c 13>
3        h: <>
4    g: <>' 0 "$U" file.c
t "a name written again: the number of its line" '1 main: int(), <a.c 11>
2    alpha: void(), <b.c 2>
3        helper: void(), <b.c 1>
4    called_zeta: <>
5    helper: void(), <a.c 4>
6    rec: int(), <a.c 7>
7        rec: 6
8 ping: int(), <a.c 10>
9    pong: int(), <a.c 9>
10        ping: 8' 0 "$U" a.c b.c
t "-i x -i _: data, and names starting with _" '1 main: int(), <a.c 11>
2    _hidden: int(), <a.c 5>
3    alpha: <>
4    called_zeta: <>
5    helper: void(), <a.c 4>
6        buf: char *, <a.c 2>
7    hook: void (*)(), <a.c 6>
8    rec: int(), <a.c 7>
9        rec: 8
10    table: int[], <a.c 3>
11 ping: int(), <a.c 10>
12    pong: int(), <a.c 9>
13        ping: 11' 0 "$U" -i x -i _ a.c
t "-i x_: both in one option-argument" "$("$U" -i x -i _ a.c)" 0 "$U" -i x_ a.c
t "-r: the callers of each name" '1 f: int(), <file.c 13>
2    main: int(), <file.c 6>
3 g: <>
4    main: 2
5 h: <>
6    f: 1
7 i: int, <file.c 1>
8    f: 1
9 main: 2' 0 "$U" -r -i x file.c
t "-r: static functions of two files are two" '5 helper: void(), <a.c 4>
6    main: 2
7 helper: void(), <b.c 1>
8    alpha: 1' 0 sh -c '"$1" -r a.c b.c | awk '"'"'/helper:/ { n = 2 } n > 0 { print; n-- }'"'"' ' sh "$U"
t "-d 1: the top level only" '1 main: int(), <file.c 6>' 0 "$U" -d 1 file.c
t "-d 2" '1 main: int(), <file.c 6>
2    f: int(), <file.c 13>
3    g: <>' 0 "$U" -d 2 file.c
t "-d 0: ignored" "$("$U" file.c)" 0 "$U" -d 0 file.c
t "-d -3: ignored" "$("$U" file.c)" 0 "$U" -d -3 file.c
t "the definition of a C function: type, file and line" '1 main: int(), <file.c 6>' 0 sh -c '"$1" file.c | head -n 1' sh "$U"
t "a function pointer used as a value is a reference" '4    helper: void(), <a.c 4>' 0 sh -c '"$1" a.c | grep helper' sh "$U"
t "-D: a macro defined" '1 main: int(), <a.c 11>
2    alpha: <>
3    called_zeta: <>
4    extra: <>' 0 sh -c '"$1" -D EXTRA a.c | head -n 4' sh "$U"
t "-D name=def" '5    level2: <>' 0 sh -c '"$1" -D LEVEL=2 a.c | grep level2' sh "$U"
t "-U after -D undefines it" '' 1 sh -c '"$1" -D EXTRA -U EXTRA a.c | grep extra' sh "$U"
t "-D after -U defines it" '4    extra: <>' 0 sh -c '"$1" -U EXTRA -D EXTRA a.c | grep extra' sh "$U"
t "-I: where headers are looked for" '1 main: int(), <inc2.c 2>
2    only_here: <>' 0 "$U" -I inc inc2.c
t "a header not found: a warning, not an error" 'cflow: inc2.c:1: only.h: No such file or directory' 0 sh -c '"$1" inc2.c 2>&1 >/dev/null' sh "$U"
cat > p.i <<'X'
# 1 "orig.c"
# 1 "<built-in>"
# 1 "orig.c"
int helper2(void);

# 10 "orig.c"
int main(void)
{
    return helper2();
}
X
t ".i: the output of c17 -E, with its line markers" '1 main: int(), <orig.c 10>
2    helper2: <>' 0 "$U" p.i
cat > s.s <<'X'
	.text
	.globl	main
	.type	main, @function
main:
	pushq	%rbp
	call	work
	call	puts@PLT
	ret
.L1:
work:
	call	*%rax
	ret
	.data
	.type	value, @object
value:
	.long	1
X
t ".s: labels and calls" '1 main: <s.s 4>
2    puts: <>
3    work: <s.s 10>' 0 "$U" s.s
t "two kinds of files together: a name is one function" '1 main: int(), <file.c 6>
2    f: int(), <file.c 13>
3        h: <>
4    g: <>
5    puts: <>
6    work: <s.s 10>' 0 "$U" file.c s.s
if command -v cc >/dev/null 2>&1 && printf 'int g(void);\nint v;\nint main(void) { return g() + v; }\n' > o.c && cc -c o.c -o o.o 2>/dev/null; then
    t "an object file: symbols and relocations" '1 main: <o.o text>
2    g: <>
3    v: <o.o bss>' 0 "$U" -i x o.o
fi
if command -v lex >/dev/null 2>&1; then
    printf '%%%%\na\t{ found(); }\n%%%%\nint main(void) { return yylex(); }\n' > l.l
    t ".l: lex input, by the lines of the .l file" '1 main: int(), <l.l 4>' 0 sh -c '"$1" -d 1 l.l | head -n 1' sh "$U"
    t ".l: the code lex makes is lex.yy.c" 'lex.yy.c' 0 sh -c '"$1" -d 2 l.l | sed -n "2s/.*<\\([^ ]*\\) .*/\\1/p"' sh "$U"
fi
if command -v yacc >/dev/null 2>&1; then
    printf '%%token A\n%%%%\ns: A { action(); } ;\n%%%%\nint main(void) { return yyparse(); }\n' > y.y
    t ".y: yacc input" '1 main: int(), <y.y 5>
2    yyparse: int(), <y.tab.c' 0 sh -c '"$1" -d 2 y.y | head -n 2 | sed "2s/ [0-9]*>\$//"' sh "$U"
    t ".y: the actions are in the graph" '1' 0 sh -c '"$1" y.y | grep -c "action: <>"' sh "$U"
fi
t "a file that cannot be read: status 1, the others written" '1 main: int(), <file.c 6>' 1 sh -c '"$1" nosuch.c file.c 2>/dev/null > out; s=$?; head -n 1 out; exit $s' sh "$U"
t "a file that cannot be read: a diagnostic" 'cflow: nosuch.c: No such file or directory' 1 sh -c '"$1" nosuch.c 2>&1' sh "$U"
t "no file: a usage error" '' 2 "$U"
t "-i with another letter: a usage error" '' 2 "$U" -i z file.c
t "-d not a number: a usage error" '' 2 "$U" -d x file.c
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" 'cflow: -r: No such file or directory' 1 sh -c '"$1" file.c -r 2>&1 >/dev/null' sh "$U"
done_tests cflow
