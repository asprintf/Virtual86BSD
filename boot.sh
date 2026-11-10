#!/bin/sh
# boot.sh - boot a V86BSD system in QEMU: V86BSD's kernel with the disk image
# mkdisk.sh made.  The same as "make boot"; the options (-egi, -m memory, -n,
# -k kernel, -p hostport:port, image, -- qemu-argument...) are those of
# distrib/boot.sh, which does the work (see there).
exec sh "$(dirname "$0")/distrib/boot.sh" "$@"
