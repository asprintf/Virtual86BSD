#!/bin/sh
# test_msgfmt.sh - tests for msgfmt (POSIX.1-2024).  Run: sh test_msgfmt.sh
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
U=${U:-$PWD/msgfmt}
T=${TMPDIR:-/tmp}/test_msgfmt.$$
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
RD="$TLBIN $OLDPWD/tests/readmo.tl"
cat > a.po <<'X'
# a translator's comment
#: src.c:1
msgid "hello"
msgstr "hallo"

msgid "world"
msgstr "welt"
X
t "messages.mo by default" "hello => hallo${nl}world => welt${nl}hash 5${nl}ok" 0 sh -c '"$1" a.po && $2 messages.mo' sh "$U" "$RD"
t "nothing on the standard output" "" 0 "$U" a.po
t "the magic number" "ok" 0 sh -c '"$1" a.po && od -An -tx1 messages.mo | sed 1q | grep -q -e "^ de 12 04 95" -e "^ 95 04 12 de" && echo ok' sh "$U"
rm -f messages.mo
cat > d.po <<'X'
domain "one"
msgid "a"
msgstr "A"
domain "two"
msgid "b"
msgstr "B"
domain "one"
msgid "c"
msgstr "C"
X
t "domains: a file each, sections joined" "a => A${nl}c => C${nl}hash 5${nl}ok${nl}b => B${nl}hash 3${nl}ok" 0 sh -c '"$1" d.po && $2 one.mo && $2 two.mo' sh "$U" "$RD"
t "domains: no messages.mo" "no" 0 sh -c '[ -f messages.mo ] && echo yes || echo no'
t "-o: one file, the domains ignored" "a => A${nl}b => B${nl}c => C${nl}hash 5${nl}ok${nl}0" 0 sh -c 'rm -f one.mo two.mo; "$1" -o out d.po && $2 out && ls one.mo two.mo 2>/dev/null | wc -l | tr -d " "' sh "$U" "$RD"
t "-o -S: .mo added" "ok" 0 sh -c '"$1" -S -o x d.po && [ -f x.mo ] && echo ok' sh "$U"
t "-o x.mo -S: not twice" "ok" 0 sh -c '"$1" -S -o y.mo d.po && [ -f y.mo ] && [ ! -f y.mo.mo ] && echo ok' sh "$U"
t "-S without -o: DOMAIN.mo" "ok" 0 sh -c 'rm -f one.mo; "$1" -S d.po && [ -f one.mo ] && echo ok' sh "$U"
cat > h.po <<'X'
msgid ""
msgstr ""
"Content-Type: text/plain; charset=UTF-8\n"
"Plural-Forms: nplurals=3; plural=n==1 ? 0 : n==2 ? 1 : 2;\n"

msgid "file"
msgid_plural "files"
msgstr[0] "one file"
msgstr[1] "two files"
msgstr[2] "many files"
X
t "a header and plural forms" " => Content-Type: text/plain; charset=UTF-8\nPlural-Forms: nplurals=3; plural=n==1 ? 0 : n==2 ? 1 : 2;\n${nl}file|files => one file|two files|many files${nl}hash 5${nl}ok" 0 sh -c '"$1" -o h.mo h.po && $2 h.mo' sh "$U" "$RD"
cat > e.po <<'X'
msgid "multi"
"line"
msgstr ""
"a\tb\n"
  "\101\x42\"\\"
X
t "continuation lines and escapes" "multiline => a	b\nAB\"\\${nl}hash 3${nl}ok" 0 sh -c '"$1" -o e.mo e.po && $2 e.mo' sh "$U" "$RD"
cat > f.po <<'X'
msgid ""
msgstr "Plural-Forms: nplurals=1; plural=0;\n"

#, fuzzy
msgid "maybe"
msgstr "vielleicht"

#, c-format, fuzzy
msgid "maybe %d"
msgstr "vielleicht %d"

