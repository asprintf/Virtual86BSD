/*
 * run.tl - the regression suite of lex: each tests/cases/NAME.sh runs in an
 * empty temporary directory with $LEX (the lex under test), $CC (from
 * $TESTCC or $CC, else c17, c99 or cc), $CWARN, $CPEDANTIC and $CQUIET
 * (strict and quiet options, where the compiler takes them), $LIBDIR (the
 * directory holding libl.a) and $LEXERS (tests/lexers).  Its standard
 * output and standard error are compared with tests/expected/NAME.out.
 *
 * usage: tl run.tl LEX [--freeze] [NAME...]
 */
include "../../testlib/cases.tl";

var lex = abspath(argv[0]);
var env = cc_env();
env.LEX = lex;
env.LC_ALL = "C";
env.LIBDIR = dirname(lex);
env.LEXERS = scriptdir + "/lexers";
exit(run_cases({dir: scriptdir, args: slice(argv, 1), verbose: true, env: env}));
