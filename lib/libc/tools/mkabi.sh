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

# mkabi.sh - the files that depend on the system call ABI the library is
# built for (FreeBSD's), written to a directory of the build (only if they
# change):
#
#   syscall_nr.h  SYS_name numbers: <internal/syscall.h> includes it
#   nr.sed        the numbers the assembly sources use: a sed script that
#                 turns $SYS_name into the number (no symbols in the objects)
#   note.s        the FreeBSD ABI note (.note.tag) for the start files
#
# Usage: sh mkabi.sh freebsd SYSCALL_H OUTDIR
#   freebsd  the numbers of the kernel's sys/sys/syscall.h (SYSCALL_H)
cd "$(dirname "$0")/.." || exit 1
abi=$1 hdr=$2 out=$3
case $abi in
freebsd) ;;
*) echo "mkabi.sh: TARGET_ABI is freebsd, not '$abi'" >&2; exit 1 ;;
esac
[ -f "$hdr" ] || { echo "mkabi.sh: no $hdr" >&2; exit 1; }
mkdir -p "$out" || exit 1
put() {   # put FILE: stdin to $out/FILE unless it is the same
	cat > "$out/$1.new" || exit 1
	if cmp -s "$out/$1.new" "$out/$1" 2>/dev/null; then rm -f "$out/$1.new"
	else mv "$out/$1.new" "$out/$1" || exit 1; fi
}
# the C header is the table itself (the same bytes, from its one source)
put syscall_nr.h < "$hdr"
# only what the assembly uses
for n in sigreturn sigprocmask sigaltstack munmap vfork thr_new thr_exit sysarch _exit getcontext setcontext swapcontext; do
	awk -v n="SYS_$n" '$1 == "#define" && $2 == n { print "s/\\$" n ",/$" $3 ",/g" }' "$hdr"
done | put nr.sed
put note.s < config/note_freebsd.s
