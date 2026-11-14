#!/bin/sh
# Copyright (c) 2026, Joost Orchidee Winter
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

# check-namespace.sh libc.a - the ISO C and POSIX namespace rules for a
# static libc.
#
# A strictly conforming ISO C program may define any external name that
# ISO C does not reserve (C17 7.1.3: reserved are the library's names and
# all names beginning with '_'); a POSIX application, any name that POSIX
# (XSH 2.2.2) does not reserve either.  So linking in any member that
# defines an ISO C (POSIX) name must not bring along, directly or through
# the members it pulls in, a strong definition of or a reference to any
# other name.  Weak definitions (nm type V/W) may be overridden and are
# allowed.  The POSIX names are those of regress/os-test/include/*.api
# (POSIX.1-2024): a name that edition removed (ftw, usleep ...) is not
# reserved.
lib=${1:?usage: check-namespace.sh libc.a}
here=$(dirname "$0")
api=$here/../../../regress/os-test/include
tmp=${TMPDIR:-/tmp}/check-namespace.$$
mkdir -m 700 "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT INT TERM
sed 's/^\[[^]]*\] *//; s/^maybe_define //; s/^parent [^ ]* //' "$api"/*.api |
	awk '$1 == "function" || $1 == "maybe_function" || $1 == "external" { sub(/:$/, "", $2); print $2 }' > "$tmp/posix"
[ -s "$tmp/posix" ] || { echo "check-namespace.sh: no names in $api" >&2; exit 1; }
nm -P -A "$lib" > "$tmp/nm" || exit 1
awk -v isofile="$here/iso-c-names.txt" -v posixfile="$tmp/posix" '
BEGIN {
	while ((getline n < isofile) > 0) iso[n] = 1
	while ((getline n < posixfile) > 0) posix[n] = 1
	# the prefixes POSIX reserves for external names (XSH 2.2.2)
	res = "^(aio_|lio_|inet_|dli_|pthread_|mq_|dbm_|ipc_|sched_|sem_|shm_|clock_|timer_|cnd_|mtx_|thrd_|tss_|posix_|atomic_[a-z]|memory_[a-z]|str[a-z]|mem[a-z]|wcs[a-z]|is[a-z]|to[a-z]|in6_|in6addr_|if_|re_)|_t$"
}
{
	obj = $1; sub(/^.*\[/, "", obj); sub(/\]:?$/, "", obj); sym = $2; t = $3
	if (t == "U" || t == "w" || t == "v") { refs[obj] = refs[obj] " " sym; next }
	if (t ~ /^[TDBRC]$/) {
		defs[obj] = defs[obj] " " sym
		if (!(sym in who)) who[sym] = obj
		if (sym in iso) isoobj[obj] = 1
		else if (sym in posix) posixobj[obj] = 1
	}
}
function reserved(s, pass) {
	if (substr(s, 1, 1) == "_" || (s in iso)) return 1
	return pass == 2 && ((s in posix) || s ~ res)
}
function scan(pass, o,    seen, queue, n, i, j, k, m, s, names) {
	n = 1; queue[1] = o; seen[o] = 1
	for (i = 1; i <= n; i++) {
		m = queue[i]
		k = split(defs[m] refs[m], names, " ")
		for (j = 1; j <= k; j++) {
			s = names[j]
			if (!reserved(s, pass) && !((o, s) in told)) {
				print o ": linking it brings in non-" (pass == 1 ? "ISO" : "POSIX") " name " s " (from " m ")"
				told[o, s] = 1; bad++
			}
		}
		k = split(refs[m], names, " ")
		for (j = 1; j <= k; j++)
			if ((names[j] in who) && !(who[names[j]] in seen)) {
				seen[who[names[j]]] = 1; queue[++n] = who[names[j]]
			}
	}
}
END {
	for (o in isoobj) scan(1, o)
	for (o in posixobj) scan(2, o)
	print "namespace: " bad + 0 " problem(s)"
	exit bad != 0
}' "$tmp/nm"
