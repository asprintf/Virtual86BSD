#!/bin/sh
# The legacy interfaces (share/doc/legacy.md) are declared under the
# feature-test macros of their editions only.  Windows: X = _XOPEN_SOURCE
# 500-700, X6 = 500-600, X67 = 600-700, X5 = 500, P:lo:hi = _POSIX_C_SOURCE in [lo, hi),
# A+B = either.  No macro, _GNU_SOURCE, _DEFAULT_SOURCE: all; strict ISO C:
# none.
# usage: legacy.sh CC   (CC is a libc-cc invocation, e.g. "tools/libc-cc gcc"; LEGACY_CC=c17
# uses that compiler as it is, inside V86BSD)
cc=${LEGACY_CC:-"$(dirname "$0")/../../../lib/libc/tools/libc-cc ${1:-gcc}"}
tmp=${TMPDIR:-/tmp}/legacy.$$; mkdir -m 700 "$tmp" || exit 1; trap 'rm -rf "$tmp"' EXIT
case $cc in *c17*) werr= ;; *) werr=-Werror=implicit-function-declaration ;; esac

# name|window|program
items='gettimeofday|X|#include <sys/time.h>
int main(void) { struct timeval t; return gettimeofday(&t, 0); }
itimer|X|#include <sys/time.h>
int main(void) { struct itimerval v; return getitimer(ITIMER_REAL, &v) + setitimer(ITIMER_PROF, &v, 0) + ITIMER_VIRTUAL; }
usleep|X6|#include <unistd.h>
int main(void) { return usleep(1); }
setpgrp|X|#include <unistd.h>
int main(void) { return (int)setpgrp(); }
sigfuncs|X|#include <signal.h>
int main(void) { sighold(SIGINT); sigrelse(SIGINT); sigignore(SIGUSR1); sigpause(SIGINT); siginterrupt(SIGINT, 0); return sigset(SIGINT, SIG_HOLD) == SIG_ERR; }
sigconst|X|#include <signal.h>
void (*h)(int) = SIG_HOLD; int y = POLL_IN + POLL_OUT + POLL_MSG + POLL_ERR + POLL_PRI + POLL_HUP;
int main(void) { return h != 0 || y == 0; }
strings|X6|#include <strings.h>
int main(void) { char a[2] = "a"; bcopy(a, a, 1); bzero(a, 0); return bcmp(a, a, 1) + (index(a, 0) == rindex(a, 0)); }
ctype|X|#include <ctype.h>
int main(void) { return isascii(1) + toascii(1) + _toupper(97) + _tolower(65); }
cvt|X6|#include <stdlib.h>
int main(void) { int d, s; char b[32], t[] = "/tmp/xXXXXXX"; return !ecvt(1, 2, &d, &s) + !fcvt(1, 2, &d, &s) + !gcvt(1, 2, b) + !mktemp(t); }
rand_r|P:199506:202405|#include <stdlib.h>
int main(void) { unsigned s = 1; return rand_r(&s); }
stdlib5|X5|#include <stdlib.h>
int main(void) { return ttyslot() + !valloc(1); }
tempnam|X|#include <stdio.h>
int main(void) { return !tempnam(P_tmpdir, "x"); }
stdio5|X5|#include <stdio.h>
int main(void) { char b[L_cuserid]; char *const v[] = { "x", 0 }; return !cuserid(b) + getw(stdin) + putw(1, stdout) + getopt(1, v, "") + (optarg != 0) + opterr + optind + optopt; }
unistd6|X6|#include <unistd.h>
int main(void) { char b[4096]; return !getwd(b) + (int)ualarm(0, 0) + (int)vfork() + _XOPEN_LEGACY + _XBS5_LP64_OFF64 + _CS_XBS5_LP64_OFF64_CFLAGS + _SC_XBS5_LP64_OFF64 + _SC_XOPEN_LEGACY; }
unistd5|X5|#include <unistd.h>
int main(void) { char b[64]; return brk(0) + (sbrk(0) == 0) + chroot("/") + !ctermid(b) + !cuserid(b) + getdtablesize() + getpagesize() + !getpass("") + _XOPEN_XCU_VERSION + _XOPEN_XPG4 + _SC_PASS_MAX + _SC_XOPEN_XCU_VERSION; }
posix2c|P:1:200112|#include <unistd.h>
long v = _POSIX2_C_VERSION + _SC_2_C_VERSION;
int main(void) { return v == 0; }
fortdev|P:1:202405|#include <unistd.h>
int main(void) { return _POSIX2_FORT_DEV + _SC_2_FORT_DEV; }
v6env|P:200112:202405|#include <unistd.h>
int main(void) { return _POSIX_V6_LP64_OFF64 + _POSIX_V6_ILP32_OFF32 + _SC_V6_LP64_OFF64 + _SC_V6_LPBIG_OFFBIG + _CS_POSIX_V6_LP64_OFF64_CFLAGS + _CS_POSIX_V6_ILP32_OFFBIG_LIBS + _CS_POSIX_V6_WIDTH_RESTRICTED_ENVS; }
v6envstr|P:200809:202405|#include <unistd.h>
int main(void) { return _CS_V6_ENV; }
pbs|P:200112:202405|#include <unistd.h>
int main(void) { return _POSIX2_PBS + _POSIX2_PBS_ACCOUNTING + _POSIX2_PBS_CHECKPOINT + _POSIX2_PBS_LOCATE + _POSIX2_PBS_MESSAGE + _POSIX2_PBS_TRACK + _SC_2_PBS + _SC_2_PBS_TRACK; }
trace|P:200112:202405|#include <unistd.h>
int main(void) { return _POSIX_TRACE + _POSIX_TRACE_EVENT_FILTER + _POSIX_TRACE_INHERIT + _POSIX_TRACE_LOG + _SC_TRACE + _SC_TRACE_EVENT_NAME_MAX + _SC_TRACE_USER_EVENT_MAX; }
tracelim|P:200112:202405|#include <limits.h>
int main(void) { return _POSIX_TRACE_EVENT_NAME_MAX + _POSIX_TRACE_NAME_MAX + _POSIX_TRACE_SYS_MAX + _POSIX_TRACE_USER_EVENT_MAX; }
xstreams|X67|#include <unistd.h>
int main(void) { return _XOPEN_STREAMS + _SC_XOPEN_STREAMS; }
limits|X|#include <limits.h>
double v = DBL_DIG + DBL_MAX + FLT_DIG + FLT_MAX;
int main(void) { return v == 0; }
nlnmax|X6|#include <limits.h>
int main(void) { return NL_NMAX; }
limits5|X5|#include <limits.h>
long v = PASS_MAX + TMP_MAX;
int main(void) { return v == 0; }
maxfloat|X|#include <math.h>
float v = MAXFLOAT;
int main(void) { return v == 0; }
scalb|X6|#include <math.h>
int main(void) { return scalb(1, 1) == 0; }
gamma|X5|#include <math.h>
int main(void) { return gamma(1) != 0; }
time_r|P:199506:202405|#include <time.h>
int main(void) { char b[26]; time_t t = 0; struct tm tm = { 0 }; return !asctime_r(&tm, b) + !ctime_r(&t, b); }
clk_tck|P:1:200112|#include <time.h>
long v = CLK_TCK;
int main(void) { return v == 0; }
nosys|P:1:200809|#include <fnmatch.h>
#include <glob.h>
#include <regex.h>
#include <wordexp.h>
int main(void) { return FNM_NOSYS + GLOB_NOSYS + REG_ENOSYS + WRDE_NOSYS; }
yesstr|X5|#include <langinfo.h>
int main(void) { return YESSTR + NOSTR; }
termios|X5|#include <termios.h>
int main(void) { return IUCLC + OLCUC + XCASE; }
useconds|X6|#include <sys/types.h>
useconds_t v;
int main(void) { return (int)v; }
wchar|X|#include <wchar.h>
int main(void) { return iswalpha(97) + iswctype(97, wctype("alpha")) + (int)towupper(97); }
wcswcs|X6|#include <wchar.h>
int main(void) { return wcswcs(L"a", L"a") == 0; }
setjmp|X|#include <setjmp.h>
jmp_buf e;
int main(void) { if (_setjmp(e)) return 0; _longjmp(e, 1); }
bsd_signal|X6|#include <signal.h>
int main(void) { return bsd_signal(SIGINT, SIG_DFL) == SIG_ERR; }
sigstack|X5|#include <signal.h>
int main(void) { struct sigstack s; return sigstack(0, &s); }
timeb|X6|#include <sys/timeb.h>
int main(void) { struct timeb t; return ftime(&t); }
wait3|X5|#include <sys/wait.h>
int main(void) { int s; return (int)wait3(&s, 0, 0); }
ulimit|X|#include <ulimit.h>
int main(void) { return (int)ulimit(UL_GETFSIZE) + UL_SETFSIZE; }
utime|P:1:202405|#include <utime.h>
int main(void) { struct utimbuf u = { 0, 0 }; return utime("/", &u); }
stackaddr|P:199506:200809|#include <pthread.h>
int main(void) { pthread_attr_t a; void *p; return pthread_attr_setstackaddr(&a, 0) + pthread_attr_getstackaddr(&a, &p); }
concurrency|X|#include <pthread.h>
int main(void) { return pthread_setconcurrency(0) + pthread_getconcurrency(); }
hostby|P:200112:200809+X6|#include <netdb.h>
int main(void) { return !gethostbyname("x") + !gethostbyaddr("", 0, 0) + h_errno + HOST_NOT_FOUND + NO_DATA + NO_RECOVERY + TRY_AGAIN; }
inetclass|X5|#include <arpa/inet.h>
int main(void) { struct in_addr a = inet_makeaddr(1, 1); return (int)(inet_lnaof(a) + inet_netof(a) + inet_network("1")); }
regexp|X5|#define INIT char *sp = instring;
#define GETC() (*sp++)
#define PEEKC() (*sp)
#define UNGETC(c) ((void) (c), (void) --sp)
#define RETURN(c) return (c);
#define ERROR(c) return (void) (c), (char *) 0;
#include <regexp.h>
char e[64], p[] = "a";
int main(void) { return !compile(p, e, e + 64, 0) + step("a", e) + advance("a", e) + (loc1 == loc2) + (locs != 0); }
re_comp|X5|#include <re_comp.h>
int main(void) { return re_comp("a") != 0 || re_exec("a") != 1; }
regcmp|X5|#include <libgen.h>
int main(void) { char *r = regcmp("a", (char *)0); return !regex(r, "a") + (__loc1 == 0); }
ucontext|X6|#include <ucontext.h>
ucontext_t u;
static void f(void) { }
int main(void) { getcontext(&u); makecontext(&u, f, 0); return setcontext(&u) + swapcontext(&u, &u); }'

