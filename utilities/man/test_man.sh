#!/bin/sh
# test_man.sh - tests for man (POSIX.1-2024).  Run: sh test_man.sh
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
U=${U:-$PWD/man}
T=${TMPDIR:-/tmp}/test_man.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T" "$T.tl"' EXIT
# tl: the test language, built here (../testlib/tl.c; ../testlib/tl.md)
TL=$(cd ../testlib && pwd) || exit 2; export TL
cc=${cc:-${CC:-}}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
$cc -o "$T.tl" "$TL/tl.c" || { echo "cannot compile ../testlib/tl.c"; exit 2; }
TLBIN=$T.tl; export TLBIN
tl() { "$TLBIN" "$@"; }
# ptyrun FDS COMMAND...: COMMAND with the descriptors FDS on a terminal (../testlib/ptyrun.tl)
ptyrun() { PTYRUN_INPUT=${PTYRUN_INPUT-} PTYRUN_WAIT=${PTYRUN_WAIT-} "$TLBIN" "$TL/ptyrun.tl" "$@"; }   # (assignments before a function call need not be exported)
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# pages of the tests' own in two directories (MANPATH)
mkdir d1 d2 bin
printf 'NAME\n    alpha - the first\n' > d1/alpha
printf 'NAME\n    beta - the second\n' > d1/beta
printf 'NAME\n    beta - shadowed\n' > d2/beta
printf 'NAME\n    gamma - in d2\n' > d2/gamma
printf 'alpha - the first letter\nbeta - the second letter\n' > d1/whatis
printf 'gamma - the third letter, Greek\n' > d2/whatis
mkdir d1/delta
MANPATH=$T/d1:$T/d2; export MANPATH
unset PAGER

t "a page" "NAME${nl}    alpha - the first" 0 "$U" alpha
t "the first directory that has it" "NAME${nl}    beta - shadowed" 0 env MANPATH="$T/d2:$T/d1" "$U" beta
t "a later directory" "NAME${nl}    gamma - in d2" 0 "$U" gamma
t "several: an empty line between" "NAME${nl}    alpha - the first${nl}${nl}NAME${nl}    gamma - in d2" 0 "$U" alpha gamma
t "no page: status 1" "" 1 "$U" nothing
t "no page: a message" "man: no entry for nothing" 0 sh -c '"$1" nothing 2>&1 >/dev/null; true' sh "$U"
t "no page among others: the others written" "NAME${nl}    alpha - the first" 1 "$U" nothing alpha
t "a directory is not a page" "" 1 "$U" delta
t "a name with a slash is not a page" "" 1 "$U" ../d1/alpha
t "whatis is not a page" "" 1 "$U" whatis
t "empty MANPATH components left out" "NAME${nl}    gamma - in d2" 0 env MANPATH="::$T/d2:" "$U" gamma
# -k
t "-k: the matching lines" "beta - the second letter" 0 "$U" -k beta
t "-k: any of the names" "alpha - the first letter${nl}gamma - the third letter, Greek" 0 "$U" -k alpha greek
t "-k: without regard to case" "gamma - the third letter, Greek" 0 "$U" -k GREEK
t "-k: extended regular expressions" "alpha - the first letter${nl}beta - the second letter" 0 "$U" -k '(first|second) letter$'
t "-k: matched anywhere in the line" "alpha - the first letter${nl}beta - the second letter${nl}gamma - the third letter, Greek" 0 "$U" -k letter
t "-k: none found: status 1" "" 1 "$U" -k zeta
t "-k: none found: a message" "man: nothing appropriate" 0 sh -c '"$1" -k zeta 2>&1; true' sh "$U"
t "-k: an invalid expression: status 1" "" 1 "$U" -k 'a('
t "-k: a directory without whatis is skipped" "gamma - the third letter, Greek" 0 env MANPATH="$T/bin:$T/d2" "$U" -k greek
t "no names: usage" "" 2 "$U"
t "-k without names: usage" "" 2 "$U" -k
t "an unknown option: usage" "" 2 "$U" -x alpha
# a terminal: the pager
cat > bin/more <<'X'
#!/bin/sh
sed 's/^/more: /'
X
chmod +x bin/more
t "a terminal: through PAGER" "P:NAME${nl}P:    alpha - the first" 0 sh -c 'PAGER="sed s/^/P:/" "$TLBIN" "$TL/ptyrun.tl" 1 "$1" alpha' sh "$U"
t "a terminal: PAGER unset: more" "more: NAME${nl}more: alpha - the first" 0 sh -c 'PATH=$2:$PATH "$TLBIN" "$TL/ptyrun.tl" 1 "$1" alpha | sed "s/    //"' sh "$U" "$T/bin"
t "a terminal: PAGER empty: more" "more: NAME${nl}more: alpha - the first" 0 sh -c 'PAGER= PATH=$2:$PATH "$TLBIN" "$TL/ptyrun.tl" 1 "$1" alpha | sed "s/    //"' sh "$U" "$T/bin"
t "a terminal: -k through PAGER" "P:beta - the second letter" 0 sh -c 'PAGER="sed s/^/P:/" "$TLBIN" "$TL/ptyrun.tl" 1 "$1" -k beta' sh "$U"
t "a pager that fails: status 1" "" 1 sh -c 'PAGER="cat >/dev/null; exit 3" "$TLBIN" "$TL/ptyrun.tl" 1 "$1" alpha' sh "$U"
t "not a terminal: no pager" "NAME${nl}    alpha - the first" 0 env PAGER="sed s/^/P:/" "$U" alpha
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: man alpha -k: -k is a name" "NAME${nl}    alpha - the first" 1 "$U" alpha -k

