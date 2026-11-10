/*
 * run.tl - the regression suite of mailx.
 *
 * usage: tl run.tl MAILX [--freeze] [NAME...]
 *
 * Each case is a shell script run by sh in a new empty directory with $M
 * the binary under test, TZ=UTC, LC_ALL a UTF-8 locale (or C if there is
 * none) and umask 022.  Its standard output and standard error (together)
 * are compared with tests/expected/NAME.out.  --freeze writes the
 * expected files instead.
 */
include "../../../lib/testlib/cases.tl";

if (!len(argv)) {
    eprintf("usage: tl run.tl MAILX [--freeze] [NAME...]\n");
    exit(2);
}
var m = abspath(argv[0]);
var u8 = utf8_locale();
var env = {MAKEFLAGS: nil, MAKELEVEL: nil, MFLAGS: nil, MAKE: nil, CURDIR: nil,
           M: m, TZ: "UTC", LC_ALL: u8 != "" ? u8 : "C", TMPDIR: "/tmp", LIB: scriptdir + "/lib.sh"};
for (var k : ["MAIL", "MAILRC", "MBOX", "DEAD", "EDITOR", "VISUAL", "PAGER", "LISTER", "SHELL", "TERM"])
    env[k] = nil;
/* MAIL and MAILRC: in the case's directory, which run_cases makes */
exit(run_cases({dir: scriptdir, args: slice(argv, 1), form: "dot", timeout: 60, home: true,
                subst: [[m, "$M"]], env: env, dirvars: {MAIL: "sys", MAILRC: "rc"}}));
