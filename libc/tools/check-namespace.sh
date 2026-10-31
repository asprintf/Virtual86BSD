#!/bin/sh
# check-namespace.sh libc.a - the ISO C namespace rule for a static libc.
#
# A strictly conforming ISO C program may define any external name that
# ISO C does not reserve (C17 7.1.3: reserved are the library's names and
# all names beginning with '_').  So linking in any member that defines an
# ISO C name must not bring along, directly or through the members it
# pulls in, a strong definition of or a reference to any other name.
# Weak definitions (nm type V/W) may be overridden and are allowed.
lib=${1:?usage: check-namespace.sh libc.a}
here=$(dirname "$0")
nm -P -A "$lib" | awk -v isofile="$here/iso-c-names.txt" '
BEGIN { while ((getline n < isofile) > 0) iso[n] = 1 }
{
	obj = $1; sub(/^.*\[/, "", obj); sub(/\]:?$/, "", obj); sym = $2; t = $3
	if (t == "U") { refs[obj] = refs[obj] " " sym; next }
	if (t ~ /^[TDBRC]$/) {
		defs[obj] = defs[obj] " " sym
		if (!(sym in who)) who[sym] = obj
		if (sym in iso) isoobj[obj] = 1
	}
}
function reserved(s) { return substr(s, 1, 1) == "_" || (s in iso) }
END {
	bad = 0
	for (o in isoobj) {
		split("", seen); n = 1; queue[1] = o; seen[o] = 1
		for (i = 1; i <= n; i++) {
			m = queue[i]
			k = split(defs[m] refs[m], names, " ")
			for (j = 1; j <= k; j++) {
				s = names[j]
				if (!reserved(s) && !((o, s) in told)) {
					print o ": linking it brings in non-ISO name " s " (from " m ")"
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
	print "namespace: " bad " problem(s)"
	exit bad != 0
}'
