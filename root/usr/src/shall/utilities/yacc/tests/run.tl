/*
 * run.tl - the regression suite of yacc: each tests/cases/NAME.sh runs in
 * an empty temporary directory with $YACC (the yacc under test), $CC (from
 * $TESTCC or $CC, else c17, c99 or cc), $CWARN, $CPEDANTIC and $CQUIET
 * (strict and quiet options, where the compiler takes them), $LIBDIR (the
 * directory holding liby.a) and $GRAMMARS (tests/grammars).  Its standard
 * output and standard error are compared with tests/expected/NAME.out.
 *
 * usage: tl run.tl YACC [--freeze] [NAME...]
 */
include "../../testlib/cases.tl";

var yacc = abspath(argv[0]);
var env = cc_env();
env.YACC = yacc;
env.LC_ALL = "C";
env.LIBDIR = dirname(yacc);
env.GRAMMARS = scriptdir + "/grammars";
exit(run_cases({dir: scriptdir, args: slice(argv, 1), verbose: true, env: env}));
