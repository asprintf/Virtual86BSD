/*
 * ardiff.tl - ar compared with GNU ar
 *
 *     tl ardiff.tl AR [SEED [RUNS]]
 *
 * Random sequences of operations (-r with -a, -b, -u, -v; -q; -d; -m with
 * -a and -b; -t, -tv, -p, -pv, -x) on files of random contents and names,
 * some long, and on object files made by the C compiler, are done by this
 * ar and by GNU ar (with its U modifier: real dates, owners and modes);
 * the archives must be the same, byte for byte, but for the date of the
 * symbol table, and so must the output and the exit status.  -d, -m, -t,
 * -p and -x are given names of members that are there (GNU ar is content
 * with others, this one reports them and exits with status 1); -a and -b
 * come with one file that is not in the archive (GNU ar moves the files
 * it replaces, which the standard does not allow), and so does -m with a
 * position, with one file other than the posname (GNU ar puts several in
 * reverse order).  The blanks of -tv are squeezed (GNU ar pads the size);
 * with -u the output is not compared (GNU ar does not replace a file as
 * old as its member, the standard wants it replaced); when both fail,
 * only the archives are compared.
 */
include "../../../lib/testlib/testlib.tl";

var GNU = nil;
for (var d : ["/usr/bin", "/usr/local/bin", "/bin"])
    if (GNU == nil && isfile(d + "/ar") && access(d + "/ar", "x"))
        GNU = d + "/ar";
var CC = getenv("CC");
if (CC == nil || CC == "")
    CC = "cc";
var ENV = {TZ: "UTC0", LC_ALL: "C"};

func ar(cmd, cwd) {
    var r = run(cmd, {cwd: cwd, env: ENV});
    return [r.status, r.out];
}

/* the archive with the date of its symbol table blanked */
func norm(d) {
    var h = substr(d, 8, 16);
    if (h == "/               " || h == "/SYM64/         ")
        d = substr(d, 0, 24) + repeat(" ", 12) + substr(d, 36);
    return d;
}

