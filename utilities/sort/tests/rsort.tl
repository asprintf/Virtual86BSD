/*
 * rsort.tl - random differential tests of sort, compared with GNU sort (as
 * "sort" in PATH): random lines of fields, random options and -k keys.
 *
 * usage: tl rsort.tl SORT SEED N [LOCALE [ascii]]
 *
 * LOCALE UTF8 is a UTF-8 locale (as testlib finds one); without one the run
 * is skipped.
 *
 * Also: -c accepts GNU sort's output, -C agrees with GNU sort -C, and -m of
 * two sorted halves agrees with GNU sort -m.  Combinations the standard
 * leaves undefined (-d or -i with -n, -d with -i) are skipped.  GNU sort
 * counts key positions in bytes and folds case byte by byte, so in a UTF-8
 * locale give "ascii" to keep the input ASCII.
 */
include "../../testlib/testlib.tl";

if (len(argv) < 3) {
    print("usage: rsort.tl SORT SEED N [LOCALE [ascii]]");
    exit(2);
}
var S = abspath(argv[0]);
var seed = int(argv[1]), N = int(argv[2]);
var loc = len(argv) > 3 ? argv[3] : "C";
if (loc == "UTF8") {
    loc = utf8_locale();
    if (loc == "") {
        print("no UTF-8 locale: skipped");
        print("0 passed, 0 failed");
        exit(0);
    }
}
var W = mkdtemp("rsort.");
srand(seed);
var words = ["a", "B", "b", "10", "9", "-3", "0", "-0", "00", "1.5", "1.50", " x", "\303\251", "\303\211",
             "e", "a-b", "x:y", "", "  ", "Z", "zz", ".5", "-.5", "3e2", "\tq"];
if (len(argv) > 4)
    words = filter(words, func (w) { return !non_ascii(w); });

/* k distinct letters of s, in random order */
func sample(s, k) {
    var l = [];
    for (var c : s)
        push(l, chr(c));
    shuffle(l);
    return join(slice(l, 0, k), "");
}

func line() {
    var sep = choice([" ", "  ", ":", "\t", " "]);
    var f = [];
    for (var n = rand(0, 4); n > 0; n--)
        push(f, choice(words));
    return join(f, sep);
}

func keydef() {
    var f = rand(1, 4);
    var s = str(f);
    if (frand() < 0.3)
        s += "." + str(rand(1, 3));
    if (frand() < 0.4)
        s += sample("bdfinr", rand(1, 2));
    if (frand() < 0.6) {
        s += "," + str(rand(f, 5));
        if (frand() < 0.3)
            s += "." + str(rand(0, 3));
        if (frand() < 0.3)
            s += choice(["b", "n", "r"]);
    }
    return s;
}

func sortrun(cmd, input, cwd) {
    return run(cmd, {in: input, cwd: cwd, env: {LC_ALL: loc}});
}

var bad = 0;
for (var i = 0; i < N; i++) {
    var data = "";
    for (var n = rand(0, 30); n > 0; n--)
        data += line() + "\n";
    var opts = [];
    if (frand() < 0.5)
        push(opts, "-" + sample("bdfinru", rand(1, 2)));
    if (frand() < 0.4)
        push(opts, "-t", choice([":", " ", "\t"]));
    for (var n = choice([0, 0, 1, 2, 3]); n > 0; n--)
        push(opts, "-k", keydef());
    /* -d/-i with -n, and -d with -i: undefined; skip those */
    var fl = join(filter(opts, func (o) { return !(o in ["-t", ":", " ", "\t"]); }), "");
    if (("d" in fl && "i" in fl) || ("n" in fl && ("d" in fl || "i" in fl)))
        continue;
    var r = sortrun([S] + opts, data, nil);
    var g = sortrun(["sort"] + opts, data, nil);
    if (r.out != g.out || r.status != g.status) {
        bad++;
        if (bad <= 5) {
            printf("== opts %q rc %d %d\n%q\n", opts, r.status, g.status, data);
            printf("ours %q\ngnu  %q\n%s\n", lines(r.out), lines(g.out), substr(r.err, 0, 200));
        }
    }
    if (g.status != 0)
        continue;
    /* -c on GNU-sorted output, -C on the unsorted */
    if (sortrun([S, "-c"] + opts, g.out, nil).status != 0) {
        bad++;
        printf("CHECK fails on sorted %q\n", opts);
    }
    var c1 = sortrun([S, "-C"] + opts, data, nil), c2 = sortrun(["sort", "-C"] + opts, data, nil);
    if (c1.status != c2.status) {
        bad++;
        printf("-C differs %q %d %d\n", opts, c1.status, c2.status);
    }
    /* merge: the sorted output split in two sorted halves (not with -u) */
    var a = "", b = "", k = 0;
    for (var l : slice(split(g.out, "\n"), 0, -1)) {
        if (k++ % 2 == 0)
            a += l + "\n";
        else
            b += l + "\n";
    }
    writefile(W + "/m1", a);
    writefile(W + "/m2", b);
    var first = len(opts) && !startswith(opts[0], "-t") && !startswith(opts[0], "-k") ? opts[0] : "";
    if (!("-u" in join(opts, "")) && !("u" in first)) {
        var m = sortrun([S, "-m"] + opts + ["m1", "m2"], "", W);
        var gm = sortrun(["sort", "-m"] + opts + ["m1", "m2"], "", W);
        if (m.out != gm.out) {
            bad++;
            printf("MERGE differs %q\n", opts);
        }
    }
}
rmtree(W);
printf("%s %d cases, %d differ\n", loc, N, bad);
printf("%d passed, %d failed\n", N - bad, bad);
exit(bad ? 1 : 0);