msgid "untranslated"
msgstr ""

msgid "sure"
msgstr "sicher"
X
t "fuzzy and untranslated messages left out" "sure => sicher" 0 sh -c '"$1" -o f.mo f.po && $2 f.mo | sed -n "/=>/p" | sed 1d' sh "$U" "$RD"
t "-f: fuzzy messages in" "maybe => vielleicht${nl}maybe %d => vielleicht %d${nl}sure => sicher" 0 sh -c '"$1" -f -o f.mo f.po && $2 f.mo | sed -n "/=>/p" | sed 1d' sh "$U" "$RD"
printf '#, fuzzy\nmsgid ""\nmsgstr "Project: x\\n"\n\nmsgid "a"\nmsgstr "b"\n' > fh.po
t "a fuzzy header is kept" " => Project: x\n${nl}a => b" 0 sh -c '"$1" -o fh.mo fh.po && $2 fh.mo | sed -n "/=>/p"' sh "$U" "$RD"
printf 'msgctxt "menu"\nmsgid "Open"\nmsgstr "Öffnen"\n' > c.po
t "msgctxt (an extension): context EOT msgid" "menu004Open\\n" 0 sh -c '"$1" -o c.mo c.po && $2 c.mo | sed -n 1p | sed "s/ =>.*//" | od -An -c | tr -d " \n"' sh "$U" "$RD"
printf 'msgid "x"\nmsgstr "y"\n#~ msgid "old"\n#~ msgstr "alt"\n' > o.po
t "obsolete messages (#~) are comments" "x => y" 0 sh -c '"$1" -o o.mo o.po && $2 o.mo | sed -n "/=>/p"' sh "$U" "$RD"
t "the standard input" "x => y" 0 sh -c '"$1" -o s.mo - < o.po && $2 s.mo | sed -n "/=>/p"' sh "$U" "$RD"
t "an empty file: no messages object" "no" 0 sh -c ': > empty.po; "$1" -o em.mo empty.po && { [ -f em.mo ] && echo yes || echo no; }' sh "$U"
t "a domain with all its messages left out: no file" "one.mo" 0 sh -c 'mkdir od && cd od && printf "domain \"one\"\nmsgid \"a\"\nmsgstr \"A\"\ndomain \"two\"\nmsgid \"b\"\nmsgstr \"\"\n" > x.po && "$1" x.po && ls *.mo' sh "$U"
t "several files: one domain" "a => A${nl}hello => hallo${nl}world => welt" 0 sh -c 'printf "msgid \"a\"\nmsgstr \"A\"\n" > b.po; "$1" -o ab.mo a.po b.po && $2 ab.mo | sed -n "/=>/p"' sh "$U" "$RD"
mkdir -p sub && cp o.po sub/inside.po
t "-D: a directory for the input files" "x => y" 0 sh -c '"$1" -D nowhere -D sub -o in.mo inside.po && $2 in.mo | sed -n "/=>/p"' sh "$U" "$RD"
t "-D: not found there either: status 1" "" 1 "$U" -D sub -o in.mo missing.po
# errors: nothing written, status 1
printf 'msgid "a"\nmsgstr "A"\nmsgid "a"\nmsgstr "B"\n' > dup.po
t "a message defined twice: status 1" "" 1 "$U" -o dup.mo dup.po
t "a message defined twice: nothing written" "no" 0 sh -c '[ -f dup.mo ] && echo yes || echo no'
t "a message defined twice: where" "msgfmt: dup.po:3: a message defined twice (first at dup.po:1)" 0 sh -c '"$1" -o dup.mo dup.po 2>&1; true' sh "$U"
printf 'msgid "a"\nmsgid_plural "as"\nmsgstr[0] "A"\nmsgstr[1] "As"\n' > np.po
t "plural forms without nplurals: status 1" "" 1 "$U" -o np.mo np.po
printf 'msgid ""\nmsgstr "Plural-Forms: nplurals=3; plural=n>1;\\n"\n\nmsgid "a"\nmsgid_plural "as"\nmsgstr[0] "A"\nmsgstr[1] "As"\n' > np2.po
t "not as many forms as nplurals: status 1" "" 1 "$U" -o np2.mo np2.po
printf 'msgid ""\nmsgstr "Plural-Forms: nplurals=2; plural=n >> 1;\\n"\n' > bp.po
t "a plural expression not valid: status 1" "" 1 "$U" -o bp.mo bp.po
printf 'msgid "a"\nmsgid_plural "as"\nmsgstr[1] "As"\n' > bi.po
t "msgstr[1] before msgstr[0]: status 1" "" 1 "$U" -o bi.mo bi.po
printf 'msgid "a"\n' > nm.po
t "msgid without msgstr: status 1" "" 1 "$U" -o nm.mo nm.po
printf 'msgid "a"\nmsgstr "b"\nfrob "c"\n' > bd.po
t "an unknown directive: status 1" "" 1 "$U" -o bd.mo bd.po
t "an unknown directive: the line" "1" 0 sh -c '"$1" -o bd.mo bd.po 2>&1 | grep -c "bd.po:3:"' sh "$U"
printf 'msgid "a\\q"\nmsgstr "b"\n' > be.po
t "an unknown escape: status 1" "" 1 "$U" -o be.mo be.po
printf 'msgid "a\nmsgstr "b"\n' > bq.po
t "no closing quote: status 1" "" 1 "$U" -o bq.mo bq.po
t "a missing file: status 1" "" 1 "$U" -o m.mo missing.po
# -c -v
cat > chk.po <<'X'
msgid "line\n"
msgstr "Zeile"

