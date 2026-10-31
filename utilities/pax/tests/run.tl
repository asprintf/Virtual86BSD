/*
 * run.tl - the regression suite of pax
 *
 * usage: tl run.tl PAX [--freeze] [NAME...]
 *
 * Each case, tests/cases/NAME.sh, is run by sh in a new empty directory
 * with $PAX the binary tested, TZ=UTC, LC_ALL=C and umask 022; its output
 * and error output (together) are compared with tests/expected/NAME.out.
 * --freeze writes the expected files instead.  The cases have the helpers
 * $FSTAT and $LTOUCH (../testlib), $TREECMP (treecmp.tl), $LIB (lib.sh)
 * and $U8, a UTF-8 locale ("" if there is none).
 */
include "../../testlib/cases.tl";

if (!len(argv)) {
    eprintf("usage: tl run.tl PAX [--freeze] [NAME...]\n");
    exit(2);
}
var helpers = mkdtemp("paxh.");
var env = {PAX: abspath(argv[0]), TZ: "UTC", LC_ALL: "C", TMPDIR: "/tmp",
           TREECMP: scriptdir + "/treecmp.tl", LIB: scriptdir + "/lib.sh",
           FSTAT: build("fstat", helpers), LTOUCH: build("ltouch", helpers), U8: utf8_locale()};
var st = run_cases({dir: scriptdir, args: slice(argv, 1), form: "dot", timeout: 60, home: true, env: env});
rmtree(helpers);
exit(st);
