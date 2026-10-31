/*
 * equiv.tl - equivalence tests: run programs under two builds of this awk
 * (for example the hand-written-parser release and the yacc/lex build) and
 * report every difference in standard output, standard error and exit
 * status.
 *
 *     tl equiv.tl OLD NEW cases               the regression cases (tests/cases*.sh)
 *     tl equiv.tl OLDD NEWD ast SEED N        random programs, parse trees compared
 *     tl equiv.tl OLDD NEWD mut SEED N        mutated random programs
 *     tl equiv.tl OLDD NEWD grid PART PARTS   the error grid
 *     tl equiv.tl OLDD NEWD edge              the hand-written corner cases (genawk.tl EDGE)
 *     tl equiv.tl OLD NEW edge-run            the same, run instead of parsed
 *     tl equiv.tl OLD NEW run SEED N          random programs run on input
 *
 * OLDD/NEWD are builds with tests/astdump.c linked in (see astdump.c); the
 * parse is printed (AWK_AST_DUMP) instead of run.  Both binaries should be
 * called "awk", since messages start with the name.  The random programs
 * come from tl's random numbers: a seed does not give the programs that
 * the first version gave.  AWK_TEST_LC is the locale (C by default), AWK_GRID_FROM
 * the first template of the grid, AWK_TEST_LOCPATH the LOCPATH of the
 * Latin-1 locale of the edge cases (they are not run in it without).
 */
include "../../testlib/testlib.tl";
include "genawk.tl";

var HERE = scriptdir;
var WORK = mkdtemp("awk-equiv-");
var DATA = "a b c\n1 2 3\nx  y\tz\n\n10 20 abc\n";
var EXTRA = {};         /* more variables for the environment (edge) */

/* [stdout, stderr, status], or ["timeout"] */
func run_awk(awk, prog, how, dump, timeout) {
    var lc = getenv("AWK_TEST_LC");
    var env = copy(EXTRA);
    env.LC_ALL = lc == nil ? "C" : lc;
    if (dump)
        env.AWK_AST_DUMP = "1";
    var args;
    if (how == nil || how == "arg")
        args = [awk, prog];
    else {
        var fn = WORK + "/prog.awk";
        writefile(fn, prog);
        args = [awk, "-f", fn];
    }
    var datafn = WORK + "/data";
    writefile(datafn, DATA);
    push(args, datafn);
    var r = run(args, {stdin: "null", timeout: timeout == nil ? 5 : timeout, cwd: WORK, env: env});
    if (r.timedout)
        return ["timeout"];
    return [r.out, replace(r.err, WORK, "$WORK"), r.status];
}

var n_runs = 0, n_diffs = 0, n_timeouts = 0, n_flaky = 0;

/* rerun: nil, or the arguments of run_awk for the old binary again */
func compare(name, a, b, prog, rerun) {
    n_runs++;
    if (a == ["timeout"] && b == ["timeout"]) {
        n_timeouts++;
        return;
    }
    if (a == b)
        return;
    if (rerun != nil) {
        /* a program whose output depends on timing (pipes to commands)
           gives different results from the same binary: not counted */
        for (var k = 0; k < 3; k++)
            if (run_awk(rerun[0], rerun[1], rerun[2], rerun[3], rerun[4]) != a) {
                n_flaky++;
                return;
            }
    }
    n_diffs++;
    printf("DIFF %s\n", name);
    printf("  prog: %q\n", prog);
    if (len(a) == 3 && len(b) == 3) {
        var whats = ["stdout", "stderr", "status"];
        for (var i = 0; i < 3; i++)
            if (a[i] != b[i]) {
                var sa = a[i], sb = b[i];
                if (typeof(sa) == "string" && len(sa) > 600) {
                    /* show where they part */
                    var k = 0;
                    while (k < min(len(sa), len(sb)) && sa[k] == sb[k])
                        k++;
                    sa = slice(sa, max(0, k - 200), k + 200);
                    sb = slice(sb, max(0, k - 200), k + 200);
                }
                printf("  %s old: %q\n", whats[i], sa);
                printf("  %s new: %q\n", whats[i], sb);
            }
    } else
        printf("  old: %q\n  new: %q\n", a, b);
    flush();
}

func summary(what) {
    printf("%s: %d runs, %d differences, %d both timed out, %d nondeterministic\n",
           what, n_runs, n_diffs, n_timeouts, n_flaky);
}

