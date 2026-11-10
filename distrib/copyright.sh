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

# copyright.sh - put the copyright and licence header on source files.
#
# Usage: sh distrib/copyright.sh [-r] [-c] -o|-n|-b file...
#        sh distrib/copyright.sh -p -o|-n|-b
#
#   -o   the original author only (Shall's files)
#   -n   the fork's author only (files the fork added)
#   -b   both: a file of Shall that the fork has largely rewritten
#   -r   replace a header the file already has (without it, a file that has
#        one is left alone: the script can be run again on every file)
#   -c   only list the files that have no header; change nothing
#   -p   only print the header in "#" lines, then a blank line (for a
#        generator that writes a file with a header: tools/genmk.sh)
#
# The licence text is that of ../LICENSE (from "Redistribution and use" on),
# so there is one place to change it.  The header goes on top of the file,
# below a "#!" line.  It is a /* */ comment in .c and .h files and "#" lines
# in everything else (.s, .sh, Makefile, .mk).  A header is recognized by a
# "Copyright (c)" line in the first twelve lines.
#
# The holders are named below.  POSIX sh and awk only.

ORIG="Joost Orchidee Winter"
NEW="asprintf"
YEAR=2026

here=$(cd "$(dirname "$0")" && pwd) || exit 2
LICENSE=$here/../LICENSE
[ -f "$LICENSE" ] || { echo "copyright: no $LICENSE" >&2; exit 2; }

mode= replace=no check=no print=no
while getopts onbrcp opt; do
	case $opt in
	o) mode=o ;;
	n) mode=n ;;
	b) mode=b ;;
	r) replace=yes ;;
	c) check=yes ;;
	p) print=yes ;;
	*) mode= ;;
	esac
done
shift $((OPTIND - 1))
if { [ -z "$mode" ] && [ $check = no ]; } || { [ $# -eq 0 ] && [ $print = no ]; }; then
	echo "usage: sh copyright.sh [-r] [-c] -o|-n|-b file..." >&2
	echo "       sh copyright.sh -p -o|-n|-b" >&2
	exit 2
fi

tmp=${TMPDIR:-/tmp}/copyright.$$
trap 'rm -f "$tmp" "$tmp.h" "$tmp.new"' 0 1 2 15

# the licence's text, from "Redistribution and use" on
body() { awk '/^Redistribution and use/ { on = 1 } on' "$LICENSE"; }

# the header for a style (c or hash), without the trailing blank line
header() {
	{
		case $mode in
		o | b) echo "Copyright (c) $YEAR, $ORIG" ;;
		esac
		case $mode in
		n | b) echo "Copyright (c) $YEAR, $NEW" ;;
		esac
		echo "All rights reserved."
		echo
		body
	} | awk -v style="$1" '
		BEGIN { if (style == "c") print "/*" }
		{ if (style == "c") print ($0 == "" ? " *" : " * " $0)
		  else print ($0 == "" ? "#" : "# " $0) }
		END { if (style == "c") print " */" }'
}

# the line of the first "Copyright (c)" in the first twelve lines, or 0
hdrline() {
	awk 'NR <= 12 && /Copyright \(c\)/ { print NR; found = 1; exit } END { if (!found) print 0 }' "$1"
}

# remove the header that is in $1 (style $2); to $tmp
strip() {
	awk -v style="$2" '
		{ line[NR] = $0 }
		END {
			for (i = 1; i <= NR && i <= 12; i++) if (line[i] ~ /Copyright \(c\)/) { c = i; break }
			for (i = c; i <= NR; i++) if (line[i] ~ /SUCH DAMAGE\./) { e = i; break }
			s = (style == "c") ? c - 1 : c
			if (style == "c") e++                  # the closing */
			if (line[e + 1] == "") e++             # the blank line after
			for (i = 1; i <= NR; i++) if (i < s || i > e) print line[i]
		}' "$1" > "$tmp"
}

[ $print = yes ] && { header hash; echo; exit 0; }

status=0
for f in "$@"; do
	[ -f "$f" ] || { echo "copyright: $f: no such file" >&2; status=1; continue; }
	case $f in
	*.c | *.h) style=c ;;
	*) style=hash ;;
	esac
	h=$(hdrline "$f")
	if [ $check = yes ]; then
		[ "$h" -eq 0 ] && echo "$f"
		continue
	fi
	if [ "$h" -ne 0 ]; then
		[ $replace = yes ] || continue
		strip "$f" "$style"
	else
		cat "$f" > "$tmp"
	fi
	[ -s "$tmp" ] || { echo "copyright: $f: empty, left alone" >&2; continue; }
	header $style > "$tmp.h"
	{
		# below a #! line
		case $(head -n 1 "$tmp") in
		'#!'*) head -n 1 "$tmp"; cat "$tmp.h"; echo; tail -n +2 "$tmp" ;;
		*) cat "$tmp.h"; echo; cat "$tmp" ;;
		esac
	} > "$tmp.new" && cat "$tmp.new" > "$f"
	rm -f "$tmp.new"
done
exit $status