/* k different elements of l, at random */
func sample(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

/* the members of lib.a in dir, sorted, without repetitions */
func members(dir) {
    var r = ar([GNU, "t", "lib.a"], dir);
    var seen = {};
    if (r[0] == 0)
        for (var l : split(r[1], "\n"))
            if (l != "")
                seen[l] = 1;
    return sorted(keys(seen));
}

var ours = abspath(argv[0]);
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 100;
if (GNU == nil) {
    print("ardiff: no GNU ar");
    exit(2);
}
srand(seed);
var fails = 0;
var tmp = mkdtemp("ardiff.");
mkdir(tmp + "/src");
var names = ["a", "b.txt", "c.o", "longer_name_than_fifteen.txt", "another_long_member_name", "x y", "d.o",
             "e", "obj1.o", "obj2.o"];
/* object files */
var objs = {};
var onames = ["c.o", "d.o", "obj1.o", "obj2.o"];
for (var k = 0; k < len(onames); k++) {
    var c = sprintf("%s/o%d.c", tmp, k);
    writefile(c, sprintf("int g%d = %d; static int s%d; int fn%d(void) { return g%d + s%d; } int com%d;\n",
                         k, k, k, k, k, k, k));
    var o = sprintf("%s/o%d.o", tmp, k);
    if (run([CC, "-c", "-o", o, c], {out: "inherit", err: "inherit"}).status == 0)
        objs[onames[k]] = readfile(o);
}
var pos = nil;
for (var i = 0; i < runs; i++) {
    for (var who : ["ours", "gnu"]) {
        rmtree(tmp + "/" + who);
        mkdir(tmp + "/" + who);
    }
    var log = [];
    var bad = nil;
    for (var step = rand(1, 8); step > 0; step--) {
        /* new contents for some files */
        for (var n : sample(names, rand(1, 4))) {
            var data = haskey(objs, n) && frand() < 0.8 ? objs[n] : randbytes(rand(0, 40));
            var mtime = rand(1000000000, 1800000000);
            var mode = choice([0644, 0600, 0755, 0640]);
            for (var who : ["ours", "gnu"]) {
                var p = tmp + "/" + who + "/" + n;
                writefile(p, data);
                chmod(p, mode);
                utime(p, mtime, mtime);
            }
        }
        var op = choice(["r", "r", "r", "q", "d", "m", "t", "p", "x"]);
        var files = sample(names, rand(1, 3));
        if (op in "dmtpx") {
            /* members there (GNU ar is content with others, this one is not) */
            var there = members(tmp + "/gnu");
            if (!len(there))
                op = "r";
            else {
                files = sample(there, min(len(there), rand(1, 3)));
                if (op == "m") {
                    /* one file, not the posname (GNU ar puts several in
                       reverse order after the posname) */
                    var f1 = filter(files, func (f) { return f != pos; });
                    if (!len(f1))
                        f1 = filter(there, func (f) { return f != pos; });
                    files = slice(f1, 0, 1);
                    if (!len(files)) {
                        op = "r";
                        files = slice(there, 0, 1);
                    }
                }
            }
        }
        pos = choice(names);
        var o_args, g_args, mods;
        if (op == "r") {
            mods = choice(["", "u", "v", "uv"]);
            o_args = ["-rc" + mods];
            g_args = ["rc" + mods + "U"];
        } else if (op == "q") {
            o_args = ["-qc"];
            g_args = ["qcU"];
        } else if (op == "d") {
            o_args = ["-dv"];
            g_args = ["dvU"];
        } else if (op == "m") {
            mods = choice(["", "a", "b"]);
            if (mods != "") {
                o_args = ["-m" + mods, pos];
                g_args = ["m" + mods + "U", pos];
            } else {
                o_args = ["-m"];
                g_args = ["mU"];
            }
        } else if (op == "t" || op == "p") {
            mods = choice(["", "v"]);
            o_args = ["-" + op + mods];
            g_args = [op + mods];
            files = choice([[], files]);
        } else {
            o_args = ["-xv"];
            g_args = ["xv"];
            files = choice([[], files]);
        }
        if (op == "r" && frand() < 0.3) {
            /* a position, for one file that is not there (GNU ar moves
               the files it replaces to the position, which the standard
               does not allow, and puts several in another order) */
            var there = members(tmp + "/gnu");
            var new = [];
            for (var n : names)
                if (!(n in there) && n != pos)
                    push(new, n);
            if (len(new)) {
                files = [choice(new)];
                mods = choice(["a", "b"]);
                o_args = ["-r", "-c", "-" + mods, pos];
                g_args = ["rc" + mods + "U", pos];
            }
        }
        var res = {};
        for (var w : [["ours", ours, o_args], ["gnu", GNU, g_args]]) {
            var d = tmp + "/" + w[0];
            var args = w[2];
            var r = ar([w[1]] + args + ["lib.a"] + files, d);
            var arch = readfile(d + "/lib.a");
            var out = r[1];
            if ("u" in args[0])
                out = "";       /* equal times: GNU ar does not replace, the standard does */
            if ("t" in args[0] && "v" in args[0])
                out = join(apply(split(out, "\n"), func (l) { return join(split(l), " "); }), "\n");
            res[w[0]] = [r[0] == 0, out, arch != nil && arch != "" ? norm(arch) : nil];
        }
        if (!res.ours[0] && !res.gnu[0]) {
            res.ours = [false, "", res.ours[2]];
            res.gnu = [false, "", res.gnu[2]];
        }
        push(log, join(o_args + ["lib.a"] + files, " "));
        /* -x gives the time of the extraction: the same for both */
        for (var n : names) {
            var g = tmp + "/gnu/" + n, o = tmp + "/ours/" + n;
            if (exists(g) && exists(o)) {
                var st = stat(g);
                utime(o, st.atime, st.mtime);
            }
        }
        if (res.ours != res.gnu) {
            bad = res;
            break;
        }
    }
    if (bad != nil) {
        fails++;
        if (fails <= 5) {
            printf("FAIL run %d: %s\n", i, join(log, " ; "));
            for (var who : ["ours", "gnu"]) {
                var b = bad[who];
                printf("  %s: ok %s, output %q, archive %s\n", who, b[0] ? "True" : "False",
                       slice(b[1], 0, 200), b[2] != nil ? str(len(b[2])) : "None");
            }
        }
    }
}
rmtree(tmp);
printf("ardiff: %d compared, %d failed\n", runs, fails);
exit(fails ? 1 : 0);
