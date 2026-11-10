#!/bin/sh
# test_xgettext.sh - tests for xgettext (POSIX.1-2024).  Run: sh test_xgettext.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/xgettext}; export PROG_DIR
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
U=${U:-$PROG_DIR/xgettext}
T=${TMPDIR:-/tmp}/test_xgettext.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
cat > a.c <<'X'
#include <stdio.h>
#include "not a msgid.h"
/* gettext("in a comment") */
int main(int argc, char **argv)
{
    printf(gettext("Hello, " "world\n"));   // gettext("in a comment too")
    printf(ngettext("%d file", "%d files", argc), argc);
    puts(gettext(("in parentheses: not taken")));
    puts(ngettext("plural not a literal", x, 2));
    puts(gettext(argv[0]));
    puts(gettext("two\nlines\n"));
    puts(gettext("Hello, world\n"));
    char c = '"';
    puts(gettext("tab\there \"q\" \\ \101"));
    puts(gettext(L"wide"));
    puts(other("not a keyword"));
    puts(gettext("a " \
"spliced"));
    puts(gettext(ngettext("inner", "inners", 2)));
    puts(gettext("x" + 1));
    puts(gettext(""));
    return 0;
}
X
t "the default file: messages.po" "msgid \"Hello, world\\n\"
msgstr \"\"

msgid \"%d file\"
msgid_plural \"%d files\"
msgstr[0] \"\"
msgstr[1] \"\"

msgid \"\"
\"two\\n\"
\"lines\\n\"
msgstr \"\"

# msgid \"Hello, world\\n\"
# msgstr \"\"

msgid \"tab\\there \\\"q\\\" \\\\ A\"
msgstr \"\"

msgid \"a spliced\"
msgstr \"\"

msgid \"inner\"
msgid_plural \"inners\"
msgstr[0] \"\"
msgstr[1] \"\"" 0 sh -c '"$1" a.c 2>/dev/null && cat messages.po' sh "$U"
t "nothing on the standard output" "" 0 "$U" a.c
t "the empty string: a warning" "1" 0 sh -c '"$1" a.c 2>&1 | grep -c "empty string"' sh "$U"
t "-n: the places" "#: a.c:6
#: a.c:7
#: a.c:11
#: a.c:12
#: a.c:14
#: a.c:17
#: a.c:19" 0 sh -c '"$1" -n a.c 2>/dev/null && grep "^#:" messages.po' sh "$U"
rm -f messages.po
t "-d: the default file's name" "ok" 0 sh -c '"$1" -d mine a.c 2>/dev/null && [ -f mine.po ] && [ ! -f messages.po ] && echo ok' sh "$U"
mkdir out
t "-p: the directory" "ok" 0 sh -c '"$1" -p out a.c 2>/dev/null && [ -f out/messages.po ] && echo ok' sh "$U"
t "-p and -d" "ok" 0 sh -c '"$1" -p out -d x a.c 2>/dev/null && [ -f out/x.po ] && echo ok' sh "$U"
# domains
cat > d.c <<'X'
f() {
    dgettext("dom", "in dom");
    dcgettext("dom", "in dom 2", LC_MESSAGES);
    dngettext("dom", "one", "ones", n);
    dgettext(var, "variable domain");
    dgettext("messages", "the default domain");
    gettext("plain");
}
X
t "d-functions: DOMAIN.po, starting with its domain" "domain \"dom\"

msgid \"in dom\"
msgstr \"\"

msgid \"in dom 2\"
msgstr \"\"

msgid \"one\"
msgid_plural \"ones\"
msgstr[0] \"\"
msgstr[1] \"\"" 0 sh -c 'rm -f *.po; "$1" d.c && cat dom.po' sh "$U"
t "d-functions without a literal domain: the default file" "msgid \"variable domain\"
msgstr \"\"

msgid \"the default domain\"
msgstr \"\"

