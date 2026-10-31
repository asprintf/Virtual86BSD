#!/bin/sh
# test_gettext.sh - tests for gettext (POSIX.1-2024).  Run: sh test_gettext.sh
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
U=${U:-$PWD/gettext}
T=${TMPDIR:-/tmp}/test_gettext.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
N=${N:-$(dirname "$U")/ngettext}
[ -x "$N" ] || ln -s gettext "$N" 2>/dev/null
MKMO="$TLBIN $OLDPWD/tests/mkmo.tl"
U8=${UTF8_LOCALE:-$(locale -a 2>/dev/null | grep -i -E '^c\.utf-?8$' | sed 1q)}
[ -n "$U8" ] || U8=$(locale -a 2>/dev/null | grep -i -E '\.utf-?8$' | sed 1q)
TAB=$(printf '\t')
unset LANGUAGE NLSPATH TEXTDOMAIN TEXTDOMAINDIR

# without a text domain, or in the C locale: the msgid
t "no text domain: the msgid" "hello" 0 "$U" hello
t "no text domain: no newline added" "68 65 6c 6c 6f" 0 sh -c '"$1" hello | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
t "an empty text domain: the msgid" "hello" 0 "$U" "" hello
t "ngettext, no text domain: n 1" "file" 0 "$N" file files 1
t "ngettext, no text domain: n 2" "files" 0 "$N" file files 2
t "ngettext, no text domain: n 0" "files" 0 "$N" file files 0
t "-s: separated by spaces, a newline" "a b c" 0 "$U" -s a b c
t "-s: the newline" "61 20 62 0a" 0 sh -c '"$1" -s a b | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
t "-s -n: no newline" "61 20 62" 0 sh -c '"$1" -s -n a b | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
# escapes
t "-e: escapes processed" "61 09 62 0a 07 5c" 0 sh -c '"$1" -e "a\\tb\\n\\a\\\\" | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
t "-e: octal and hexadecimal escapes" "AB" 0 "$U" -e '\101\x42'
t "-E: escapes as they are" 'a\tb' 0 "$U" -E 'a\tb'
t "neither: as they are" 'a\tb' 0 "$U" 'a\tb'
t "-s: escapes as they are by default" 'a\tb' 0 "$U" -s 'a\tb'
t "-s -e: escapes processed" "61 09 62 0a" 0 sh -c '"$1" -s -e "a\\tb" | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
t "-e: \\c ends the output" "ab" 0 "$U" -e 'ab\cde'
t "-s -e: \\c ends the output, no newline" "61 20 62" 0 sh -c '"$1" -s -e a "b\\cx" c | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$U"
t "ngettext -e: both strings" "61 09" 0 sh -c '"$1" -e "x" "a\\t" 2 | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//"' sh "$N"
# errors
t "gettext: no operands: usage" "" 2 "$U"
t "gettext: three operands: usage" "" 2 "$U" a b c
t "gettext -s: no msgid: usage" "" 2 "$U" -s
t "ngettext: two operands: usage" "" 2 "$N" a b
t "ngettext: five operands: usage" "" 2 "$N" d a b 1 2
t "ngettext: -s is not an option of it" "" 2 "$N" -s a b 1
t "ngettext: n not a number: status 1" "" 1 "$N" a b x
t "ngettext: n with something after: status 1" "" 1 "$N" a b 1x
t "an unknown option: usage" "" 2 "$U" -x a

if [ -n "$U8" ]; then
export LC_ALL="$U8"
mkdir -p loc/$U8/LC_MESSAGES loc/de/LC_MESSAGES loc/fr/LC_MESSAGES loc/de_DE/LC_MESSAGES nls
$MKMO loc/$U8/LC_MESSAGES/dom.mo <<X
${TAB}Content-Type: text/plain; charset=UTF-8\\nPlural-Forms: nplurals=4; plural=n==1?0: (n>1&&n<=10)?1: (n==0)?2:3;\\n
hello${TAB}hallo
recipient\\0recipients${TAB}1 recipient\\0two to ten recipients\\0no recipients\\0more than ten recipients
a\\tb${TAB}tabbed
X
$MKMO loc/$U8/LC_MESSAGES/other.mo <<X
hello${TAB}hello from other
X
$MKMO loc/de/LC_MESSAGES/dom.mo <<X
hello${TAB}hallo (de)
only de${TAB}nur de
X
$MKMO loc/fr/LC_MESSAGES/dom.mo <<X
hello${TAB}bonjour
only fr${TAB}seulement fr
X
$MKMO loc/de_DE/LC_MESSAGES/dom.mo <<X
hello${TAB}hallo (de_DE)
X
TEXTDOMAINDIR=$T/loc; export TEXTDOMAINDIR

