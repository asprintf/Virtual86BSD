#!/bin/sh
# test_getconf.sh - tests for getconf (POSIX.1-2024).  Run: sh test_getconf.sh
cd "$(dirname "$0")" || exit 2
PROG_DIR=${PROG_DIR:-$PWD/../../../usr.bin/getconf}; export PROG_DIR
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
U=${U:-$PROG_DIR/getconf}
T=${TMPDIR:-/tmp}/test_getconf.$$
mkdir -m 700 "$T" || exit 2
trap 'chmod -R u+rwx "$T" 2>/dev/null; rm -rf "$T"' EXIT
cd "$T" || exit 2
export LC_ALL=C
root=false; [ "$(id -u)" = 0 ] && root=true
nl='
'
# the reference: the C library's sysconf(), confstr() and pathconf(), for
# each variable of getconf's tables that it has a name for (a program made
# here: ref.sh writes it, compiles it, and compares getconf with it)
cat > ref.sh <<'X'
u=$1 src=$2
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
sed -n "/^static const struct var sysvars/,/^};/s/^    { \"\([A-Z0-9_]*\)\".*/sys \1/p
/^static const struct var strvars/,/^};/s/^    { \"\([A-Z0-9_]*\)\".*/str \1/p
/^static const struct var pathvars/,/^};/s/^    { \"\([A-Z0-9_]*\)\".*/path \1/p" "$src" | sort -u | awk '
BEGIN {
    print "#define _DEFAULT_SOURCE"
    print "#include <errno.h>"
    print "#include <stdio.h>"
    print "#include <stdlib.h>"
    print "#include <unistd.h>"
    print "static void out(const char *n, const char *p, long v)"
    print "{"
    print "    printf(\"%s%s%s\\t\", n, p ? \" \" : \"\", p ? p : \"\");"
    print "    if (v == -1) puts(\"undefined\"); else printf(\"%ld\\n\", v);"
    print "}"
    print "int main(void)"
    print "{"
    print "    long v;"
    print "    size_t n;"
    print "    char *b;"
    print "    (void)v; (void)n; (void)b;"
}
$1 == "sys" {
    k = $2; sub(/^_+/, "", k)
    if (k ~ /^POSIX2_/) k = "2_" substr(k, 8)
    else if (k ~ /^POSIX_/) k = substr(k, 7)
    else if (k ~ /^PTHREAD_/) k = "THREAD_" substr(k, 9)
    printf "#ifdef _SC_%s\n    errno = 0; v = sysconf(_SC_%s);\n    if (!(v == -1 && errno)) out(\"%s\", 0, v);\n#endif\n", k, k, $2
}
$1 == "str" {
    printf "#ifdef _CS_%s\n    errno = 0; n = confstr(_CS_%s, 0, 0);\n", $2, $2
    printf "    if (n == 0 && !errno) printf(\"%s\\tundefined\\n\");\n", $2
    printf "    else if (n) { b = malloc(n); confstr(_CS_%s, b, n);\n", $2
    print  "        for (char *q = b; *q; q++) if (*q == 10) *q = 124;  /* lists of names: | for newline */"
    printf "        printf(\"%s\\t%%s\\n\", b); free(b); }\n#endif\n", $2
}
$1 == "path" {
    k = $2
    if (k ~ /^_POSIX_/) k = substr(k, 8)
    else { sub(/POSIX_/, "", k); sub(/POSIX2_/, "2_", k) }
    printf "#ifdef _PC_%s\n", k
    printf "    errno = 0; v = pathconf(\"/\", _PC_%s);\n    if (!(v == -1 && errno)) out(\"%s\", \"/\", v);\n", k, $2
    printf "    errno = 0; v = pathconf(\".\", _PC_%s);\n    if (!(v == -1 && errno)) out(\"%s\", \".\", v);\n#endif\n", k, $2
}
END { print "    return 0;"; print "}" }' > ref.c
$cc -o ref ref.c || { echo "cannot compile ref.c"; exit 1; }
./ref > want || exit 1
checked=0 bad=0
while IFS='	' read -r args value; do
    checked=$((checked + 1))
    got=$("$u" $args 2>/dev/null)
    st=$?
    got=$(printf '%s' "$got" | tr '\n' '|')   # (as ref writes lists of names)
    if [ $st != 0 ] || [ "$got" != "$value" ]; then
        bad=$((bad + 1))
        echo "differs: $args: [$got], want [$value]"
    fi
done < want
if [ "$checked" -ge 60 ] && [ "$bad" = 0 ]; then echo ok; else echo "checked $checked, $bad differ"; fi
X
src=${U%/*}/getconf.c
t "every variable the C library has a name for, as it finds it" "ok" 0 sh ref.sh "$U" "$src"
# the system's getconf, for a few
t "ARG_MAX, as the system's getconf" "$(getconf ARG_MAX)" 0 "$U" ARG_MAX
t "PATH, as the system's getconf" "$(getconf PATH)" 0 "$U" PATH
t "NAME_MAX /, as the system's getconf" "$(getconf NAME_MAX /)" 0 "$U" NAME_MAX /
t "_POSIX_VERSION, as the system's getconf" "$(getconf _POSIX_VERSION)" 0 "$U" _POSIX_VERSION
t "LINE_MAX: at least _POSIX2_LINE_MAX" "yes" 0 sh -c '[ "$("$1" LINE_MAX)" -ge 2048 ] && echo yes' sh "$U"
t "PATH finds the standard utilities" "yes" 0 sh -c 'p=$("$1" PATH); PATH=$p command -v sh >/dev/null && echo yes' sh "$U"
# the values the standard fixes
t "_POSIX_ARG_MAX" "4096" 0 "$U" _POSIX_ARG_MAX
t "_POSIX_NAME_MAX" "14" 0 "$U" _POSIX_NAME_MAX
t "_POSIX_PATH_MAX" "256" 0 "$U" _POSIX_PATH_MAX
t "_POSIX2_LINE_MAX" "2048" 0 "$U" _POSIX2_LINE_MAX
t "_POSIX_CLOCKRES_MIN" "20000000" 0 "$U" _POSIX_CLOCKRES_MIN
t "_XOPEN_IOV_MAX" "16" 0 "$U" _XOPEN_IOV_MAX
t "_POSIX_TZNAME_MAX" "6" 0 "$U" _POSIX_TZNAME_MAX
# the earlier names
t "POSIX2_LINE_MAX is _POSIX2_LINE_MAX" "2048" 0 "$U" POSIX2_LINE_MAX
t "POSIX2_BC_BASE_MAX is _POSIX2_BC_BASE_MAX" "99" 0 "$U" POSIX2_BC_BASE_MAX
t "POSIX2_VERSION is _POSIX2_VERSION" "$("$U" _POSIX2_VERSION)" 0 "$U" POSIX2_VERSION
t "POSIX2_C_BIND is _POSIX2_C_BIND" "$("$U" _POSIX2_C_BIND)" 0 "$U" POSIX2_C_BIND
t "NPROCESSORS_ONLN: a number" "yes" 0 sh -c '[ "$("$1" NPROCESSORS_ONLN)" -ge 1 ] && echo yes' sh "$U"
t "NPROCESSORS_CONF: a number" "yes" 0 sh -c '[ "$("$1" NPROCESSORS_CONF)" -ge 1 ] && echo yes' sh "$U"
# the V8 environments: one of them is there
t "one of the V8 environments is supported" "yes" 0 sh -c 'for e in ILP32_OFF32 ILP32_OFFBIG LP64_OFF64 LPBIG_OFFBIG; do v=$("$1" _POSIX_V8_$e); case $v in -1|undefined) ;; *) echo yes; exit;; esac; done' sh "$U"
t "-v with a supported environment" "$("$U" PAGESIZE)" 0 sh -c 'for e in ILP32_OFF32 ILP32_OFFBIG LP64_OFF64 LPBIG_OFFBIG; do v=$("$1" _POSIX_V8_$e); case $v in -1|undefined) ;; *) exec "$1" -v POSIX_V8_$e PAGESIZE;; esac; done' sh "$U"
t "-v with a specification that is none" "" 1 "$U" -v POSIX_V8_NONE PAGESIZE
t "-v with an empty specification" "" 1 "$U" -v "" PAGESIZE
# the names of SUSv2 to SUSv4
t "_XBS5_LP64_OFF64" "1" 0 "$U" _XBS5_LP64_OFF64
t "_XBS5_ILP32_OFF32: not supported" "undefined" 0 "$U" _XBS5_ILP32_OFF32
t "XBS5_LP64_OFF64_CFLAGS: nothing to add" "" 0 "$U" XBS5_LP64_OFF64_CFLAGS
t "-v XBS5_LP64_OFF64" "$("$U" PAGESIZE)" 0 "$U" -v XBS5_LP64_OFF64 PAGESIZE
t "-v XBS5_ILP32_OFF32: not this environment" "" 1 "$U" -v XBS5_ILP32_OFF32 PAGESIZE
t "_POSIX_V6_LP64_OFF64" "1" 0 "$U" _POSIX_V6_LP64_OFF64
t "_POSIX_V6_ILP32_OFFBIG: not supported" "undefined" 0 "$U" _POSIX_V6_ILP32_OFFBIG
t "POSIX_V6_WIDTH_RESTRICTED_ENVS" "POSIX_V6_LP64_OFF64${nl}POSIX_V6_LPBIG_OFFBIG" 0 "$U" POSIX_V6_WIDTH_RESTRICTED_ENVS
t "V6_ENV" "" 0 "$U" V6_ENV
t "-v POSIX_V6_LPBIG_OFFBIG" "$("$U" PAGESIZE)" 0 "$U" -v POSIX_V6_LPBIG_OFFBIG PAGESIZE
t "_POSIX2_C_VERSION" "199209" 0 "$U" _POSIX2_C_VERSION
t "_XOPEN_XCU_VERSION" "5" 0 "$U" _XOPEN_XCU_VERSION
t "_XOPEN_LEGACY" "1" 0 "$U" _XOPEN_LEGACY
t "PASS_MAX" "128" 0 "$U" PASS_MAX
t "POSIX2_FORT_DEV is _POSIX2_FORT_DEV" "undefined" 0 "$U" POSIX2_FORT_DEV
t "_POSIX2_PBS: not supported" "undefined" 0 "$U" _POSIX2_PBS
t "_POSIX_TRACE: not supported" "undefined" 0 "$U" _POSIX_TRACE
t "_POSIX_TRACE_SYS_MAX: no trace" "undefined" 0 "$U" _POSIX_TRACE_SYS_MAX
t "_XOPEN_STREAMS: not supported" "undefined" 0 "$U" _XOPEN_STREAMS
# errors: nothing on the standard output
t "an unknown variable" "" 1 "$U" NO_SUCH_VARIABLE
t "an unknown variable: a diagnostic" "" 0 sh -c '"$1" NO_SUCH_VARIABLE 2>&1 | grep -q .' sh "$U"
t "a path variable without a pathname" "" 1 "$U" NAME_MAX
t "a system variable with a pathname" "" 1 "$U" ARG_MAX /
t "a missing pathname" "" 1 "$U" NAME_MAX ./no_such_file
t "_SC_ARG_MAX is not a name" "" 1 "$U" _SC_ARG_MAX
t "a name in lower case" "" 1 "$U" arg_max
t "no operand" "" 2 "$U"
t "three operands" "" 2 "$U" NAME_MAX / /
t "-v without a specification" "" 2 "$U" -v
t "an unknown option" "" 2 "$U" -x ARG_MAX
t "--" "$("$U" ARG_MAX)" 0 "$U" -- ARG_MAX
t "a path variable for a file" "$("$U" NAME_MAX .)" 0 sh -c ': > f; "$1" NAME_MAX f' sh "$U"
t "a path variable for a FIFO: PIPE_BUF" "yes" 0 sh -c 'mkfifo p && [ "$("$1" PIPE_BUF p)" -ge 512 ] && echo yes' sh "$U"
# getopt() must not take an option after an operand for one (no permuting)
t "options after operands: getconf ARG_MAX -v: -v is a pathname" "" 1 "$U" ARG_MAX -v
[ -c /dev/full ] && t "a write error" "" 1 sh -c '"$1" ARG_MAX > /dev/full' sh "$U"

done_tests getconf
