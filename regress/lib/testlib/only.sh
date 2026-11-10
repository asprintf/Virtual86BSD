#!/bin/sh
# only.sh ERE: each part of the standard input that the extended regular
# expression matches, on a line of its own (what grep -o does, which POSIX
# grep does not have), by awk's match().
RE=$1 awk '{
	s = $0
	while (s != "" && match(s, ENVIRON["RE"])) {
		print substr(s, RSTART, RLENGTH)
		if (RLENGTH == 0) RLENGTH = 1
		s = substr(s, RSTART + RLENGTH)
	}
}'
