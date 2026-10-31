/*
 * difftest.tl - differential tests of lex against flex -X (POSIX mode).
 *
 * usage: tl difftest.tl LEX [NAME...]
 *
 * Each case in cases.tl is a lex source and a list of inputs; both
 * scanners are compiled and run on each input, and the outputs (stdout
 * and the exit status) must be the same.  Cases with a differs reason
 * are known differences: they are run, and reported only if they stop
 * differing.  $SCANFLAGS are added to the compiler options.
 */
include "../../testlib/testlib.tl";
include "cases.tl";

var lex = abspath(argv[0]);
var libdir = dirname(lex);
var names = slice(argv, 1);
var ccenv = cc_env();
var scanflags = getenv("SCANFLAGS");

/* runs CMD on t.l in d and compiles lex.yy.c into d/exe: [path or nil, messages] */
func build(cmd, d, exe, src) {
    writefile(d + "/t.l", src);
    var r = run(cmd + ["t.l"], {cwd: d});
    if (r.status)
        return [nil, r.err];
    var c = run(ccenv.CC + " " + ccenv.CQUIET + " " + (scanflags == nil ? "" : scanflags) + " -o " + exe +
                " lex.yy.c -L" + shquote(libdir) + " -ll", {cwd: d});
    if (c.status)
        return [nil, c.err];
    return [d + "/" + exe, r.err];
}

func scan(exe, inp, args) {
    var r = run([exe] + args, {in: inp, timeout: 10});
    if (r.timedout)
        return "TIMEOUT";
    return r.out + (r.status == 0 ? "" : sprintf("[status %d]", r.status));
}

var ok = 0, bad = 0, known = 0;
for (var c : CASES) {
    if (len(names) && !(c.name in names))
        continue;
    var d = mkdtemp("lexdiff.");
    var m = build([lex], d, "mine", c.src), f = build(["flex", "-X"], d, "ref", c.src);
    var mine = m[0], ref = f[0];
    if (mine && !ref && c.differs) {
        known++;
        rmtree(d);
        continue;
    }
    if (!mine || !ref) {
        print("BUILD", c.name, !mine ? "mine:" : "", !mine ? m[1] : "", !ref ? "flex:" : "", !ref ? f[1] : "");
        bad++;
        rmtree(d);
        continue;
    }
    var args = get(c, "args", []);
    var diffs = [];
    for (var inp : c.inputs) {
        var a = scan(mine, inp, args), b = scan(ref, inp, args);
        if (a != b)
            push(diffs, [inp, a, b]);
    }
    if (c.differs) {
        if (len(diffs))
            known++;
        else {
            print("NOW SAME", c.name, "(listed as differing:", c.differs + ")");
            bad++;
        }
    } else if (len(diffs)) {
        bad++;
        print("DIFF", c.name);
        for (var x : slice(diffs, 0, 3))
            printf("  input %q\n   mine: %q\n   flex: %q\n", x[0], substr(x[1], 0, 400), substr(x[2], 0, 400));
    } else
        ok++;
    rmtree(d);
}
printf("%d same, %d known differences, %d failures\n", ok, known, bad);
exit(bad ? 1 : 0);
