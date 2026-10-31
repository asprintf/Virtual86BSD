#!/bin/sh
# test_cxref.sh - tests for cxref (POSIX.1-2024).  Run: sh test_cxref.sh
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
U=${U:-$PWD/cxref}
T=${TMPDIR:-/tmp}/test_cxref.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
cat > m.c <<'X'
#define N 10
#define TWICE(x) ((x) * 2)
typedef int count_t;
enum mode { OFF, ON };
static count_t total;
int values[N];
struct rec { int field; };
static int add(int a, int b)
{
    int sum = a + b;
    total += sum;
    return TWICE(sum);
}
int main(int argc, char **argv)
{
    struct rec r;
    enum mode m = ON;
    r.field = add(argc, N);
    values[0] = r.field;
    m++;
#ifdef N
    (void)argv;
#endif
    return m == OFF;
}
X
cat > n.c <<'X'
extern int values[];
int other(void)
{
    return values[1];
}
X
# many references to one name in one function, on lines 3 to 62
{ echo 'int x;'; echo 'void f(void) {'; i=0; while [ $i -lt 60 ]; do echo '    (void)x;'; i=$((i + 1)); done; echo '}'; } > w.c
mkdir inc
printf '#define FROM_INC 1\n' > inc/h.h
printf '#include <h.h>\nint v = FROM_INC;\n#if DEF == 3\nint three;\n#endif\n' > i.c
t "the listing of a file" 'm.c

SYMBOL   FILE FUNCTION LINE
N        m.c  --       *1 6
N        m.c  main     18 21
OFF      m.c  --       *4
OFF      m.c  main     24
ON       m.c  --       *4
ON       m.c  main     17
TWICE    m.c  --       *2
TWICE    m.c  add      12
a        m.c  add      *8 10
add      m.c  --       *8 18
argc     m.c  main     *14 18
argv     m.c  main     *14 22
b        m.c  add      *8 10
count_t  m.c  --       *3 5
m        m.c  main     *17 =20 24
main     m.c  --       *14
r        m.c  main     *16 =18 19
sum      m.c  add      *10 11 12
total    m.c  --       *5
total    m.c  add      =11
values   m.c  --       *6
values   m.c  main     =19' 0 "$U" m.c
t "a macro: * at its #define, its uses in their functions" 'N        m.c  --       *1 6
N        m.c  main     18 21' 0 sh -c '"$1" m.c | grep "^N "' sh "$U"
t "a function: -- as its function, called or defined" 'add      m.c  --       *8 18' 0 sh -c '"$1" m.c | grep "^add "' sh "$U"
t "parameters and automatic objects: in their functions" 'a        m.c  add      *8 10
sum      m.c  add      *10 11 12' 0 sh -c '"$1" m.c | grep -E "^(a|sum) "' sh "$U"
t "a static object used in a function" 'total    m.c  --       *5
total    m.c  add      =11' 0 sh -c '"$1" m.c | grep "^total "' sh "$U"
t "= where it is assigned to: =, op=, ++, a subscript, a member" 'm        m.c  main     *17 =20 24
r        m.c  main     *16 =18 19
total    m.c  add      =11
values   m.c  main     =19' 0 sh -c '"$1" m.c | grep -E "^(m|r|total|values) " | grep =' sh "$U"
t "typedef names and enumeration constants" 'OFF      m.c  --       *4
count_t  m.c  --       *3 5' 0 sh -c '"$1" m.c | grep -E "^(OFF|count_t) " | grep -- --' sh "$U"
t "members and tags are not symbols" '' 1 sh -c '"$1" m.c | grep -E "^(field|rec|mode) "' sh "$U"
t "a name used in #ifdef is a reference" '21' 0 sh -c '"$1" m.c | grep "^N .* main" | sed "s/.* //"' sh "$U"
t "two files: two listings, each after the file's name" 'n.c
m.c' 0 sh -c '"$1" n.c m.c | grep "\.c\$"' sh "$U"
t "-s: without the names of the files" '' 1 sh -c '"$1" -s n.c m.c | grep "\.c\$"' sh "$U"
t "-c: one listing of all the files" 'SYMBOL   FILE FUNCTION LINE
values   m.c  --       *6
values   m.c  main     =19
values   n.c  --       *1
values   n.c  other    4' 0 sh -c '"$1" -c m.c n.c | grep -E "^(SYMBOL|values) "' sh "$U"
t "-c: the file name lines are left out" '1' 0 sh -c '"$1" -c m.c n.c | grep -c SYMBOL' sh "$U"
t "-o: the listing to a file" "$("$U" m.c)" 0 sh -c '"$1" -o out m.c && cat out' sh "$U"
t "-o: nothing on the standard output" '' 0 "$U" -o out m.c
t "the references wrapped to 80 columns" '0' 0 sh -c '"$1" w.c | awk "length > 80" | wc -l | tr -d " "' sh "$U"
t "wrapped lines indented to the column of the references" '1' 0 sh -c '"$1" w.c | grep "^x .* f " | awk "{ print index(\$0, \$4) }" > c1; "$1" w.c | sed -n "/^x .* f /{n;p;}" | awk "{ match(\$0, /[0-9]/); print RSTART }" > c2; cmp -s c1 c2 && echo 1' sh "$U"
t "all the references are there" '60' 0 sh -c '"$1" w.c | sed -n "/^x .* f /,\$p" | grep -o "[0-9][0-9]*" | wc -l | tr -d " "' sh "$U"
t "-w 60: no line wider" '0' 0 sh -c '"$1" -w 60 w.c | awk "length > 60" | wc -l | tr -d " "' sh "$U"
t "-w 60: more lines than -w 80" 'yes' 0 sh -c 'a=$("$1" -w 60 w.c | wc -l); b=$("$1" w.c | wc -l); [ $a -gt $b ] && echo yes' sh "$U"
t "-w less than 51: 80" "$("$U" -w 80 w.c)" 0 "$U" -w 30 w.c
t "-I: where headers are looked for; their symbols listed with their names" 'FROM_INC  inc/h.h  --       *1' 0 sh -c '"$1" -I inc i.c 2>/dev/null | grep "^FROM_INC .*h.h"' sh "$U"
t "a macro of a header used in the file" 'FROM_INC  i.c      --       2' 0 sh -c '"$1" -I inc i.c 2>/dev/null | grep "^FROM_INC .*i\\.c "' sh "$U"
t "-D name=def" 'three     i.c      --       *4' 0 sh -c '"$1" -I inc -D DEF=3 i.c | grep "^three "' sh "$U"
t "-U after -D" '' 1 sh -c '"$1" -I inc -D DEF=3 -U DEF i.c | grep "^three "' sh "$U"
t "a file that cannot be read: status 1, the others listed" 'n.c' 1 sh -c '"$1" nosuch.c n.c 2>/dev/null > o; s=$?; head -1 o; exit $s' sh "$U"
t "a file that cannot be read: a diagnostic" 'cxref: nosuch.c: No such file or directory' 1 sh -c '"$1" nosuch.c 2>&1 >/dev/null' sh "$U"
t "no file: a usage error" '' 2 "$U"
t "-w not a number: a usage error" '' 2 "$U" -w x m.c
t "an unknown option: a usage error" '' 2 "$U" -q m.c
# getopt() must not take an option after an operand for one (no permuting)
t "an option after an operand is an operand" 'cxref: -s: No such file or directory' 1 sh -c '"$1" m.c -s 2>&1 >/dev/null' sh "$U"
done_tests cxref
