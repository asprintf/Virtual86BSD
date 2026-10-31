/*
 * run.tl - the regression suite of bc: each tests/cases/NAME.sh runs in an
 * empty temporary directory with $BC (the bc under test), $SRCDIR (the
 * directory of the sources) and $CC (from $TESTCC or $CC, else c17, c99 or
 * cc).  Its standard output and standard error are compared with
 * tests/expected/NAME.out.
 *
 * usage: tl run.tl BC [--freeze] [NAME...]
 */
include "../../testlib/cases.tl";

exit(run_cases({dir: scriptdir, args: slice(argv, 1), verbose: true,
                env: {BC: abspath(argv[0]), LC_ALL: "C", CC: compiler(), SRCDIR: dirname(scriptdir)}}));
