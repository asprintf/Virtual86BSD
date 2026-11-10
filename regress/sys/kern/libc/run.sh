#!/bin/sh
# run.sh - the self-checking tests of V86BSD's C library (built from
# libc/tests/s_*.c), run on this system.  Each gets 600 seconds (emulated, s_malloc needs about 300).
cd "$(dirname "$0")" || exit 2
# the test locales of s_collsym, made from collate.def (copied here with
# the tests) when localedef has the locale sources and charmaps it needs
# (/usr/share/i18n, which bootstrap.sh copies from the host)
loc=$PWD/locales
if [ -f collate.def ] && [ ! -d "$loc" ] && command -v localedef >/dev/null 2>&1; then
    mkdir -p "$loc"
    for c in UTF-8 ISO-8859-1; do
        localedef -f $c -i ./collate.def "$loc/xx_XX.$c" >/dev/null 2>&1 || rm -rf "$loc/xx_XX.$c"
    done
fi
LOCPATH=$loc; export LOCPATH
pass=0 fail=0
for t in s_*; do
    case $t in *.txt|*.def) continue ;; esac
    timeout 600 ./$t > $t.txt 2>&1; st=$?
    if [ $st = 0 ] && ! grep -q FAIL $t.txt; then
        echo "$t: ok"; pass=$((pass + 1))
    else
        echo "$t: FAILED (status $st)"; grep FAIL $t.txt | sed -n '1,8p'; fail=$((fail + 1))
    fi
done
echo "libc tests: $pass passed, $fail failed"
[ $fail = 0 ]