# macro set (";" for a space)|levels: all, or _POSIX_C_SOURCE _XOPEN_SOURCE
sets='|all
-D_GNU_SOURCE|all
-D_DEFAULT_SOURCE|all
-D_XOPEN_SOURCE=500|199506 500
-D_XOPEN_SOURCE=600|200112 600
-D_XOPEN_SOURCE=700|200809 700
-D_POSIX_C_SOURCE=200809L;-D_XOPEN_SOURCE=700|200809 700
-D_XOPEN_SOURCE=800|202405 800
-D_POSIX_C_SOURCE=202405L|202405 0
-D_POSIX_C_SOURCE=200809L|200809 0
-D_POSIX_C_SOURCE=200112L|200112 0
-D_POSIX_C_SOURCE=199506L|199506 0
-D_POSIX_C_SOURCE=1|1 0'

visible() {
	w=$1 p=$2 x=$3
	for part in $(printf '%s' "$w" | tr '+' ' '); do
		case $part in
		X)  [ "$x" -ge 500 ] && [ "$x" -lt 800 ] && return 0 ;;
		X6) [ "$x" -ge 500 ] && [ "$x" -lt 700 ] && return 0 ;;
		X67) [ "$x" -ge 600 ] && [ "$x" -lt 800 ] && return 0 ;;
		X5) [ "$x" -eq 500 ] && return 0 ;;
		P:*) lo=${part#P:}; hi=${lo#*:}; lo=${lo%%:*}
		    [ "$p" -ge "$lo" ] && [ "$p" -lt "$hi" ] && return 0 ;;
		esac
	done
	return 1
}

