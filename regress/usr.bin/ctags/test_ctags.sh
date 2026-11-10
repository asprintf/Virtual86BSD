#!/bin/sh
# test_ctags.sh - tests for ctags (POSIX.1-2024).  Run: sh test_ctags.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/ctags}; export PROG_DIR
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
U=${U:-$PROG_DIR/ctags}
T=${TMPDIR:-/tmp}/test_ctags.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
tab=$(printf '\t')
cat > x.c <<'X'
#include <stdio.h>
#define MAX 10
#define SQ(a) ((a) * (a))
typedef unsigned long ulong;
struct point { int x, y; };
union u { int i; float f; };
enum color { RED, GREEN };
typedef struct node { struct node *next; } node_t;
typedef int (*handler)(int);
struct point *origin;
int counter = 0;
int proto(int);
struct decl;
static int helper(int a)
{
    return a + 1; /* { */
}
char *
name(void)
{
    return "}";
}
int main(void)
{
    return helper(SQ(MAX));
}
X
cat > kr.c <<'X'
int old(a, b)
int a;
char *b;
{
    return a;
}
X
cat > cond.c <<'X'
#if 0
int zero(void) { return 0; }
#else
int one(void) { return 1; }
#endif
#ifdef UNIX
int first(void) { return 1; }
#else
int second(void) { return 2; }
#define FROM_ELSE 1
#endif
#ifndef X
int neg(void) { return 1; }
#elif Y
int elif(void) { return 1; }
#endif
X
cat > esc.c <<'X'
int path(char *s) { return s[0] == '/' || s[0] == '\\'; }
#define LONG(a, \
    b) ((a) + (b))
