#!/bin/sh
# kill - terminate or signal processes: the shell's kill, for the exec
# family and the utilities that run others (env, find, nice, nohup, time,
# xargs).  POSIX.1-2024 (XCU 1.6) requires this of kill alone among the
# intrinsic utilities.  Job IDs (%n) mean nothing outside a shell.
command kill "$@"
