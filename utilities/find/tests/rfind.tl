/*
 * rfind.tl - random differential tests of find, compared with GNU find (as
 * "find" in PATH): random trees (directories, files of a few sizes and
 * modes, symbolic links, FIFOs) and random expressions, with -H, -L or
 * neither.
 *
 * usage: tl rfind.tl FIND SEED N
 *
 * The sorted outputs and whether the exit status is zero must agree.  With
 * -L, an entry that GNU find reports as a directory loop and leaves out is
 * allowed to be listed (this find lists it, and reports the loop without
 * entering it).  GNU find's expression optimizer can move -name before
 * -print ("-print ... -name x -o ..."), which changes its output; run GNU
 * find with -O0 to compare such cases.
 */

if (len(argv) != 3) {
    print("usage: rfind.tl FIND SEED N");
    exit(2);
}
var F = abspath(argv[0]);
var seed = int(argv[1]), N = int(argv[2]);
srand(seed);
var W = mkdtemp("rfind.");

func tree() {
    rmtree(W);
    mkdirs(W + "/t");
    var dirs = ["t"];
    for (var i = 0, n = rand(3, 25); i < n; i++) {
        var d = choice(dirs);
        var p = d + "/" + choice(["a", "b", "c", "ab", "x.c", ".h", "Ab", "dd"]) + str(i % 3);
        var w = W + "/" + p;
        var k = frand();
        var dl = lstat(W + "/" + d);
        if (lstat(w) != nil || dl == nil || dl.type != "d")
            continue;
        if (k < 0.3) {
            mkdir(w);
            push(dirs, p);
        } else if (stat(w) != nil)
            continue;
        else if (k < 0.8) {
            writefile(w, "x" * choice([0, 1, 511, 512, 513, 2000]));
            chmod(w, choice([0644, 0755, 0600, 04755, 0640]));
        } else if (k < 0.9)
            symlink(choice(["a0", "../t", "nowhere", "."]), w);
        else
            mkfifo(w);
    }
}

var prims = ["-type f", "-type d", "-type l", "-type p", "-name a*", "-name *[0-9]", "-iname A*",
             "-path t/a*", "-path */b*", "-perm 644", "-perm -100", "-perm -u=x", "-perm g=r,u=rw",
             "-links 1", "-links +1", "-size 0", "-size +1", "-size -2", "-size 512c", "-size +512c",
             "-prune", "-print", "-depth", "-mtime 0", "-newer t", "-nouser", "-xdev"];

func expr(d) {
    var k = frand();
    if (d > 2 || k < 0.4)
        return split(choice(prims), " ");
    if (k < 0.55)
        return ["!"] + expr(d + 1);
    if (k < 0.7)
        return ["("] + expr(d + 1) + [")"];
    if (k < 0.85)
        return expr(d + 1) + ["-o"] + expr(d + 1);
    return expr(d + 1) + expr(d + 1);
}

/* the set of the lines of s */
func lineset(s) {
    var m = {};
    for (var l : split(s, "\n"))
        m[l] = 1;
    return m;
}

var bad = 0;
for (var i = 0; i < N; i++) {
    if (i % 10 == 0)
        tree();
    var cmd = choice([[], ["-H"], ["-L"]]) + ["t"] + expr(0);
    var r = run([F] + cmd, {cwd: W});
    var g = run(["find", "-O0"] + cmd, {cwd: W});
    var rs = lineset(r.out), gs = lineset(g.out);
    var err = replace(replace(g.err, "\342\200\230", "'"), "\342\200\231", "'");
    /* the lines in one output only, and whether all are loops GNU find reported */
    var diff = [], loops = true;
    for (var l : keys(rs))
        if (!haskey(gs, l))
            push(diff, l);
    for (var l : keys(gs))
        if (!haskey(rs, l))
            push(diff, l);
    for (var l : diff)
        if (!("'" + l + "'" in err))
            loops = false;
    if ((len(diff) && !loops) || (r.status != 0) != (g.status != 0)) {
        bad++;
        if (bad <= 6) {
            printf("== %q %d %d\n", cmd, r.status, g.status);
            printf("%q\n%s\n", sorted(diff), substr(g.err, 0, 200));
        }
    }
}
rmtree(W);
printf("%d passed, %d failed\n", N - bad, bad);
exit(bad ? 1 : 0);
