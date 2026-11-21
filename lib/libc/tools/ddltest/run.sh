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

# run.sh - tests the double-double functions of math/__ddl.c on the host.
#   sh run.sh [OBJDIR]       POINTS=n (default 200) random points per run
# __ddl.c is compiled for the host (the library itself is built for the FreeBSD
# ABI and does not run here) with host.c, and ref.py compares the results with
# exact values.  Needs clang and python3.  Exit status 1 if a result is wrong.
top=$(cd "$(dirname "$0")/../../../.." && pwd)
here=$top/lib/libc/tools/ddltest
out=${1:-$top/obj/ddltest}
n=${POINTS:-200}
mkdir -p "$out" || exit 1
L=$top/lib/libc
clang -std=gnu17 -O2 -w -I"$L/internal" -I"$L" -c "$L/math/__ddl.c" -o "$out/ddl.o" &&
	clang -std=gnu17 -O2 -w -o "$out/host" "$here/host.c" "$out/ddl.o" -lm || exit 1
export DDLHOST=$out/host
fail=0
t() { python3 -I "$here/ref.py" "$@" || fail=1; }
for r in "-14 -3" "-2 2" "0 9"; do
	for f in cexp csinh ccosh ctanh catanh casinh cacosh cpow tgamma sinh cosh asinh; do
		t $f $n $r 7
	done
done
for f in cexp csinh ccosh ctanh catanh cacosh cpow; do t -x $f $n -4 4 11; done
for f in catanh cpow; do t -y $f $n -4 -1 21; done
t special
exit $fail
