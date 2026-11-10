#!/bin/sh
# localediff.sh LOCALE: locale compared with the GNU C library's locale.
# Locales are made with localedef in a directory of their own (LOCPATH);
# in each, every keyword of the standard (but ab_alt_mon and alt_mon,
# which GNU locale does not know) must give the same with -k and without,
# and so must the categories with -k, and locale without operands for
# some settings of the variables (GNU's lines of its own categories left
# out), and locale -a.  GNU locale writes the entries of era and
# alt_digits as "a";"b", this one as "a;b": GNU's are joined so for the
# comparison.
U=$1
case $U in /*) ;; *) U=$PWD/$U ;; esac
command -v locale >/dev/null && command -v localedef >/dev/null || { echo "localediff: needs locale and localedef"; exit 2; }
G=$(command -v locale)
T=${TMPDIR:-/tmp}/localediff.$$
mkdir "$T" || exit 2
trap 'rm -rf "$T"' EXIT
mkdir "$T/loc"
LOCPATH=$T/loc; export LOCPATH
locs="C POSIX"
for l in de_DE:UTF-8 fr_FR:ISO-8859-1 ja_JP:UTF-8 en_US:UTF-8 ar_SA:UTF-8 ru_RU:KOI8-R th_TH:UTF-8 fa_IR:UTF-8; do
    i=${l%:*} f=${l#*:}
    n=$i.$f
    [ "$f" = ISO-8859-1 ] && n=$i
    localedef -i "$i" -f "$f" "$T/loc/$n" >/dev/null 2>&1 && [ -d "$T/loc/$n" ] && locs="$locs $n"
done
kws="abday day abmon mon d_t_fmt d_fmt t_fmt am_pm t_fmt_ampm era era_d_fmt era_t_fmt era_d_t_fmt alt_digits
decimal_point thousands_sep grouping int_curr_symbol currency_symbol mon_decimal_point mon_thousands_sep
mon_grouping positive_sign negative_sign int_frac_digits frac_digits p_cs_precedes p_sep_by_space
n_cs_precedes n_sep_by_space p_sign_posn n_sign_posn int_p_cs_precedes int_p_sep_by_space int_n_cs_precedes
int_n_sep_by_space int_p_sign_posn int_n_sign_posn yesexpr noexpr charmap"
fail=0 n=0
norm() { sed '/^era=/s/";"/;/g; /^alt_digits=/s/";"/;/g; s/^era=$/era=""/; s/^alt_digits=$/alt_digits=""/'; }
for l in $locs; do
    for k in $kws; do
        for o in -k ""; do
            n=$((n + 1))
            a=$(LC_ALL=$l "$U" $o "$k" 2>&1)
            b=$(LC_ALL=$l "$G" $o "$k" 2>&1 | norm)
            case $k in era|alt_digits) [ -z "$o" ] && b=$(LC_ALL=$l "$G" "$k" 2>&1 | sed 's/";"/;/g; s/^"\(.*\)"$/\1/') ;; esac
            if [ "$a" != "$b" ]; then
                fail=$((fail + 1)); echo "FAIL LC_ALL=$l locale $o $k"; echo "  ours: $a"; echo "  gnu:  $b"
            fi
        done
    done
done
# no operands
for e in "LANG=de_DE.UTF-8" "LANG=x LC_COLLATE=y" "LC_ALL=C LANG=x LC_TIME=z" "LANG=" "LC_MESSAGES=fr_FR"; do
    n=$((n + 1))
    a=$(env -i PATH="$PATH" LOCPATH="$LOCPATH" $e "$U" 2>/dev/null)
    b=$(env -i PATH="$PATH" LOCPATH="$LOCPATH" $e "$G" 2>/dev/null | grep -E '^(LANG|LC_CTYPE|LC_COLLATE|LC_TIME|LC_NUMERIC|LC_MONETARY|LC_MESSAGES|LC_ALL)=')
    a=$(printf '%s\n' "$a" | LC_ALL=C sort) b=$(printf '%s\n' "$b" | LC_ALL=C sort)
    if [ "$a" != "$b" ]; then
        fail=$((fail + 1)); echo "FAIL $e locale"; echo "  ours: $a"; echo "  gnu:  $b"
    fi
done
# -a (without LOCPATH, which GNU locale -a does not look in)
n=$((n + 1))
if [ "$(unset LOCPATH; "$U" -a)" != "$(unset LOCPATH; "$G" -a | LC_ALL=C sort -u)" ]; then
    fail=$((fail + 1)); echo "FAIL locale -a"
fi
echo "localediff: $n compared, $fail failed"
[ $fail = 0 ]
