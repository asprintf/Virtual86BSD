#!/bin/sh
# The legacy XSI interfaces (SUSv2-SUSv4; POSIX.1-2024 removed them) are
# declared with the feature-test macros of the editions that have them, and
# only with them.  The windows come from the editions' texts, not from
# memory: gettimeofday, getitimer, setitimer (struct itimerval, ITIMER_*),
# sighold, sigrelse, sigignore, sigpause, sigset (SIG_HOLD), siginterrupt,
# the POLL_* codes and setpgrp are XSI in SUSv2 (_XOPEN_SOURCE 500), SUSv3
# (600) and SUSv4 (700), gone in POSIX.1-2024 (800); usleep is in SUSv2 and
# SUSv3 only.  None is in POSIX.1-2008 or POSIX.1-2001 without XSI
# (_POSIX_C_SOURCE alone).  With no macro everything is visible.
# usage: legacy.sh CC   (CC is a libc-cc invocation, e.g. "tools/libc-cc gcc"; LEGACY_CC=c17
# uses that compiler as it is, inside V86BSD)
cc=${LEGACY_CC:-"$(dirname "$0")/../../../lib/libc/tools/libc-cc ${1:-gcc}"}
tmp=${TMPDIR:-/tmp}/legacy.$$; mkdir -m 700 "$tmp" || exit 1; trap 'rm -rf "$tmp"' EXIT
case $cc in *c17*) werr= ;; *) werr=-Werror=implicit-function-declaration ;; esac

# name|program
items='gettimeofday|#include <sys/time.h>
int main(void) { struct timeval t; return gettimeofday(&t, 0); }
itimer|#include <sys/time.h>
int main(void) { struct itimerval v; return getitimer(ITIMER_REAL, &v) + setitimer(ITIMER_PROF, &v, 0) + ITIMER_VIRTUAL; }
usleep|#include <unistd.h>
int main(void) { return usleep(1); }
setpgrp|#include <unistd.h>
int main(void) { return (int)setpgrp(); }
sigfuncs|#include <signal.h>
int main(void) { sighold(SIGINT); sigrelse(SIGINT); sigignore(SIGUSR1); sigpause(SIGINT); siginterrupt(SIGINT, 0); return sigset(SIGINT, SIG_HOLD) == SIG_ERR; }
sigconst|#include <signal.h>
void (*h)(int) = SIG_HOLD; int y = POLL_IN + POLL_OUT + POLL_MSG + POLL_ERR + POLL_PRI + POLL_HUP;
int main(void) { return h != 0 || y == 0; }'

# set|visible (space-separated item names; "-" none).  Macro sets use ";" for spaces.
sets='|gettimeofday itimer usleep setpgrp sigfuncs sigconst
-D_GNU_SOURCE|gettimeofday itimer usleep setpgrp sigfuncs sigconst
-D_DEFAULT_SOURCE|gettimeofday itimer usleep setpgrp sigfuncs sigconst
-D_XOPEN_SOURCE=500|gettimeofday itimer usleep setpgrp sigfuncs sigconst
-D_XOPEN_SOURCE=600|gettimeofday itimer usleep setpgrp sigfuncs sigconst
-D_XOPEN_SOURCE=700|gettimeofday itimer setpgrp sigfuncs sigconst
-D_POSIX_C_SOURCE=200809L;-D_XOPEN_SOURCE=700|gettimeofday itimer setpgrp sigfuncs sigconst
-D_XOPEN_SOURCE=800|-
-D_POSIX_C_SOURCE=202405L|-
-D_POSIX_C_SOURCE=200809L|-
-D_POSIX_C_SOURCE=200112L|-
-D_POSIX_C_SOURCE=199506L|-'

fail=0 n=0
names="gettimeofday itimer usleep setpgrp sigfuncs sigconst"
prog_of() {	# the program of item $1
	printf '%s\n' "$items" | awk -F'|' -v n="$1" '
		index($0, n "|") == 1 { on = 1; sub(/^[^|]*\|/, ""); print; next }
		/^[a-z]+\|/ { on = 0; next }
		on { print }'
}
while IFS= read -r s; do
	macros=$(printf '%s' "${s%%|*}" | tr ';' ' ')
	visible=${s#*|}
	for name in $names; do
		prog_of "$name" > "$tmp/t.c"
		want=no
		case " $visible " in *" $name "*) want=yes ;; esac
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
