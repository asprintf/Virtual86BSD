#!/bin/sh
# tl.sh - runs a tl script: sh tl.sh SCRIPT [ARGUMENT...]
#
# The interpreter (tl.c, beside this file) is compiled for the run, with
# $CC or the first of c17, c99 and cc, and exported as TLBIN for the
# script and what it starts.  The exit status is the script's.
d=$(dirname "$0")
cc=${CC:-}
[ -n "$cc" ] || for c in c17 c99 cc; do command -v $c >/dev/null 2>&1 && { cc=$c; break; }; done
TLBIN=${TMPDIR:-/tmp}/tl.$$
trap 'rm -f "$TLBIN"' EXIT
trap 'rm -f "$TLBIN"; trap - INT; kill -s INT $$' INT
$cc -o "$TLBIN" "$d/tl.c" || { echo "tl.sh: cannot compile $d/tl.c" >&2; exit 2; }
export TLBIN
"$TLBIN" "$@"