/* every regression case, with $AWK set to each binary in turn */
func cmd_cases(old, new) {
    var cases = [];
    for (var fn : ["cases.sh", "cases2.sh"])
        for (var line : lines(readfile(HERE + "/" + fn))) {
            var m = match(line, "^t[[:space:]]+([^[:space:]]+)[[:space:]]+'(.*)'[[:space:]]*$");
            if (m != nil)
                push(cases, [m[1], m[2]]);
        }
    for (var c : cases) {
        var res = [];
        for (var awk : [old, new]) {
            var r = run(["sh", "-c", c[1]], {cwd: HERE, env: {AWK: awk}, stdin: "null", timeout: 20});
            if (r.timedout)
                push(res, ["timeout"]);
            else
                push(res, [replace(r.out, awk, "AWK"), replace(r.err, awk, "AWK"), r.status]);
        }
        compare("case " + c[0], res[0], res[1], c[1]);
    }
}

func cmd_ast(old, new, seed, n, mut) {
    srand(seed);
    for (var k = 0; k < n; k++) {
        var toks = program(rand(1, 4));
        if (mut)
            toks = mutate(toks);
        var prog = render(toks);
        var how = k % 3 ? "arg" : "file";
        compare(sprintf("%s %d/%d [%s]", mut ? "mut" : "ast", seed, k, how),
                run_awk(old, prog, how, true), run_awk(new, prog, how, true), prog);
    }
}

func cmd_grid(old, new, part, parts) {
    var from = getenv("AWK_GRID_FROM");
    var first = from == nil || from == "" ? 0 : int(from);      /* skip templates */
    var g = grid();
    for (var k = 0; k < len(g); k++) {
        var name = g[k][0], prog = g[k][1];
        if (k % parts != part || int(substr(name, 1, index(name, ".") - 1)) < first)
            continue;
        var how = k % 4 ? "arg" : "file";
        compare(sprintf("grid %s [%s]", name, how), run_awk(old, prog, how, true),
                run_awk(new, prog, how, true), prog);
    }
}

/* the hand-written corner cases, in three locales */
func cmd_edge(old, new, dump) {
    var locpath = getenv("AWK_TEST_LOCPATH");
    var locs = [["C", {}]];
    var u8 = utf8_locale();
    if (u8 != "")
        push(locs, [u8, {}]);
    push(locs, ["en_US.ISO-8859-1", {LOCPATH: locpath == nil ? "" : locpath}]);
    for (var i = 0; i < len(EDGE); i++) {
        var prog = EDGE[i];
        for (var l : locs) {
            var loc = l[0], extra = l[1];
            if (startswith(loc, "en_US") && extra.LOCPATH == "")
                continue;
            for (var how : ["arg", "file"]) {
                if (how == "arg" && "\0" in prog)
                    continue;       /* an argument cannot hold a NUL */
                EXTRA = extra;
                setenv("AWK_TEST_LC", loc);
                var a = run_awk(old, prog, how, dump);
                var b = run_awk(new, prog, how, dump);
                EXTRA = {};
                compare(sprintf("edge %d %s [%s]", i, loc, how), a, b, prog);
            }
        }
    }
}

func cmd_run(old, new, seed, n) {
    srand(seed);
    for (var k = 0; k < n; k++) {
        var toks = program(rand(1, 3));
        var prog = render(toks);
        compare(sprintf("run %d/%d", seed, k), run_awk(old, prog, "arg", false, 3),
                run_awk(new, prog, "arg", false, 3), prog, [old, prog, "arg", false, 3]);
    }
}

var old = abspath(argv[0]), new = abspath(argv[1]), what = argv[2];
var rest = apply(slice(argv, 3), int);
if (what == "cases")
    cmd_cases(old, new);
else if (what == "ast")
    cmd_ast(old, new, rest[0], rest[1], false);
else if (what == "mut")
    cmd_ast(old, new, rest[0], rest[1], true);
else if (what == "grid")
    cmd_grid(old, new, rest[0], rest[1]);
else if (what == "edge")
    cmd_edge(old, new, true);
else if (what == "edge-run")
    cmd_edge(old, new, false);
else if (what == "run")
    cmd_run(old, new, rest[0], rest[1]);
summary(what);
rmtree(WORK);
exit(n_diffs ? 1 : 0);