U8=${UTF8_LOCALE:-$(locale -a 2>/dev/null | grep -i -E '^c\.utf-?8$' | sed 1q)}
[ -n "$U8" ] || U8=$(locale -a 2>/dev/null | grep -i -E '\.utf-?8$' | sed 1q)
# the pages made from the standard (mkpages.tl), when the standard is here
SPEC=${SPEC:-}
for s in "$OLDPWD/../susv5-html" "$OLDPWD/../../susv5-html" "$OLDPWD/../../spec/susv5-html" "$OLDPWD/../../../spec/susv5-html"; do
    [ -z "$SPEC" ] && [ -d "$s/utilities" ] && SPEC=$s
done
if [ -n "$SPEC" ] && [ -d "$SPEC/utilities" ]; then
    "$TLBIN" "$OLDPWD/mkpages.tl" "$SPEC" pages > /dev/null || echo "FAIL mkpages.tl"
    MANPATH=$T/pages; export MANPATH
    for u in $(sed 's/ - .*//' pages/whatis); do
        "$U" "$u" > page.out 2>/dev/null || { echo "FAIL man $u"; fail=$((fail + 1)); continue; }
        for s in NAME SYNOPSIS DESCRIPTION OPTIONS OPERANDS 'ENVIRONMENT VARIABLES' 'EXIT STATUS'; do
            grep -q "^$s\$" page.out || { echo "FAIL man $u: no $s"; fail=$((fail + 1)); continue 2; }
        done
        pass=$((pass + 1))
    done
    t "the standard's pages: every utility has one" "157" 0 sh -c 'sed "s/ - .*//" pages/whatis | wc -l | tr -d " "'
    t "the standard's pages: man's own" "    [UP] man [-k] name..." 0 sh -c '"$1" man | sed -n "/^SYNOPSIS/{n;p;}"' sh "$U"
    t "the standard's pages: -k as in the standard's example" "ps - report process status" 0 "$U" -k 'process status'
    t "the standard's pages: [ is test" "    test — evaluate expression" 0 sh -c '"$1" "[" | sed -n 2p' sh "$U"
    t "the standard's pages: the informative sections after a line" "ok" 0 sh -c '"$1" man | grep -q "^The following sections are informative.\$" && "$1" man | grep -q "^RATIONALE\$" && ! "$1" man | grep -q "^CHANGE HISTORY" && echo ok' sh "$U"
    t "the standard's pages: filled to 72 columns" "0" 0 sh -c 'cat pages/man pages/cat pages/ls pages/mesg | LC_ALL=$1 awk "length > 72" | wc -l | tr -d " "' sh "${U8:-C}"
fi

done_tests man
