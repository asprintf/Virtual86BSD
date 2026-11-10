/*
 * cases.tl - the regression suites of case scripts, for the tl runners
 * (include "../../testlib/cases.tl").  Each case, DIR/cases/NAME.sh, runs
 * in a new empty directory; its output is compared with
 * DIR/expected/NAME.out (written instead with --freeze).
 *
 *   run_cases(opts): runs the cases; the exit status (0 if all passed).
 *   opts:
 *     dir      the directory of cases/ and expected/
 *     args     the runner's arguments: [--freeze] [NAME...]
 *     env      a map of the variables set (nil: unset)
 *     form     "sh": sh CASE; the output, then the error output, then
 *              "exit N" (the default); "dot": sh -c 'umask 022; set -u;
 *              . CASE', the outputs together
 *     home     true: HOME is the case's directory
 *     dirvars  a map of variables set to files in the case's directory
 *              (the names relative to it)
 *     subst    a list of [string, replacement] made in the output; with
 *              "dot", the case's directory is $TMP
 *     timeout  seconds (300)
 *     verbose  "ok   NAME" for each case that passed
 *   The output: "FAIL NAME" with the end of the output, or the
 *   differences, for each case that failed; last "N passed, M failed".
 *   A case whose whole output is "SKIP" is not counted.
 */

include "testlib.tl";

func run_cases(opts) {
    var dir = opts.dir, args = opts.args == nil ? [] : opts.args;
    var freeze = "--freeze" in args;
    var form = opts.form == nil ? "sh" : opts.form;
    var timeout = opts.timeout == nil ? 300 : opts.timeout;
    var names = filter(args, func (a) { return !startswith(a, "--"); });
    var cases = [];
    for (var f : listdir(dir + "/cases"))
        if (endswith(f, ".sh") && (!len(names) || substr(f, 0, len(f) - 3) in names))
            push(cases, substr(f, 0, len(f) - 3));
    var fails = 0, skipped = 0;
    for (var c : cases) {
        var d = mkdtemp("cases.");
        var env = opts.env == nil ? {} : copy(opts.env);
        if (opts.home)
            env.HOME = d;
        if (opts.dirvars != nil)
            for (var k : opts.dirvars)
                env[k] = d + "/" + opts.dirvars[k];
        var r, out;
        if (form == "dot") {
            r = run(["sh", "-c", "umask 022; set -u; . \"$1\"", "case", dir + "/cases/" + c + ".sh"],
                    {cwd: d, env: env, err: "out", stdin: "null", timeout: timeout});
            out = replace(r.out, d, "$TMP");
        } else {
            r = run(["sh", dir + "/cases/" + c + ".sh"], {cwd: d, env: env, stdin: "null", timeout: timeout});
            out = r.out + r.err + sprintf("exit %d\n", r.signal ? -r.signal : r.status);
        }
        if (r.timedout)
            out = "TIMEOUT\n";
        if (opts.subst != nil)
            for (var p : opts.subst)
                out = replace(out, p[0], p[1]);
        run(["chmod", "-R", "u+rwx", d], {err: "null"});
        rmtree(d);
        if (out == "SKIP\n") {
            skipped++;
            continue;
        }
        var exp = dir + "/expected/" + c + ".out";
        if (freeze) {
            writefile(exp, out);
            printf("froze %s\n", c);
            continue;
        }
        var want = readfile(exp);
        if (want == out) {
            if (opts.verbose)
                printf("ok   %s\n", c);
            continue;
        }
        fails++;
        printf("FAIL %s\n", c);
        if (want == nil || form != "dot")
            puts(len(out) > 3000 ? slice(out, -3000) : out);
        else {
            var t = mkdtemp("casesdiff.");
            writefile(t + "/expected", want);
            writefile(t + "/got", out);
            for (var l : lines(run(["diff", "-U", "2", t + "/expected", t + "/got"]).out))
                printf("    %s\n", l);
            rmtree(t);
        }
    }
    if (!freeze)
        printf("%d passed, %d failed\n", len(cases) - fails - skipped, fails);
    return fails ? 1 : 0;
}
