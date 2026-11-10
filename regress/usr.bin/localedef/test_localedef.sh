#!/bin/sh
# test_localedef.sh - tests for localedef (POSIX.1-2024).  Run: sh test_localedef.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/localedef}; export PROG_DIR
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
U=${U:-$PROG_DIR/localedef}
T=${TMPDIR:-/tmp}/test_localedef.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../../../usr.bin/tl/tl.c; ../../../usr.bin/tl/tl.md)
TL=$(cd ../../lib/testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/../../../usr.bin/tl/tl.c" || { echo "cannot compile ../../../usr.bin/tl/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# the files of a locale
files='LC_ADDRESS LC_COLLATE LC_CTYPE LC_IDENTIFICATION LC_MEASUREMENT LC_MESSAGES/SYS_LC_MESSAGES LC_MONETARY LC_NAME LC_NUMERIC LC_PAPER LC_TELEPHONE LC_TIME'
# allfiles DIR: all the files are there
allfiles() { for _f in $files; do [ -f "$1/$_f" ] || return 1; done; }
# nofile DIR CATEGORY: that file is not there
nofile() { [ ! -f "$1/$2" ]; }
# in LOCALE COMMAND...: COMMAND in the locale made in $T (glibc's LOCPATH)
cat > in.sh <<'EOF'
l=$1; shift
LOCPATH=$(pwd) LC_ALL=$l exec "$@"
EOF
# glibc reads the files: a locale made is used by the utilities
glibc=false; getconf GNU_LIBC_VERSION > /dev/null 2>&1 && glibc=true

# a charmap: ASCII (as <Uxxxx>), E and e with acute
{ echo '<code_set_name> TESTMAP'; echo '<comment_char> %'; echo '<escape_char> /'
  echo '<mb_cur_min> 1'; echo '<mb_cur_max> 1'; echo '% the characters'; echo CHARMAP
  awk 'BEGIN { for (i = 0; i < 128; i++) printf "<U%04X> /x%02x\n", i, i }'
  echo '<U00C9> /xc9 LATIN CAPITAL LETTER E WITH ACUTE'
  echo '<U00E9> /xe9 LATIN SMALL LETTER E WITH ACUTE'
  echo 'END CHARMAP'; } > cm
cat > base <<'EOF'
comment_char %
escape_char /
% a test locale
LC_CTYPE
upper <U0041>..<U005A>;<U00C9>
lower <U0061>..<U007A>;<U00E9>
class "vowel";<U0061>;<U0065>;<U0069>;<U006F>;<U0075>;<U00E9>
toupper (<U0061>,<U0041>);(<U0062>,<U0042>);(<U0063>,<U0043>);(<U00E9>,<U00C9>)
tolower (<U0041>,<U0061>);(<U0042>,<U0062>);(<U0043>,<U0063>);(<U00C9>,<U00E9>)
translit_start
<U00FC> "<U0075><U0065>"
translit_end
END LC_CTYPE
LC_COLLATE
collating-element <ch> from "<U0063><U0068>"
order_start forward
<U002D> IGNORE
<U0062>
<U0061>
<U0063>
<ch>
<U0064>
UNDEFINED
order_end
END LC_COLLATE
LC_NUMERIC
decimal_point "<U002C>"
thousands_sep "<U002E>"
grouping 3
END LC_NUMERIC
LC_TIME
abday "d1";"d2";"d3";"d4";"d5";"d6";"d7"
day "day1";"day2";"day3";"day4";"day5";"day6";"day7"
abmon "m1";"m2";"m3";"m4";"m5";"m6";"m7";"m8";"m9";"m10";"m11";"m12"
mon "mon1";"mon2";"mon3";"mon4";"mon5";"mon6";"mon7";"mon8";"mon9";"mon10";"mon11";"m<U00FC>n12"
d_t_fmt "%a %d %b %Y %T"
d_fmt "%d.%m.%Y"
t_fmt "%T"
am_pm "";""
t_fmt_ampm ""
END LC_TIME
LC_MESSAGES
yesexpr "^[jJ]"
noexpr "^[nN]"
END LC_MESSAGES
EOF
cat > rest <<'EOF'
LC_MONETARY
int_curr_symbol "EUR "
currency_symbol "E"
mon_decimal_point ","
mon_thousands_sep "."
mon_grouping 3
positive_sign ""
negative_sign "-"
int_frac_digits 2
frac_digits 2
p_cs_precedes 0
p_sep_by_space 1
n_cs_precedes 0
n_sep_by_space 1
p_sign_posn 1
n_sign_posn 1
END LC_MONETARY
LC_PAPER
height 297
width 210
END LC_PAPER
LC_NAME
name_fmt "%g %f"
END LC_NAME
LC_ADDRESS
postal_fmt "%a%N%f%N%s %h%N%z %T%N%c%N"
country_ab2 "DE"
country_ab3 "DEU"
country_num 276
END LC_ADDRESS
LC_TELEPHONE
tel_int_fmt "+%c %a %l"
int_prefix "49"
END LC_TELEPHONE
LC_MEASUREMENT
measurement 1
END LC_MEASUREMENT
LC_IDENTIFICATION
title "test"
source "test"
address "test"
contact "test"
email "test"
tel ""
fax ""
language "test"
territory "test"
revision "1.0"
date "2026-10-29"
EOF
for c in IDENTIFICATION CTYPE COLLATE TIME NUMERIC MONETARY MESSAGES PAPER NAME ADDRESS TELEPHONE MEASUREMENT; do
    echo "category \"i18n:2012\";LC_$c"
done >> rest
echo 'END LC_IDENTIFICATION' >> rest
cat base rest > full

# ---- making a locale ----
t "a full definition: status 0" '' 0 "$U" -f ./cm -i full "$T/loc"
ok "a full definition: the files of all the categories" allfiles loc
t "a full definition: nothing on the standard error" '' 0 sh -c '"$1" -f ./cm -i full "$2" 2>&1' sh "$U" "$T/loc2"
t "missing categories: warnings, status 1" '' 1 "$U" -f ./cm -i base "$T/part"
ok "missing categories: the files are made" allfiles part
t "missing categories: the warning names the category" '1' 0 sh -c '"$1" -f ./cm -i base "$2" 2>&1 >/dev/null | grep -c "LC_PAPER"' sh "$U" "$T/part2"
t "the definition from the standard input" '' 0 sh -c '"$1" -f ./cm "$2" < full' sh "$U" "$T/sin"
ok "the definition from the standard input: the files" allfiles sin
ok "the standard input and -i: the same files" sh -c 'for f in $1; do cmp -s "loc/$f" "sin/$f" || exit 1; done' sh "$files"
t "a remade locale: the same files" '' 0 sh -c '"$1" -f ./cm -i full "$2" && for f in $3; do cmp -s "loc/$f" "loc3/$f" || exit 1; done' sh "$U" "$T/loc3" "$files"
t "the directory is made" 'yes' 0 sh -c '"$1" -f ./cm -i full "$2/newdir" && [ -d newdir ] && echo yes' sh "$U" "$T"
mkdir src.d; cp full src.d/mine
t "-i: a name found in I18NPATH/locales" '' 0 sh -c 'mkdir -p i18n/locales && cp full i18n/locales/mine && I18NPATH="$2/i18n" "$1" -f ./cm -i mine "$2/fromi18n"' sh "$U" "$T"
ok "-i: a name found in I18NPATH/locales: the files" allfiles fromi18n

# ---- copy ----
cat > numsrc <<'EOF'
LC_NUMERIC
decimal_point "<U003B>"
thousands_sep ""
grouping -1
END LC_NUMERIC
EOF
sed '/^LC_NUMERIC/,/^END LC_NUMERIC/c\
LC_NUMERIC\
copy "'"$T"'/numsrc"\
END LC_NUMERIC' full > withcopy
t "copy: status 0" '' 0 "$U" -f ./cm -i withcopy "$T/cp"
ok "copy: the category is that of the copied locale" sh -c '! cmp -s loc/LC_NUMERIC cp/LC_NUMERIC'
sed 's|^copy ".*"|copy "'"$T"'/nonesuch"|' withcopy > badcopy
t "copy of a locale that is not there: status 4" '' 4 "$U" -f ./cm -i badcopy "$T/badcp"
ok "copy of a locale that is not there: no LC_NUMERIC" nofile badcp LC_NUMERIC

# ---- errors ----
sed '/^decimal_point/d' full > nodp
t "an error (no decimal_point): status 4" '' 4 "$U" -f ./cm -i nodp "$T/err"
ok "an error: no files" nofile err LC_CTYPE
t "an error: a message on the standard error" '1' 0 sh -c '"$1" -f ./cm -i nodp "$2" 2>&1 | grep -c decimal_point' sh "$U" "$T/err2"
t "-c: an error, the locale made anyway, status 1" '' 1 "$U" -c -f ./cm -i nodp "$T/forced"
ok "-c: the files" allfiles forced
t "a charmap that is not there: status 4" '' 4 "$U" -f ./nonesuch -i full "$T/nocm"
ok "a charmap that is not there: no files" nofile nocm LC_CTYPE
t "a definition file that is not there: status 4" '' 4 "$U" -f ./cm -i ./nonesuch "$T/nosrc"
ok "a definition file that is not there: no files" nofile nosrc LC_CTYPE
printf 'LC_NUMERIC\ndecimal_point "<U002C>"\nthousands_sep ""\ngrouping -1\nbogus 1\nEND LC_NUMERIC\n' > syn
t "a syntax error: a warning, status 1" '' 1 "$U" -f ./cm -i syn "$T/synout"
t "a syntax error: the line in the message" '1' 0 sh -c '"$1" -f ./cm -i syn "$2" 2>&1 | grep -c "syn:5:.*syntax error"' sh "$U" "$T/synout2"
t "no operand: a usage message" '1' 0 sh -c '"$1" -f ./cm -i full 2>&1 | grep -c "^usage:"' sh "$U"
t "no operand: status 4" '' 4 "$U" -f ./cm -i full
t "two operands: status 4" '' 4 "$U" -f ./cm -i full "$T/a1" "$T/a2"
ok "two operands: nothing made" sh -c '[ ! -d a1 ] && [ ! -d a2 ]'
t "an unknown option: status 4" '' 4 "$U" -q -f ./cm -i full "$T/q"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: localedef -i full NAME -c: -c is an operand" '' 4 "$U" -f ./cm -i full "$T/perm" -c
ok "options after operands: nothing made" sh -c '[ ! -d perm ] && [ ! -d ./-c ]'

# ---- the locale made, in use (glibc reads the files) ----
if $glibc; then
t "use: the charmap" 'TESTMAP' 0 sh in.sh loc locale charmap
t "use: toupper" 'ABC' 0 sh -c 'echo abc | sh in.sh loc tr "[:lower:]" "[:upper:]"'
t "use: toupper of an 8-bit character" ' 311  \n' 0 sh -c 'printf "\351\n" | sh in.sh loc tr "[:lower:]" "[:upper:]" | od -An -c'
t "use: tolower" 'abc' 0 sh -c 'echo ABC | sh in.sh loc tr "[:upper:]" "[:lower:]"'
t "use: an 8-bit character is alpha" '1' 0 sh -c 'printf "\351x\n" | sh in.sh loc grep -c "^[[:alpha:]]x"'
# (grep and tr of GNU know only the classes of POSIX: iswctype() asked)
cat > wct.tl <<'EOF'
/* wct.tl LOCALE CLASS CHARACTERS: those of the characters in the class */
setlocale(argv[0]) != nil || exit(1);
var found = [];
for (var c : argv[2])
    if (iswctype(c, argv[1]))
        push(found, chr(c));
print(join(found, " "));
EOF
t "use: a class of its own (class \"vowel\")" 'a e i' 0 env LOCPATH="$T" "$TLBIN" wct.tl loc vowel abeiz
t "use: no class of that name" '' 0 env LOCPATH="$T" "$TLBIN" wct.tl loc nonesuch abeiz
t "use: the collation order" 'b a c' 0 sh -c 'printf "a\nb\nc\n" | sh in.sh loc sort | tr "\n" " " | sed "s/ $//"'
t "use: a collating element (ch after c)" 'c cd ch d' 0 sh -c 'printf "d\nch\ncd\nc\n" | sh in.sh loc sort | tr "\n" " " | sed "s/ $//"'
t "use: IGNORE (- not looked at)" 'a -c' 0 sh -c 'printf -- "-c\na\n" | sh in.sh loc sort | tr "\n" " " | sed "s/ $//"'
t "use: the decimal point" '2,5' 0 sh in.sh loc printf '%.1f\n' 2.5
t "use: locale -k" 'thousands_sep="."' 0 sh in.sh loc locale -k thousands_sep
t "use: the days" 'day1;day2;day3;day4;day5;day6;day7' 0 sh in.sh loc locale day
t "use: date" '1' 0 sh -c 'sh in.sh loc date -u +%A | grep -c "^day[1-7]\$"'
t "use: yesexpr" '^[jJ]' 0 sh in.sh loc locale yesexpr
t "use: the currency" 'EUR ' 0 sh in.sh loc locale int_curr_symbol
t "use: transliteration of a character the charmap does not have" 'mon1;mon2;mon3;mon4;mon5;mon6;mon7;mon8;mon9;mon10;mon11;muen12' 0 sh in.sh loc locale mon
t "use: copy" ';' 0 sh in.sh cp locale decimal_point
t "use: the locale made with missing categories (POSIX values)" '.' 0 sh in.sh part locale mon_decimal_point
fi

done_tests localedef
