/*
 * rdiff.tl - random differential tests of diff: pairs of files of a few
 * kinds of lines, compared with GNU diff (as "diff" in PATH), with and
 * without -b.
 *
 * usage: tl rdiff.tl DIFF SEED N
 *
 * For each pair: the edit script must be no longer than GNU diff's (both
 * should be minimal); the default and -u output, given to patch, and the -e
 * output with "w" appended, given to ed, must make file2 of file1.  The
 * output itself may differ from GNU diff's where several minimal scripts
 * exist; how often it is the same is reported.  Exit status 1 if a check
 * failed.
 */

if (len(argv) != 3) {
    print("usage: rdiff.tl DIFF SEED N");
    exit(2);
}
var D = abspath(argv[0]);
var W = mkdtemp("rdiff.");
var seed = int(argv[1]), N = int(argv[2]);
srand(seed);

func gen() {
    var alpha = [];
    for (var k = rand(1, 6); k > 0; k--)
        push(alpha, choice(["a", "b", "c", "d", ".", "", "x y", "x  y ", " ", "e\tf"]));
    var lines = [];
    for (var n = rand(0, 40); n > 0; n--)
        push(lines, choice(alpha));
    return lines;
}

func mutate(lines) {
    var l = copy(lines);
    for (var k = rand(0, 8); k > 0; k--) {
        var op = frand();
        if (op < 0.3 && len(l))
            pop(l, rand(len(l)));
        else if (op < 0.6)
            insert(l, rand(0, len(l)), choice(["a", "b", "z", ".", "q r", ""]));
        else if (len(l))
            l[rand(len(l))] = choice(["a", "Z", ".", "x y", ""]);
    }
    return l;
}

func text(lines, nl) {
    var s = join(lines, "\n");
    if (len(lines) && nl)
        s += "\n";
    return s;
}

/* the number of lines inserted or deleted in a normal-format script */
func count(out) {
    var n = 0;
    for (var l : lines(out))
        if (startswith(l, "< ") || startswith(l, "> ") || l == "<" || l == ">")
            n++;
    return n;
}

/* a list of options as Python writes it */
func plist(l) {
    return "[" + join(apply(l, func (o) { return "'" + o + "'"; }), ", ") + "]";
}

/* does patch (or ed) applied to a copy of a make b? */
func applies(prog, script, copyname) {
    run(["cp", "a", copyname], {cwd: W});
    if (prog == "ed")
        run(["ed", "-s", copyname], {cwd: W, in: script + "w\nq\n"});
    else {
        writefile(W + "/p", script);
        run(["patch", "-s", copyname, "p"], {cwd: W});
    }
    return readfile(W + "/" + copyname) == readfile(W + "/b");
}

var st = {same: 0, cases: 0, longer: 0, ed_bad: 0, patch_bad: 0, upatch_bad: 0, status: 0};
for (var i = 0; i < N; i++) {
    var A = gen();
    var B = frand() < 0.8 ? mutate(A) : gen();
    var nla = frand() < 0.9, nlb = frand() < 0.9;
    writefile(W + "/a", text(A, nla));
    writefile(W + "/b", text(B, nlb));
    for (var opts : [[], ["-b"]]) {
        var r = run([D] + opts + ["a", "b"], {cwd: W});
        var g = run(["diff"] + opts + ["a", "b"], {cwd: W});
        st.cases++;
        if (r.status != g.status) {
            st.status++;
            print("STATUS", seed, i, plist(opts), r.status, g.status);
        }
        if (r.out == g.out)
            st.same++;
        if (count(r.out) > count(g.out)) {
            st.longer++;
            print("LONGER", seed, i, plist(opts));
        }
        if (len(opts) || r.status != 1)
            continue;
        /* normal format via patch */
        if (!applies("patch", r.out, "pa")) {
            st.patch_bad++;
            print("PATCH", seed, i);
        }
        if (!applies("patch", run([D, "-u", "a", "b"], {cwd: W}).out, "ua")) {
            st.upatch_bad++;
            print("UPATCH", seed, i);
        }
        if (nla && nlb && !applies("ed", run([D, "-e", "a", "b"], {cwd: W}).out, "ea")) {
            st.ed_bad++;
            print("ED", seed, i);
        }
    }
}
rmtree(W);
printf("{'same': %d, 'cases': %d, 'longer': %d, 'ed_bad': %d, 'patch_bad': %d, 'upatch_bad': %d, 'status': %d}\n",
       st.same, st.cases, st.longer, st.ed_bad, st.patch_bad, st.upatch_bad, st.status);
var fails = st.longer + st.ed_bad + st.patch_bad + st.upatch_bad + st.status;
printf("%d passed, %d failed\n", st.cases - fails, fails);
exit(fails ? 1 : 0);