msgid \"plain\"
msgstr \"\"" 0 cat messages.po
t "-K \"\" and the keywords of the standard's example: all in the default file" "in dom${nl}in dom 2${nl}one${nl}variable domain${nl}the default domain${nl}plain" 0 sh -c 'rm -f *.po; "$1" -K "" -K gettext:1 -K dgettext:2 -K dcgettext:2 -K ngettext:1,2 -K dngettext:2,3 -K dcngettext:2,3 d.c && [ ! -f dom.po ] && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
# -K
cat > k.c <<'X'
#define i18n gettext
f() {
    fprintf(stdout, i18n("The value is %s"), value1);
    N_("noop");
    P_(n, "sing", "plur");
    gettext("default kept");
    T_(1, 2, "third");
}
X
t "-K ID" "The value is %s${nl}noop${nl}default kept" 0 sh -c 'rm -f *.po; "$1" -K i18n -K N_ k.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-K ID:N1,N2" "msgid \"sing\"${nl}msgid_plural \"plur\"" 0 sh -c 'rm -f *.po; "$1" -K "" -K P_:2,3 k.c && grep "^msgid" messages.po' sh "$U"
t "-K ID:N" "third" 0 sh -c 'rm -f *.po; "$1" -K "" -K T_:3 k.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-K ID:N in octal and hexadecimal" "third" 0 sh -c 'rm -f *.po; "$1" -K "" -K T_:0x3 k.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-K \"\": the defaults off" "noop" 0 sh -c 'rm -f *.po; "$1" -K "" -K N_ k.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-K: a bad spec: usage" "" 2 "$U" -K "1x" k.c
t "-K: the same argument twice: usage" "" 2 "$U" -K "P_:2,2" k.c
t "-K: argument 0: usage" "" 2 "$U" -K "P_:0" k.c
# -a, -x
cat > s.c <<'X'
char *s = "one", *t = "two" "parts";
f() { gettext("three"); ngettext("four", "fours", 1); x("five"); dgettext("dm", "six"); }
X
t "-a: every string" "one${nl}twoparts${nl}three${nl}four${nl}fours${nl}five${nl}dm${nl}six" 0 sh -c 'rm -f *.po; "$1" -a s.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-a: no plurals" "0" 1 sh -c 'grep -c "^msgid_plural" messages.po' sh "$U"
t "-a: one file" "messages.po" 0 ls *.po
printf 'msgid "two"\n"parts"\nmsgstr ""\nmsgid "five"\nmsgstr "x"\n' > ex.po
t "-x: excluded msgids" "one${nl}three${nl}four${nl}fours${nl}dm${nl}six" 0 sh -c 'rm -f messages.po; "$1" -a -x ex.po s.c && sed -n "s/^msgid \"\\(.*\\)\"/\\1/p" messages.po' sh "$U"
t "-x: a file missing: status 1" "" 1 "$U" -a -x nofile.po s.c
# -j
printf 'gettext("old");\ngettext("both");\n' > j1.c
printf 'gettext("both");\ngettext("new");\n' > j2.c
t "-j: appended; those already there as comments" "msgid \"old\"
msgstr \"\"

msgid \"both\"
msgstr \"\"

# msgid \"both\"
# msgstr \"\"

msgid \"new\"
msgstr \"\"" 0 sh -c 'rm -f *.po; "$1" j1.c && "$1" -j j2.c && cat messages.po' sh "$U"
t "-j: a file that does not exist is made" "msgid \"new\"" 0 sh -c 'rm -f *.po; "$1" -j -d fresh j2.c && grep "^msgid \"new\"" fresh.po' sh "$U"
t "-j: its domain directive dropped" "0" 1 sh -c 'rm -f *.po; printf "domain \"x\"\nmsgid \"a\"\nmsgstr \"\"\n" > messages.po; "$1" -j j2.c && grep -c "^domain" messages.po' sh "$U"
t "without -j: overwritten" "msgid \"both\"${nl}msgid \"new\"" 0 sh -c 'rm -f *.po; "$1" j1.c && "$1" j2.c && grep "^msgid" messages.po' sh "$U"
# the standard input, errors
t "-: the standard input" "#: -:1${nl}msgid \"from stdin\"" 0 sh -c 'rm -f *.po; echo "gettext(\"from stdin\");" | "$1" -n - && sed -n "1,2p" messages.po' sh "$U"
t "a file missing: status 1" "" 1 "$U" missing.c
t "no operands: usage" "" 2 "$U"
t "an unknown option: usage" "" 2 "$U" -z a.c
t "output not writable: status 1" "" 1 "$U" -p /nonexistent/dir a.c
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: xgettext a.c -n: -n is a file" "" 1 sh -c 'rm -f *.po; "$1" a.c -n 2>/dev/null' sh "$U"

done_tests xgettext
