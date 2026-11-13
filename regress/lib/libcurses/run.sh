#!/bin/sh
# Copyright (c) 2026, asprintf
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# run.sh - the behaviour tests of libcurses (lib/libcurses): each t_*.c
# runs on a pseudo-terminal (../testlib/ptydrive.c) as a terminal of type
# v86test (v86test.ti); vtscreen.c makes the screen of what it wrote, which
# with the program's log must be t_*.expect.
#   sh run.sh [test...]
# On V86BSD the tests are built with c17 and the system's libcurses, the
# terminal with its tic.  Elsewhere lib/libcurses and usr.bin/tic are built
# into out/ with ../../../lib/libc/tools/libc-cc gcc (V86BSD's C library,
# built there first).
#   REF=1  the tests also with the host's curses (REFCC, default cc;
#          REFLIBS, default -lncursesw; the host's tic), and where its
#          outcome differs from the expectation that is reported (only)
# What is typed on the terminal is the ptydrive ops of the test's " * OPS:"
# line (default w:, wait for the end).

cd "$(dirname "$0")" || exit 1
unset MAKEFLAGS LINES COLUMNS
out=out
mkdir -p "$out" || exit 1
here=$(pwd)
top=$(cd ../../.. && pwd)
if [ "$(uname -s)" = V86BSD ]; then
	tcc=c17 cc=c17 curses="-l curses" tic=tic
else
	tcc=${HOSTCC:-cc}
	cc="$top/lib/libc/tools/libc-cc gcc"
	if [ ! -f "$out/curses/lib/libcurses.a" ] || [ ! -x "$out/tic" ]; then
		rm -rf "$out/src" "$out/curses" && mkdir -p "$out/src" &&
		cp -R "$top/lib/libcurses" "$out/src/" &&
		(cd "$out/src/libcurses" && make CC="$cc" PREFIX="$here/$out/curses" install) > "$out/build.log" 2>&1 &&
		$cc -D_DEFAULT_SOURCE -I"$top/lib/libcurses/terminfo" -I"$top/lib/libcurses/cdb" \
			-I"$top/lib/libcurses/compat" -o "$out/tic" "$top/usr.bin/tic/tic.c" \
			"$out/curses/lib/libcurses.a" >> "$out/build.log" 2>&1 ||
			{ echo "libcurses: build failed (see $out/build.log)"; exit 1; }
	fi
	cc="$cc -I$here/$out/curses/include" curses=$here/$out/curses/lib/libcurses.a tic=$here/$out/tic
fi
$tcc -o "$out/ptydrive" ../testlib/ptydrive.c && $tcc -o "$out/vtscreen" vtscreen.c &&
	"$tic" -o "$here/$out/v86test.cdb" v86test.ti > /dev/null ||
	{ echo "ptydrive, vtscreen, the terminal: build failed"; exit 1; }
if [ -n "$REF" ]; then
	refcc=${REFCC:-cc} reflibs=${REFLIBS:--lncursesw}
	rm -rf "$out/ref-terminfo"
	tic -x -o "$here/$out/ref-terminfo" v86test.ti || { echo "the host's tic failed"; exit 1; }
fi

# run PROGRAM RESULT TERMINFO: the program on the terminal; RESULT is its
# screen, its log and how it ended
run() {
	rm -f "$2.raw" "$2.log"
	set -f	# (the ops are words, not patterns)
	TERM=v86test TERMINFO=$3 CT_LOG=$2.log "$out/ptydrive" -T 20 -o "$2.raw" $ops -- "$1" > "$2.drive"
	set +f
	{
		"$out/vtscreen" 8 20 < "$2.raw"
		echo "--- log"
		cat "$2.log" 2>/dev/null
		echo "--- $(tail -n 1 "$2.drive")"
	} > "$2"
}

fail=0 n=0
for t in t_*.c; do
	b=${t%.c}
	case " ${*:-$b} " in *" $b "*) ;; *) continue ;; esac
	n=$((n + 1))
	ops=$(sed -n 's/^ \* OPS: \(.*\)$/\1/p' "$t" | sed 's, *\*/ *$,,')
	[ -n "$ops" ] || ops=w:
	if ! $cc -o "$out/$b" "$t" $curses; then
		echo "$b: build failed"; fail=$((fail + 1)); continue
	fi
	run "$here/$out/$b" "$out/$b.txt" "$here/$out/v86test.cdb"
	if cmp -s "$b.expect" "$out/$b.txt"; then
		echo "$b: ok"
	else
		echo "$b: FAILED (diff $b.expect $out/$b.txt)"; fail=$((fail + 1))
	fi
	if [ -n "$REF" ]; then
		if ! $refcc -o "$out/$b.ref" "$t" $reflibs; then
			echo "$b: the host's curses: build failed"
		else
			run "$here/$out/$b.ref" "$out/$b.ref.txt" "$here/$out/ref-terminfo"
			cmp -s "$b.expect" "$out/$b.ref.txt" ||
				echo "$b: the host's curses differs (diff $b.expect $out/$b.ref.txt)"
		fi
	fi
done
echo "tests: $((n - fail)) passed, $fail failed"
[ $fail = 0 ]