msgid "\nstart"
msgstr "anfang"

#, c-format
msgid "%d of %s"
msgstr "%d von"

#, c-format
msgid "%d items"
msgstr "%s Dinge"

#, c-format
msgid "%1$s has %2$d"
msgstr "%2$d hat %1$s"

#, no-c-format
msgid "100% sure"
msgstr "100%"

msgid "%s is %d"
msgstr "%d ist %s"

msgid "fine\n"
msgstr "gut\n"
X
t "-c -v: abnormalities found: status 1" "" 1 "$U" -c -v -o chk.mo chk.po
t "-c -v: the diagnostics" "chk.po:1: msgid and msgstr: one ends with a newline, the other not
chk.po:4: msgid and msgstr: one starts with a newline, the other not
chk.po:8: msgid and msgstr have 2 and 1 conversions
chk.po:12: msgid and msgstr have conversions for different types
chk.po:23: msgid and msgstr have conversions for different types" 0 sh -c '"$1" -c -v -o chk.mo chk.po 2>&1 | grep -v translated | sed "s/^msgfmt: //"' sh "$U"
t "-v: the counts" "msgfmt: 8 translated messages, 0 fuzzy, 0 untranslated" 0 sh -c '"$1" -v -o v.mo chk.po 2>&1' sh "$U"
t "without -c: no checks" "" 0 "$U" -o chk.mo chk.po
cat > plchk.po <<'X'
msgid ""
msgstr "Plural-Forms: nplurals=2; plural=n!=1;\n"

#, c-format
msgid "%d file"
msgid_plural "%d files"
msgstr[0] "%d Datei"
msgstr[1] "%s Dateien"
X
t "-c -v: each plural form checked" "plchk.po:5: msgid_plural and msgstr[1] have conversions for different types" 0 sh -c '"$1" -c -v -o pl.mo plchk.po 2>&1 | grep -v translated | sed "s/^msgfmt: //"' sh "$U"
t "no operands: usage" "" 2 "$U"
t "an unknown option: usage" "" 2 "$U" -x a.po
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: msgfmt a.po -v: -v is a file" "" 1 "$U" a.po -v

done_tests msgfmt
