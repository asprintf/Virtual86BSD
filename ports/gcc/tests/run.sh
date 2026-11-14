#!/bin/sh
# run.sh DIR - inside V86BSD: run the programs that build.sh -t made (copied
# to DIR, with expect-* beside them) and compare their output.
dir=${1:?usage: sh run.sh DIR}
cd "$dir" || exit 2
fail=0
for m in static dynamic; do
	for p in hello rt rt-cxx; do
		if [ $p = hello ]; then HOME=/root ./$p-$m a b > out.$p.$m 2>&1; else ./$p-$m a b > out.$p.$m 2>&1; fi
		if cmp -s out.$p.$m expect-$p; then echo "$p-$m: ok"; else echo "$p-$m: FAILED"; diff out.$p.$m expect-$p | head -n 5; fail=1; fi
	done
done
exit $fail
