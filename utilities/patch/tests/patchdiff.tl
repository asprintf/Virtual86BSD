/*
 * patchdiff.tl - compare patch with GNU patch, and with diff.
 *
 *     tl tests/patchdiff.tl ./patch
 *
 * Random pairs of files are compared with diff in every output format
 * (normal, -c, -u, -e, with 0, 1 and 3 lines of context), and the
 * differences applied: to the old file (the result must be the new file,
 * and -R must bring the old one back), and to versions of the old file
 * with lines added at the top and context lines changed, where the
 * result and whether hunks were rejected must agree with GNU patch
 * (run with --posix).  Needs diff, and GNU patch as "patch" in PATH.
 * With PATCHDIFF_KEEP set, the patch and the old file of each
 * disagreement are kept in that directory.
 */
include "../../testlib/testlib.tl";

func write(name, lines, eol) {
    writefile(name, join(lines, "\n") + (eol && len(lines) ? "\n" : ""));
}

func cp(from, to) {
    writefile(to, readfile(from));
}

var prog = abspath(argv[0]);
if (!("GNU patch" in run(["patch", "--version"], {err: "null"}).out)) {
    print("patchdiff: GNU patch is needed");
    exit(2);
}
var keep = getenv("PATCHDIFF_KEEP");
if (keep != nil && keep != "")
    keep = abspath(keep);
var tmp = mkdtemp("patchdiff.");
chdir(tmp);
srand(2024);

for (var it = 0; it < 250; it++) {
    var n = rand(0, 60);
    var a = [];
    for (var i = 0; i < n; i++)
        push(a, sprintf("line %d", rand(0, 20)));
    var b = copy(a);
    for (var k = rand(1, 6); k > 0; k--) {
        var op = frand();
        if (op < 0.3 && len(b))
            pop(b, rand(len(b)));
        else if (op < 0.6)
            insert(b, rand(0, len(b)), sprintf("new %d", rand(0, 99)));
        else if (len(b))
            b[rand(len(b))] = sprintf("chg %d", rand(0, 99));
    }
    var ea = frand() > 0.1, eb = frand() > 0.1;
    write("a", a, ea);
    write("b", b, eb);
    for (var fmt : ["", "-c", "-u", "-e", "-C1", "-U1", "-C0", "-U0"]) {
        if (fmt == "-e" && !(ea && eb))
            continue;
        var d = run(["diff"] + (fmt != "" ? [fmt] : []) + ["a", "b"]).out;
        if (d == "")
            continue;
        writefile("p", d);
        var name = sprintf("%d diff %s", it, fmt != "" ? fmt : "(normal)");
        cp("a", "t");
        var r = run([prog, "-i", "p", "t"]);
        check(name + ": the new file", r.status == 0 && readfile("t") == readfile("b"));
        if (fmt != "-e") {
            r = run([prog, "-R", "-i", "p", "t"]);
            check(name + ": -R: the old file", r.status == 0 && readfile("t") == readfile("a"));
        }
        if (!(fmt in ["-c", "-u", "-C1", "-U1"]) || !len(a))
            continue;
        /* a changed old file: lines at the top, a context line changed */
        var t = ["top"] * choice([0, 2, 7]) + a;
        if (frand() < 0.5 && len(t) > 2)
            t[rand(len(t))] = "changed context";
        write("t1", t, ea);
        cp("t1", "t2");
        var r1 = run([prog, "-i", "p", "t1"]);
        var r2 = run(["patch", "--posix", "-s", "-f", "-i", "p", "t2"]);
        var same = r1.status == r2.status && readfile("t1") == readfile("t2");
        check(sprintf("%s, changed: the same as GNU patch (status %d and %d)", name, r1.status, r2.status), same);
        if (!same && keep != nil && keep != "") {
            mkdirs(keep);
            var kp = sprintf("%s/%d%s", keep, it, fmt);
            cp("p", kp + ".patch");
            write(kp + ".old", t, ea);
        }
        for (var x : listdir("."))
            if (endswith(x, ".rej") || endswith(x, ".orig"))
                remove(x);
    }
}
chdir("/");
rmtree(tmp);
exit(tests_done("patchdiff"));