names=$(printf '%s\n' "$items" | awk -F'|' 'NF >= 3 && $1 ~ /^[a-z_0-9]+$/ { print $1 }')
prog_of() {	# the program of item $1
	printf '%s\n' "$items" | awk -F'|' -v n="$1" '
		NF >= 3 && $1 ~ /^[a-z_0-9]+$/ { on = $1 == n; if (on) { sub(/^[^|]*\|[^|]*\|/, ""); print }; next }
		on { print }'
}
window_of() {
	printf '%s\n' "$items" | awk -F'|' -v n="$1" 'NF >= 3 && $1 == n { print $2; exit }'
}

fail=0 n=0
while IFS= read -r s; do
	macros=$(printf '%s' "${s%%|*}" | tr ';' ' ')
	levels=${s#*|}
	for name in $names; do
		prog_of "$name" > "$tmp/t.c"
		if [ "$levels" = all ] || visible "$(window_of "$name")" $levels; then want=yes; else want=no; fi
		n=$((n + 1))
		if $cc $macros $werr -c -o "$tmp/t.o" "$tmp/t.c" 2> "$tmp/err" < /dev/null; then got=yes; else got=no; fi
		if [ $want != $got ]; then
			echo "FAIL: $name with [${macros:-no macro}]: declared=$got, the standard says $want"
			sed 's/^/    /' "$tmp/err" | head -3
			fail=$((fail + 1))
		fi
	done
done <<EOF2
$sets
EOF2
case $cc in
*c17*) ;;
*)	# a strict ISO C compilation sees none of them
	for name in $names; do
		prog_of "$name" > "$tmp/t.c"; n=$((n + 1))
		if $cc -std=c17 $werr -c -o "$tmp/t.o" "$tmp/t.c" 2>/dev/null; then
			echo "FAIL: $name is declared in strict ISO C mode"; fail=$((fail + 1))
		fi
	done ;;
esac
[ $fail = 0 ] && echo "legacy visibility ok ($n checks)"
[ $fail = 0 ]