t "-d: the translation" "hallo" 0 "$U" -d dom hello
t "the text domain operand" "hallo" 0 "$U" dom hello
t "TEXTDOMAIN" "hallo" 0 env TEXTDOMAIN=dom "$U" hello
t "-d before TEXTDOMAIN" "hello from other" 0 env TEXTDOMAIN=dom "$U" -d other hello
t "the operand before -d" "hallo" 0 "$U" -d other dom hello
t "not in the messages object: the msgid" "absent" 0 "$U" -d dom absent
t "no such domain: the msgid" "hello" 0 "$U" -d nodom hello
t "-s: each msgid" "hallo absent hallo" 0 "$U" -s -d dom hello absent hello
t "-e: the msgid after its escapes" "tabbed" 0 "$U" -e -d dom 'a\tb'
t "-E: not found as it is" 'a\tb' 0 "$U" -E -d dom 'a\tb'
t "the C locale: no translation" "hello" 0 env LC_ALL=C "$U" -d dom hello
t "the POSIX locale: no translation, also with LANGUAGE" "hello" 0 env LC_ALL=POSIX LANGUAGE=de "$U" -d dom hello
# the plural forms (the standard's example)
t "ngettext: 0" "no recipients" 0 "$N" -d dom recipient recipients 0
t "ngettext: 1" "1 recipient" 0 "$N" -d dom recipient recipients 1
t "ngettext: 5" "two to ten recipients" 0 "$N" -d dom recipient recipients 5
t "ngettext: 11" "more than ten recipients" 0 "$N" -d dom recipient recipients 11
t "ngettext: the operand domain" "1 recipient" 0 "$N" dom recipient recipients 1
t "ngettext: not found, 1: msgid" "Call" 0 "$N" -d dom Call Calls 1
t "ngettext: not found, 0: msgid_plural" "Calls" 0 "$N" -d dom Call Calls 0
t "ngettext: not found, 10: msgid_plural" "Calls" 0 "$N" -d dom Call Calls 10
t "ngettext: n as strtoul (leading blanks)" "1 recipient" 0 "$N" -d dom recipient recipients ' 1'
t "gettext of a plural message: the first form" "1 recipient" 0 "$U" -d dom recipient
t "gettext -s of a plural message" "1 recipient" 0 "$U" -s -d dom recipient
# LANGUAGE
t "LANGUAGE" "hallo (de)" 0 env LANGUAGE=de "$U" -d dom hello
t "LANGUAGE: the first that exists" "bonjour" 0 env LANGUAGE=xx:fr:de "$U" -d dom hello
t "LANGUAGE: the next when the message is not there" "seulement fr" 0 env LANGUAGE=de:fr "$U" -d dom "only fr"
t "LANGUAGE: shorter forms of a name" "hallo (de_DE)" 0 env LANGUAGE=de_DE.UTF-8@euro "$U" -d dom hello
t "LANGUAGE: the language alone" "hallo (de)" 0 env LANGUAGE=de_AT "$U" -d dom hello
t "LANGUAGE: none found: the locale's" "hallo" 0 env LANGUAGE=xx:yy "$U" -d dom hello
t "LANGUAGE: names with slashes left out" "hallo" 0 env LANGUAGE=../loc/de "$U" -d dom hello
t "LANGUAGE empty: the locale's" "hallo" 0 env LANGUAGE= "$U" -d dom hello
t "LANGUAGE: ngettext" "seulement fr" 0 env LANGUAGE=fr "$N" -d dom "only fr" x 1
t "LANGUAGE led to an object without the message: not the locale's" "recipient" 0 env LANGUAGE=fr "$U" -d dom recipient
t "ngettext of a message without plural forms: its translation for any n" "hallo hallo" 0 sh -c 'a=$("$1" -d dom hello hellos 1); b=$("$1" -d dom hello hellos 5); echo "$a $b"' sh "$N"
# NLSPATH
cp loc/fr/LC_MESSAGES/dom.mo nls/dom.mo
cp loc/de/LC_MESSAGES/dom.mo "nls/$U8.mo"
t "NLSPATH: %N" "bonjour" 0 env NLSPATH="$T/nls/%N.mo" "$U" -d dom hello
t "NLSPATH: before LANGUAGE and TEXTDOMAINDIR" "bonjour" 0 env NLSPATH="$T/nls/%N.mo" LANGUAGE=de "$U" -d dom hello
t "NLSPATH: %L" "hallo (de)" 0 env NLSPATH="$T/nls/%L.mo" "$U" -d dom hello
t "NLSPATH: the first template that names a messages object" "bonjour" 0 env NLSPATH="$T/nls/none%N:$T/nls/%N.mo:$T/nls/%L.mo" "$U" -d dom hello
t "NLSPATH: an empty template is %N" "bonjour" 0 sh -c 'cd nls && NLSPATH=":x" "$1" -d dom.mo hello' sh "$U"
t "NLSPATH: %% is a %" "bonjour" 0 sh -c 'cp nls/dom.mo "nls/a%b" && NLSPATH="$1/nls/a%%b" "$2" -d dom hello' sh "$T" "$U"
t "NLSPATH: %l %t %c" "bonjour" 0 sh -c 'cp nls/dom.mo "nls/x-$(printf %s "$LC_ALL" | sed "s/\\..*//")-$(printf %s "$LC_ALL" | sed "s/.*\\.//").mo" && NLSPATH="$1/nls/x-%l%t-%c.mo" "$2" -d dom hello' sh "$T" "$U"
t "NLSPATH: an object without the message: the msgid" "nur de" 0 env NLSPATH="$T/nls/%L.mo" "$U" -d dom "nur de"
t "NLSPATH: no object: LANGUAGE then" "hallo (de)" 0 env NLSPATH="$T/nls/none" LANGUAGE=de "$U" -d dom hello
# messages objects
printf 'not a messages object\n' > loc/fr/LC_MESSAGES/bad.mo
mkdir -p loc/de/LC_MESSAGES
$MKMO loc/de/LC_MESSAGES/bad.mo <<X
hello${TAB}hallo (good)
X
t "not a messages object: the next form" "hallo (good)" 0 env LANGUAGE=fr:de "$U" -d bad hello
$MKMO -b loc/$U8/LC_MESSAGES/big.mo <<X
hello${TAB}big-endian
X
t "a big-endian messages object" "big-endian" 0 "$U" -d big hello
$MKMO -n loc/$U8/LC_MESSAGES/unsorted.mo <<X
zeta${TAB}z
alpha${TAB}a
mid${TAB}m
X
t "an unsorted messages object" "a z m" 0 "$U" -s -d unsorted alpha zeta mid
# the codeset
$MKMO loc/$U8/LC_MESSAGES/latin.mo <<X
${TAB}Content-Type: text/plain; charset=ISO-8859-1\\n
cafe${TAB}caf\\xe9
X
t "converted from the object's charset" "c3 a9" 0 sh -c '"$1" -d latin cafe | od -An -tx1 | tr -s " " | sed "s/^ //; s/ \$//" | sed "s/.* \\(c3 a9\\)\$/\\1/"' sh "$U"
$MKMO loc/$U8/LC_MESSAGES/nocs.mo <<X
${TAB}Content-Type: text/plain; charset=NO-SUCH-CODESET\\n
cafe${TAB}caf\\xe9
X
t "a charset that cannot be converted: the msgid" "cafe" 0 "$U" -d nocs cafe
# plural forms that cannot be used
$MKMO loc/$U8/LC_MESSAGES/pl.mo <<X
${TAB}Plural-Forms: nplurals=2; plural=n==1?0:7;\\n
one\\0many${TAB}eins\\0viele
X
t "a form out of range: as not found" "many" 0 "$N" -d pl one many 2
t "a form in range" "eins" 0 "$N" -d pl one many 1
$MKMO loc/$U8/LC_MESSAGES/div.mo <<X
${TAB}Plural-Forms: nplurals=2; plural=1/(n-5);\\n
one\\0many${TAB}eins\\0viele
X
t "a division by zero: as not found" "many" 0 "$N" -d div one many 5
t "no division by zero: the form" "viele" 0 "$N" -d div one many 6
$MKMO loc/$U8/LC_MESSAGES/noplural.mo <<X
one\\0many${TAB}eins\\0viele
X
t "no plural forms in the header: n 1 the first" "eins" 0 "$N" -d noplural one many 1
t "no plural forms in the header: else the second" "viele" 0 "$N" -d noplural one many 3
fi

# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: gettext dom -s: -s is the msgid" "-s" 0 "$U" dom -s

done_tests gettext
