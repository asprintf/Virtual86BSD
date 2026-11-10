#!/bin/sh
# mkabi.sh - the files that depend on the system call ABI the library is
# built for, written to a directory of the build (only if they change):
#
#   syscall_nr.h  SYS_name numbers: <internal/syscall.h> includes it
#   abi.h         __V86BSD_NATIVE_ABI, defined for native only
#   nr.sed        the numbers the assembly sources use: a sed script that
#                 turns $SYS_name into the number (no symbols in the objects)
#   note.s        the brand of V86BSD's own ABI for the start files (native);
#                 empty for linux
#
# Usage: sh mkabi.sh linux|native SYSCALL_H OUTDIR
#   linux   Linux's numbers: internal/syscall_nr_linux.h
#   native  V86BSD's own: the kernel's generated <sys/syscall.h> (SYSCALL_H)
cd "$(dirname "$0")/.." || exit 1
abi=$1 hdr=$2 out=$3
case $abi in
linux) nr=internal/syscall_nr_linux.h ;;
native) nr=$hdr ;;
*) echo "mkabi.sh: TARGET_ABI is linux or native, not '$abi'" >&2; exit 1 ;;
esac
[ -f "$nr" ] || { echo "mkabi.sh: no $nr" >&2; exit 1; }
mkdir -p "$out" || exit 1
put() {   # put FILE: stdin to $out/FILE unless it is the same
	cat > "$out/$1.new" || exit 1
	if cmp -s "$out/$1.new" "$out/$1" 2>/dev/null; then rm -f "$out/$1.new"
	else mv "$out/$1.new" "$out/$1" || exit 1; fi
}
if [ "$abi" = native ]; then echo "#define __V86BSD_NATIVE_ABI 1"; else echo "/* the Linux ABI */"; fi | put abi.h
# the C header is the table itself (the same bytes, from its one source)
put syscall_nr.h < "$nr"
# only what the assembly uses
for n in rt_sigreturn rt_sigprocmask clone exit munmap; do
	awk -v n="SYS_$n" '$1 == "#define" && $2 == n { print "s/\\$" n ",/$" $3 ",/g" }' "$nr"
done | put nr.sed
if [ "$abi" = native ]; then put note.s < config/note_native.s; else : | put note.s; fi
