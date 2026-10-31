/*
 * difftest.tl - differential tests of bc against GNU bc.
 *
 * usage: tl difftest.tl BC [NAME...]
 *
 * Each case in cases.tl is a program for the standard input, with
 * options, files and environment variables; the standard output must be
 * the same, and both must write to standard error or neither.  Cases
 * with a 'differs' reason are known differences.
 */
include "cases.tl";

if (!len(argv)) {
    eprintf("usage: tl difftest.tl BC [NAME...]\n");
    exit(2);
}
var bc = abspath(argv[0]);
var names = slice(argv, 1);

/* [standard output, whether anything was written to standard error] */
func runbc(cmd, cs, d) {
    var env = cs.env == nil ? {} : copy(cs.env);
    env.BC_TEST_GNU_SQRT = "1";
    var args = [cmd] + (cs.args == nil ? [] : cs.args);
    if (cs.files != nil)
        args += keys(cs.files);
    var r = run(args, {in: cs.prog, cwd: d, env: env, timeout: 60});
    return [r.out, trim(r.err) != ""];
}

var ok = 0, bad = 0, known = 0;
for (var cs : CASES) {
    if (len(names) && !(cs.name in names))
        continue;
    var d = mkdtemp("bcdiff.");
    if (cs.files != nil)
        for (var f : cs.files)
            writefile(d + "/" + f, cs.files[f]);
    var ra = runbc(bc, cs, d), rb = runbc("bc", cs, d);
    var a = ra[0], ea = ra[1], b = rb[0], eb = rb[1];
    var same = a == b && ea == eb;
    if (cs.differs) {
        if (same) {
            print("NOW SAME", cs.name);
            bad++;
        } else
            known++;
    } else if (same)
        ok++;
    else {
        bad++;
        printf("DIFF %s (stderr: mine %s, gnu %s)\n", cs.name, ea ? "True" : "False", eb ? "True" : "False");
        var la = split(a, "\n"), lb = split(b, "\n");
        for (var i = 0; i < max(len(la), len(lb)); i++) {
            var x = i < len(la) ? la[i] : "<none>";
            var y = i < len(lb) ? lb[i] : "<none>";
            if (x != y) {
                printf("  line %d\n   mine: %q\n   gnu:  %q\n", i + 1, substr(x, 0, 200), substr(y, 0, 200));
                break;
            }
        }
    }
    rmtree(d);
}
printf("%d same, %d known differences, %d failures\n", ok, known, bad);
exit(bad ? 1 : 0);
