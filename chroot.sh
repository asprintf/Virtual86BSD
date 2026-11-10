#!/bin/sh
# chroot.sh - enter a V86BSD system made by bootstrap.sh, with or without
# root privileges: [-m method] [root [command [argument...]]].  The options
# are those of distrib/chroot.sh, which does the work (see there).
exec sh "$(dirname "$0")/distrib/chroot.sh" "$@"
