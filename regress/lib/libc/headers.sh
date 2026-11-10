#!/bin/sh
# Header checks: every public header must compile on its own in each
# language mode, twice in a row (include guards), and in strict ISO C
# mode must not declare POSIX-only names.
# usage: headers.sh CC   (CC is a libc-cc invocation, e.g. "tools/libc-cc gcc")
cc="$(dirname "$0")/../../../lib/libc/tools/libc-cc ${1:-gcc}"
inc=$(cd "$(dirname "$0")/../../../include" && pwd)
tmp=${TMPDIR:-/tmp}/headers.$$; mkdir -m 700 "$tmp" || exit 1; trap 'rm -rf "$tmp"' EXIT
fail=0
case $cc in
*c17*)  # c17 has no language-mode or warning options; it is always C17
	modes="-DMODE0 -D_POSIX_C_SOURCE=202405L -D_XOPEN_SOURCE=800 -D_POSIX_C_SOURCE=200809L"
	wflags=; strict=;;
*)	modes="-std=c17|-std=c17 -D_POSIX_C_SOURCE=202405L|-std=c17 -D_XOPEN_SOURCE=800|-std=gnu17|-std=c99 -D_XOPEN_SOURCE=700|-std=c11|-std=c89 -Wno-long-long"
	wflags="-Wall -Wextra -pedantic -Werror"; strict=1;;
esac
case $cc in *c17*) IFS0=" ";; *) IFS0="|";; esac
for h in $(cd "$inc" && find . -name '*.h' ! -path './bits/*' | sed 's|^\./||' | sort); do
	oldifs=$IFS; IFS=$IFS0; set -f
	for mode in $modes; do
		IFS=$oldifs
		printf '#include <%s>\n#include <%s>\nint main(void){return 0;}\n' "$h" "$h" > "$tmp/t.c"
		if ! $cc $mode $wflags -c -o "$tmp/t.o" "$tmp/t.c" 2> "$tmp/err"; then
			echo "FAIL: <$h> with $mode"; sed 's/^/    /' "$tmp/err" | head -5; fail=1
		fi
	done
	IFS=$oldifs; set +f
done
[ "$strict" ] || { [ $fail = 0 ] && echo "headers ok"; exit $fail; }
# Strict ISO C: POSIX extensions must be invisible.
for pair in string.h:strdup string.h:strtok_r string.h:memccpy stdlib.h:posix_memalign stdlib.h:qsort_r \
            limits.h:PATH_MAX limits.h:SSIZE_MAX ctype.h:isalpha_l string.h:locale_t \
            signal.h:kill signal.h:SIGHUP signal.h:sigset_t signal.h:siginfo_t \
            math.h:M_PI math.h:signgam math.h:j0 math.h:y1 \
            threads.h:pthread_t threads.h:pthread_mutex_t threads.h:clockid_t; do
	h=${pair%%:*}; n=${pair#*:}
	printf '#include <%s>\nint %s;\nint main(void){return 0;}\n' "$h" "$n" > "$tmp/t.c"
	if ! $cc -std=c17 -c -o "$tmp/t.o" "$tmp/t.c" 2>/dev/null; then
		echo "FAIL: <$h> exposes $n in strict ISO C mode"; fail=1
	fi
done
[ $fail = 0 ] && echo "headers ok"
exit $fail
