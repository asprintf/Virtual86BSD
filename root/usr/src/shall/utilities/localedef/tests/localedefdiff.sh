#!/bin/sh
# localedefdiff.sh LOCALEDEF [LOCALE[:CHARMAP]... | all]: localedef
# compared with the GNU C library's localedef, whose binary format it
# writes: the files of all the categories must be the same byte for
# byte, and the exit status the same (or 1 where glibc's is 0: glibc
# does not count the problems of syntax it reports).  The locale
# sources and charmaps are glibc's (/usr/share/i18n); without a
# CHARMAP, UTF-8.  By default a set of locales, with charmaps of one
# byte and of several; "all": every locale source (UTF-8; slow).
L=$1; shift
[ -x "$L" ] || { echo "usage: localedefdiff.sh LOCALEDEF [LOCALE[:CHARMAP]... | all]" >&2; exit 2; }
L=$(cd "$(dirname "$L")" && pwd)/$(basename "$L")
command -v localedef > /dev/null || { echo "localedefdiff: no localedef to compare with" >&2; exit 2; }
SRC=/usr/share/i18n/locales
[ -d $SRC ] || { echo "localedefdiff: no locale sources in $SRC" >&2; exit 2; }
if [ "$1" = all ]; then
    set --
    for f in $SRC/*; do
        grep -q '^LC_IDENTIFICATION' "$f" && set -- "$@" "$(basename "$f")"
    done
elif [ $# = 0 ]; then
    set -- C POSIX:ANSI_X3.4-1968 en_US en_US:ISO-8859-1 de_DE de_DE:ISO-8859-15 fr_FR fr_FR:ISO-8859-1 \
        es_ES nl_NL sv_SE fi_FI da_DK is_IS pl_PL cs_CZ hu_HU tr_TR tr_TR:ISO-8859-9 el_GR el_GR:ISO-8859-7 \
        ru_RU ru_RU:KOI8-R ru_RU:CP1251 uk_UA he_IL he_IL:CP1255 ar_SA fa_IR hi_IN ta_IN bn_IN th_TH \
        th_TH:TIS-620 vi_VN vi_VN:TCVN5712-1 ja_JP ja_JP:EUC-JP ko_KR ko_KR:EUC-KR zh_CN zh_CN:GB18030 \
        zh_TW zh_TW:BIG5 am_ET ka_GE hy_AM km_KH lo_LA my_MM si_LK sr_RS@latin wo_SN yi_US
fi
T=${TMPDIR:-/tmp}/localedefdiff.$$
mkdir -m 700 "$T" || exit 2
trap 'rm -rf "$T"' EXIT
cats='LC_CTYPE LC_COLLATE LC_NUMERIC LC_TIME LC_MONETARY LC_MESSAGES/SYS_LC_MESSAGES LC_PAPER LC_NAME LC_ADDRESS LC_TELEPHONE LC_MEASUREMENT LC_IDENTIFICATION'
n=0 bad=0 skip=0
for p; do
    name=${p%%:*}
    case $p in *:*) cm=${p#*:};; *) cm=UTF-8;; esac
    [ -f "$SRC/$name" ] || { echo "localedefdiff: $name: no such locale source"; skip=$((skip+1)); continue; }
    rm -rf "$T/g" "$T/u"
    localedef -f "$cm" -i "$name" "$T/g" > /dev/null 2>&1; gs=$?
    "$L" -f "$cm" -i "$name" "$T/u" > /dev/null 2> "$T/err"; us=$?
    if [ ! -f "$T/g/LC_IDENTIFICATION" ]; then
        skip=$((skip+1)); continue    # glibc made nothing: nothing to compare
    fi
    n=$((n+1)) d=
    for c in $cats; do
        cmp -s "$T/g/$c" "$T/u/$c" || d="$d $c"
    done
    if [ "$gs" != "$us" ] && ! { [ "$gs" = 0 ] && [ "$us" = 1 ] && grep -q '^localedef: .*:[0-9]*: ' "$T/err"; }; then
        d="$d (status: glibc $gs, this $us)"
    fi
    if [ -n "$d" ]; then
        bad=$((bad+1)); echo "DIFF $name $cm:$d"
    fi
done
echo "localedefdiff: $n compared, $bad differ, $skip skipped"
[ "$bad" = 0 ]
