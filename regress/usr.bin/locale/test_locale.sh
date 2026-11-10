#!/bin/sh
# test_locale.sh - tests for locale (POSIX.1-2024).  Run: sh test_locale.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/locale}; export PROG_DIR
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
U=${U:-$PROG_DIR/locale}
T=${TMPDIR:-/tmp}/test_locale.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
unset LANG LC_ALL LC_CTYPE LC_COLLATE LC_TIME LC_NUMERIC LC_MONETARY LC_MESSAGES
export LC_ALL=POSIX

# the values of the POSIX locale (XBD 7.3)
t "the standard's example: -ck decimal_point" "LC_NUMERIC${nl}decimal_point=\".\"" 0 "$U" -ck decimal_point
t "a keyword without -k: its value" "." 0 "$U" decimal_point
t "yesexpr" "^[yY]" 0 "$U" yesexpr
t "noexpr" "^[nN]" 0 "$U" noexpr
t "LC_MESSAGES -k" "yesexpr=\"^[yY]\"${nl}noexpr=\"^[nN]\"" 0 "$U" -k LC_MESSAGES
t "LC_NUMERIC -k" "decimal_point=\".\"${nl}thousands_sep=\"\"${nl}grouping=-1" 0 "$U" -k LC_NUMERIC
t "LC_MONETARY -k" 'int_curr_symbol=""
currency_symbol=""
mon_decimal_point=""
mon_thousands_sep=""
mon_grouping=-1
positive_sign=""
negative_sign=""
int_frac_digits=-1
frac_digits=-1
p_cs_precedes=-1
p_sep_by_space=-1
n_cs_precedes=-1
n_sep_by_space=-1
p_sign_posn=-1
n_sign_posn=-1
int_p_cs_precedes=-1
int_p_sep_by_space=-1
int_n_cs_precedes=-1
int_n_sep_by_space=-1
int_p_sign_posn=-1
int_n_sign_posn=-1' 0 "$U" -k LC_MONETARY
t "LC_TIME -k" 'abday="Sun;Mon;Tue;Wed;Thu;Fri;Sat"
day="Sunday;Monday;Tuesday;Wednesday;Thursday;Friday;Saturday"
abmon="Jan;Feb;Mar;Apr;May;Jun;Jul;Aug;Sep;Oct;Nov;Dec"
ab_alt_mon="Jan;Feb;Mar;Apr;May;Jun;Jul;Aug;Sep;Oct;Nov;Dec"
mon="January;February;March;April;May;June;July;August;September;October;November;December"
alt_mon="January;February;March;April;May;June;July;August;September;October;November;December"
d_t_fmt="%a %b %e %H:%M:%S %Y"
d_fmt="%m/%d/%y"
t_fmt="%H:%M:%S"
am_pm="AM;PM"
t_fmt_ampm="%I:%M:%S %p"
era=""
era_d_fmt=""
era_t_fmt=""
era_d_t_fmt=""
alt_digits=""' 0 "$U" -k LC_TIME
t "a list without -k" "Sun;Mon;Tue;Wed;Thu;Fri;Sat" 0 "$U" abday
t "a number without -k" "-1" 0 "$U" frac_digits
t "several names, in their order" "^[nN]${nl}.${nl}%H:%M:%S" 0 "$U" noexpr decimal_point t_fmt
t "-c: the category before each" "LC_MESSAGES${nl}^[nN]${nl}LC_NUMERIC${nl}." 0 "$U" -c noexpr decimal_point
t "-c with a category" "LC_NUMERIC${nl}.${nl}${nl}-1" 0 "$U" -c LC_NUMERIC
t "LC_CTYPE: no keyword values" "LC_CTYPE" 0 "$U" -ck LC_CTYPE
t "LC_COLLATE: no keyword values" "" 0 "$U" -k LC_COLLATE
t "an LC_CTYPE keyword: no value" "" 0 "$U" -k upper
t "charmap: the codeset" "ok" 0 sh -c '[ "$("$1" -k charmap)" = "charmap=\"$("$1" charmap)\"" ] && [ -n "$("$1" charmap)" ] && echo ok' sh "$U"
t "charmap -c: under LC_CTYPE" "LC_CTYPE" 0 sh -c '"$1" -c charmap | sed 1q' sh "$U"
t "an unknown name: status 1" "" 1 "$U" nosuchkeyword
t "an unknown name: a message" 'locale: unknown name "nosuchkeyword"' 0 sh -c '"$1" nosuchkeyword 2>&1; true' sh "$U"
t "an unknown name among others: the others written" "." 1 "$U" nosuchkeyword decimal_point
# without operands: the environment
t "no operands: all implied" 'LANG=
LC_CTYPE="POSIX"
LC_COLLATE="POSIX"
LC_TIME="POSIX"
LC_NUMERIC="POSIX"
LC_MONETARY="POSIX"
LC_MESSAGES="POSIX"
LC_ALL=POSIX' 0 "$U"
t "the standard's example: LANG and LC_COLLATE" 'LANG=locale_x
LC_CTYPE="locale_x"
LC_COLLATE=locale_y
LC_TIME="locale_x"
LC_NUMERIC="locale_x"
LC_MONETARY="locale_x"
LC_MESSAGES="locale_x"
LC_ALL=' 0 sh -c 'unset LC_ALL; LANG=locale_x LC_COLLATE=locale_y "$1" 2>/dev/null' sh "$U"
t "LC_ALL overrides: implied" 'LANG=x
LC_CTYPE="C"
LC_COLLATE="C"
LC_TIME="C"
LC_NUMERIC="C"
LC_MONETARY="C"
LC_MESSAGES="C"
LC_ALL=C' 0 env LANG=x LC_TIME=y LC_ALL=C "$U"
t "nothing set: POSIX implied" 'LANG=
LC_CTYPE="POSIX"
LC_COLLATE="POSIX"
LC_TIME="POSIX"
LC_NUMERIC="POSIX"
LC_MONETARY="POSIX"
LC_MESSAGES="POSIX"
LC_ALL=' 0 sh -c 'unset LC_ALL; "$1"' sh "$U"
t "an empty variable is not set" 'LANG=
LC_CTYPE="x"
LC_COLLATE="x"
LC_TIME="x"
LC_NUMERIC="x"
LC_MONETARY="x"
LC_MESSAGES="x"
LC_ALL=' 0 sh -c 'unset LC_ALL; LANG= LC_ALL= LC_CTYPE= "$1" | sed "s/\"POSIX\"/\"x\"/"' sh "$U"
t "values quoted for the shell" "LANG='a b'${nl}LC_CTYPE=\"a b\"" 0 sh -c 'unset LC_ALL; LANG="a b" "$1" 2>/dev/null | sed -n 1,2p' sh "$U"
t "a quote in a value" "LANG='it'\\''s'" 0 sh -c 'unset LC_ALL; LANG="it'"'"'s" "$1" 2>/dev/null | sed -n 1p' sh "$U"
t "the output can be read back by the shell" "ok" 0 sh -c 'unset LC_ALL; LANG="a b" LC_TIME="c\$d" "$1" 2>/dev/null > env.sh && . ./env.sh && [ "$LANG" = "a b" ] && [ "$LC_TIME" = "c\$d" ] && [ "$LC_CTYPE" = "a b" ] && echo ok' sh "$U"
t "a locale that cannot be used: a warning, status 0" "ok" 0 sh -c 'unset LC_ALL; LANG=no_such_locale "$1" decimal_point 2>err >out && grep -q warning err && [ "$(cat out)" = . ] && echo ok' sh "$U"
# -a and -m
t "-a: C and POSIX" "C${nl}POSIX" 0 sh -c '"$1" -a | grep -x -e C -e POSIX' sh "$U"
t "-a: sorted, each once" "ok" 0 sh -c '"$1" -a > l1 && LC_ALL=C sort -u l1 > l2 && cmp -s l1 l2 && echo ok' sh "$U"
t "-a: each one usable" "ok" 0 sh -c 'for l in $("$1" -a); do [ "$(LC_ALL=$l "$1" decimal_point 2>&1)" != "" ] || exit 1; LC_ALL=$l "$1" 2>&1 >/dev/null | grep -q warning && exit 1; done; echo ok' sh "$U"
t "-m: names, sorted" "ok" 0 sh -c '"$1" -m > m1 && LC_ALL=C sort -u m1 > m2 && cmp -s m1 m2 && ! grep -q -e "\\.gz\$" -e "/" m1 && echo ok' sh "$U"
# usage
t "-a with -m: usage" "" 2 "$U" -a -m
t "-a with a name: usage" "" 2 "$U" -a decimal_point
t "-k without names: usage" "" 2 "$U" -k
t "-c without names: usage" "" 2 "$U" -c
t "an unknown option: usage" "" 2 "$U" -x
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: locale decimal_point -k: -k is a name" "." 1 "$U" decimal_point -k

done_tests locale
