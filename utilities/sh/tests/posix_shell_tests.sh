#!/bin/sh
# posix_shell_tests.sh - run the POSIX Shell Tests (the test suite of yash,
# https://github.com/posix-shell-tests/posix-shell-tests, GPL-2; not in
# this tree) against a shell.
#
# Usage: sh posix_shell_tests.sh SUITE SHELL [file.tst...]
#
#   SUITE  a checkout of posix-shell-tests (its src/ is copied to a
#          directory of results, results-<shell's name>)
#   SHELL  the shell to test, an absolute pathname
#
# The suite's runner needs yash, and its job-control tests a terminal:
# each file is run on a pseudo-terminal of its own with script(1) (from
# util-linux), 300 seconds at most.  A run that is cut short continues
# where it stopped.  At the end: the counts of passed, failed and skipped
# cases, and the failures.
suite=${1:?usage: sh posix_shell_tests.sh SUITE SHELL [file.tst...]}
shell=${2:?usage: sh posix_shell_tests.sh SUITE SHELL [file.tst...]}
shift 2
command -v yash >/dev/null 2>&1 || { echo "posix_shell_tests: yash is needed" >&2; exit 2; }
command -v script >/dev/null 2>&1 || { echo "posix_shell_tests: script(1) is needed" >&2; exit 2; }
out=results-$(basename "$shell")
[ -d "$out" ] || cp -R "$suite/src" "$out" || exit 2
cd "$out" || exit 2
[ $# -gt 0 ] || set -- *.tst
for f; do
	t=${f%.tst}.trs
	[ -f "$t" ] && grep -q '^%%%' "$t" && continue
	LANG=C.UTF-8 script -qec "timeout -k 5 300 yash ./run-test.sh $shell $f" /dev/null \
		</dev/null >/dev/null 2>&1
done
cat ./*.trs | grep -E '^%%% (PASSED|FAILED|SKIPPED)' | awk '{ n[$2]++ }
	END { printf "passed %d, failed %d, skipped %d\n", n["PASSED:"], n["FAILED:"], n["SKIPPED:"] }'
cat ./*.trs | grep '^%%% FAILED' | sed 's/^%%% FAILED: /  /'
