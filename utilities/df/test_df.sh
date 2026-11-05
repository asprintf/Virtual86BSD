#!/bin/sh
# test_df.sh - tests for df (POSIX.1-2024).  Run: sh test_df.sh
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
U=${U:-$PWD/df}
T=${TMPDIR:-/tmp}/test_df.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
sq() { tr -s ' '; }

# the -P format: the header lines, exactly but for spacing
t "-P header" "Filesystem 512-blocks Used Available Capacity Mounted on" 0 sh -c '"$1" -P / | sed 1q | tr -s " "' sh "$U"
t "-P -k header" "Filesystem 1024-blocks Used Available Capacity Mounted on" 0 sh -c '"$1" -P -k / | sed 1q | tr -s " "' sh "$U"
t "-kP header" "Filesystem 1024-blocks Used Available Capacity Mounted on" 0 sh -c '"$1" -kP / | sed 1q | tr -s " "' sh "$U"
t "-P: one line per operand" "4" 0 sh -c '"$1" -P / / . | wc -l | tr -d " "' sh "$U"
t "-P /: mounted on /" "/" 0 sh -c '"$1" -P / | awk "NR==2 { print \$NF }"' sh "$U"
t "-P: six fields" "6" 0 sh -c '"$1" -P / | awk "NR==2 { print NF }"' sh "$U"
t "-P: numbers are integers" "ok" 0 sh -c '"$1" -P / | awk "NR==2 && \$2 ~ /^[0-9]+\$/ && \$3 ~ /^[0-9]+\$/ && \$4 ~ /^-?[0-9]+\$/ && \$5 ~ /^[0-9]+%\$/ { print \"ok\" }"' sh "$U"
# the capacity: used / (used + available), rounded up
t "-P: the capacity" "ok" 0 sh -c '"$1" -P / | awk "NR==2 { u=\$3; a=\$4; p=\$5; sub(/%/, \"\", p); if (u+a == 0) e = 0; else { e = int(u*100/(u+a)); if (e*(u+a) < u*100) e++ } if (e == p) print \"ok\"; else print e, p }"' sh "$U"
t "-P: used + available <= total (no reserve beyond)" "ok" 0 sh -c '"$1" -P / | awk "NR==2 { if (\$3 + \$4 <= \$2 + 1) print \"ok\" }"' sh "$U"
# -k: 1024-byte units, rounded up; the total does not change between runs
t "-k: half the 512-byte total, rounded up" "ok" 0 sh -c 'a=$("$1" -P / | awk "NR==2 { print \$2 }"); b=$("$1" -Pk / | awk "NR==2 { print \$2 }"); [ "$b" = $(( (a + 1) / 2 )) ] && echo ok' sh "$U"
# a file names the file system it is in
t "a directory: its file system" "/" 0 sh -c '"$1" -P / | awk "NR==2 { print \$NF }"' sh "$U"
t "the file system of ., as with /" "same" 0 sh -c 'cd / && a=$("$1" -P . | awk "NR==2 { print \$1, \$2, \$NF }"); b=$("$1" -P / | awk "NR==2 { print \$1, \$2, \$NF }"); [ "$a" = "$b" ] && echo same' sh "$U"
t "a regular file" "ok" 0 sh -c 'echo x > f && m=$("$1" -P f | awk "NR==2 { print \$NF }") && d=$("$1" -P . | awk "NR==2 { print \$NF }") && [ "$m" = "$d" ] && echo ok' sh "$U"
t "a FIFO" "ok" 0 sh -c 'mkfifo p && m=$("$1" -P p | awk "NR==2 { print \$NF }") && d=$("$1" -P . | awk "NR==2 { print \$NF }") && [ "$m" = "$d" ] && echo ok' sh "$U"
t "the same file system as the reference df" "ok" 0 sh -c 'a=$("$1" -P . | awk "NR==2 { print \$NF }"); b=$(df -P . | awk "NR==2 { print \$NF }"); [ "$a" = "$b" ] && echo ok' sh "$U"
# a block special file that is a mounted file system
dev=$(df -P / 2>/dev/null | awk 'NR==2 { print $1 }')
if [ -b "$dev" ]; then
    t "a block special file: its file system" "/" 0 sh -c '"$1" -P "$2" | awk "NR==2 { print \$NF }"' sh "$U" "$dev"
fi
# no operands: all the file systems, / among them
t "no operands: / is there" "1" 0 sh -c '"$1" -P | awk "\$NF == \"/\"" | wc -l | tr -d " "' sh "$U"
t "no operands: a header and lines" "ok" 0 sh -c 'n=$("$1" -P | wc -l); [ "$n" -ge 2 ] && echo ok' sh "$U"
# the default output: the name, the space available, the free file slots
t "default header" "Filesystem 512-blocks Used Available Capacity Ifree Inodes Mounted on" 0 sh -c '"$1" / | sed 1q | tr -s " "' sh "$U"
t "default -k header" "Filesystem 1024-blocks Used Available Capacity Ifree Inodes Mounted on" 0 sh -c '"$1" -k / | sed 1q | tr -s " "' sh "$U"
t "-t: the total too" "Filesystem 512-blocks Used Available Capacity Ifree Inodes Mounted on" 0 sh -c '"$1" -t / | sed 1q | tr -s " "' sh "$U"
t "-t: eight fields" "8" 0 sh -c '"$1" -t / | awk "NR==2 { print NF }"' sh "$U"
t "default: the same totals as -P" "same" 0 sh -c 'a=$("$1" -P / | awk "NR==2 { print \$1, \$2 }"); b=$("$1" / | awk "NR==2 { print \$1, \$2 }"); [ "$a" = "$b" ] && echo same' sh "$U"
t "the columns line up" "1" 0 sh -c '"$1" -P / /dev | awk "NR == 1 { print index(\$0, \"Capacity\") + 7; next } { print index(\$0, \"%\") }" | sort -u | wc -l | tr -d " "' sh "$U"
# errors
t "a missing file: status 1" "" 1 "$U" -P ./nonexistent
t "a missing file: a message" "1" 0 sh -c '"$1" -P ./nonexistent 2>&1 >/dev/null | grep -c nonexistent' sh "$U"
t "a missing file among others: the rest written" "2" 1 sh -c '"$1" -P / ./nonexistent | wc -l | tr -d " "; exit 1' sh "$U"
t "an unknown option: status 2" "" 2 "$U" -z
t "a usage message" "1" 0 sh -c '"$1" -z 2>&1 | grep -c usage' sh "$U"
t "-- ends the options" "/" 0 sh -c '"$1" -P -- / | awk "NR==2 { print \$NF }"' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: df / -k: -k is a file" "" 1 sh -c '"$1" -P / -k >/dev/null 2>&1' sh "$U"
t "a write error: status 1" "" 1 sh -c '[ -w /dev/full ] || exit 1; "$1" -P / > /dev/full 2>/dev/null' sh "$U"

done_tests df
