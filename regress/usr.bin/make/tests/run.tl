/*
 * run.tl - the regression suite of make.
 *
 * usage: tl run.tl MAKE [--freeze] [NAME...]
 *
 * Each case is a shell script run by sh in a new empty directory with $M
 * the binary under test, TZ=UTC, LC_ALL=C and umask 022.  Its standard
 * output and standard error (together) are compared with
 * tests/expected/NAME.out.  --freeze writes the expected files instead.
 */
include "../../../lib/testlib/cases.tl";

if (!len(argv)) {
    eprintf("usage: tl run.tl MAKE [--freeze] [NAME...]\n");
    exit(2);
}
var m = abspath(argv[0]);
exit(run_cases({dir: scriptdir, args: slice(argv, 1), form: "dot", timeout: 60, home: true,
                subst: [[m, "$M"]],
                env: {MAKEFLAGS: nil, MAKELEVEL: nil, MFLAGS: nil, MAKE: nil, CURDIR: nil,
                      M: m, TZ: "UTC", LC_ALL: "C", TMPDIR: "/tmp", LIB: scriptdir + "/lib.sh"}}));