X
printf 'int main(void) { return 0; }\n' > prog
t "the tags file: identifier, file, pattern" "helper${tab}x.c${tab}/^static int helper(int a)\$/" 0 sh -c '"$1" x.c && grep "^helper" tags' sh "$U"
t "the objects tagged, in the order of the identifiers" 'MAX
Mx
SQ
color
handler
helper
name
node
node_t
point
u
ulong' 0 sh -c '"$1" x.c && cut -f 1 tags' sh "$U"
t "a macro without arguments" "MAX${tab}x.c${tab}/^#define MAX 10\$/" 0 sh -c '"$1" x.c && grep "^MAX" tags' sh "$U"
t "a macro with arguments" "SQ${tab}x.c${tab}/^#define SQ(a) ((a) * (a))\$/" 0 sh -c '"$1" x.c && grep "^SQ" tags' sh "$U"
t "main: M and the file's name without .c" "Mx${tab}x.c${tab}/^int main(void)\$/" 0 sh -c '"$1" x.c && grep "^Mx" tags' sh "$U"
t "main in a file without the .c suffix" "Mprog${tab}prog${tab}/^int main(void) { return 0; }\$/" 0 sh -c '"$1" prog && cat tags' sh "$U"
t "a function returning a pointer: the line of its name" "name${tab}x.c${tab}/^name(void)\$/" 0 sh -c '"$1" x.c && grep "^name" tags' sh "$U"
t "typedef struct: the tag and the typedef name" "node${tab}x.c${tab}/^typedef struct node { struct node *next; } node_t;\$/
node_t${tab}x.c${tab}/^typedef struct node { struct node *next; } node_t;\$/" 0 sh -c '"$1" x.c && grep "^node" tags' sh "$U"
t "a typedef of a function pointer" "handler${tab}x.c${tab}/^typedef int (*handler)(int);\$/" 0 sh -c '"$1" x.c && grep "^handler" tags' sh "$U"
t "prototypes, objects, tags without members: not tagged" '' 1 sh -c '"$1" x.c && grep -E "^(proto|counter|origin|decl|RED)	" tags' sh "$U"
t "braces in comments and strings do not count" 'Mx
name' 0 sh -c '"$1" x.c && grep -E "^(name|Mx)	" tags | cut -f 1' sh "$U"
t "a K&R definition" "old${tab}kr.c${tab}/^int old(a, b)\$/" 0 sh -c '"$1" kr.c && cat tags' sh "$U"
t "#if 0: not read, its #else is" 'one' 0 sh -c '"$1" cond.c && grep -E "^(zero|one)	" tags | cut -f 1' sh "$U"
t "#ifdef: the first group read, not the #else" 'first' 0 sh -c '"$1" cond.c && grep -E "^(first|second)	" tags | cut -f 1' sh "$U"
t "#ifndef: the first group read, not the #elif" 'neg' 0 sh -c '"$1" cond.c && grep -E "^(neg|elif)	" tags | cut -f 1' sh "$U"
t "a #define in a group not read is tagged" 'FROM_ELSE' 0 sh -c '"$1" cond.c && grep "^FROM_ELSE" tags | cut -f 1' sh "$U"
t "/ and \\ in the pattern are escaped" "path${tab}esc.c${tab}/^int path(char *s) { return s[0] == '\\/' || s[0] == '\\\\\\\\'; }\$/" 0 sh -c '"$1" esc.c && grep "^path" tags' sh "$U"
t "a macro continued on the next line: its first line" "LONG${tab}esc.c${tab}/^#define LONG(a, \\\\\$/" 0 sh -c '"$1" esc.c && grep "^LONG" tags' sh "$U"
t "sorted in the order of the POSIX locale" 'B
_c
a' 0 sh -c 'printf "int a(void){}\nint _c(void){}\nint B(void){}\n" > s.c && LC_ALL=C "$1" s.c && cut -f 1 tags' sh "$U"
t "several files" 'Mx x.c
helper x.c
old kr.c' 0 sh -c '"$1" x.c kr.c && grep -E "^(Mx|helper|old)	" tags | cut -f 1,2 | tr "\t" " "' sh "$U"
t "-f: another file" "old${tab}kr.c${tab}/^int old(a, b)\$/" 0 sh -c 'rm -f tags && "$1" -f mytags kr.c && ! test -f tags && cat mytags' sh "$U"
t "-f -: the standard output" "old${tab}kr.c${tab}/^int old(a, b)\$/" 0 sh -c 'rm -f tags && "$1" -f - kr.c && ! test -f tags' sh "$U"
t "a tags file is replaced" "old${tab}kr.c${tab}/^int old(a, b)\$/" 0 sh -c '"$1" x.c && "$1" kr.c && cat tags' sh "$U"
t "-a: appended, sorted with the lines there" 'Mx
helper
old
zz' 0 sh -c 'printf "zz\tz.c\t/^zz$/\n" > tags && "$1" -a kr.c && "$1" -a x.c && grep -E "^(Mx|helper|old|zz)	" tags | cut -f 1' sh "$U"
t "-a: no line twice" '1' 0 sh -c '"$1" kr.c && "$1" -a kr.c && grep -c "^old" tags' sh "$U"
t "-a without a tags file: made" "old${tab}kr.c${tab}/^int old(a, b)\$/" 0 sh -c '"$1" -a kr.c && cat tags' sh "$U"
t "-x: name, line, file, text" 'old                 1 kr.c             int old(a, b)' 0 "$U" -x kr.c
t "-x: no tags file is made" '' 0 sh -c 'rm -f tags && "$1" -x kr.c > /dev/null && ! test -f tags' sh "$U"
t "-x: sorted by name, then file" 'MAX                 2 x.c              #define MAX 10
Mx                 23 x.c              int main(void)
SQ                  3 x.c              #define SQ(a) ((a) * (a))
old                 1 kr.c             int old(a, b)' 0 sh -c '"$1" -x x.c kr.c | grep -E "^(MAX|Mx|SQ|old) "' sh "$U"
t "a file that cannot be read: status 1, the others tagged" "old${tab}kr.c${tab}/^int old(a, b)\$/" 1 sh -c '"$1" nosuch.c kr.c; s=$?; cat tags; exit $s' sh "$U"
t "a file that cannot be read: a diagnostic" 'ctags: nosuch.c: No such file or directory' 1 sh -c '"$1" -x nosuch.c 2>&1 >/dev/null' sh "$U"
t "an empty file: an empty tags file" '0' 0 sh -c ': > e.c && "$1" e.c && wc -c < tags | tr -d " "' sh "$U"
t "no file: a usage error" '' 2 "$U"
t "-x with -f: a usage error" '' 2 "$U" -x -f t kr.c
t "-x with -a: a usage error" '' 2 "$U" -x -a kr.c
t "an unknown option: a usage error" '' 2 "$U" -q kr.c
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" 'ctags: -x: No such file or directory' 1 sh -c '"$1" kr.c -x 2>&1 >/dev/null' sh "$U"
done_tests ctags
