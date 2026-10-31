/*
 * csscdiff.tl - compare the SCCS utilities with CSSC.
 *
 *     tl tests/csscdiff.tl DIR [SEED [RUNS]]
 *
 * DIR holds the programs (admin, delta, get, ... as links to sccs).  Random
 * histories are made twice, with these utilities and with CSSC (its
 * programs in $CSSC_DIR, by default /usr/lib/x86_64-linux-gnu/cssc), in two
 * directories: admin -i, then edits (get -e with -r and -b, changes to the
 * g-file, delta with comments and -g), rmdel of leaves, unget, flags set
 * and cleared, and between them get -p (-m, -n, -k, -i, -x, -r of every
 * kind, -l, -L), prs (-d with every data keyword, -e, -l, -a), sact and val
 * (its status only: CSSC reports on standard error).  The l-file is not
 * compared with -i or -x: CSSC leaves those deltas out of it.  delta -g is
 * not tried: CSSC does not have it.
 * The standard output, the exit status (zero or not), the g-, p- and
 * l-files, and the SCCS files up to the body (without the checksum and the
 * dates and times) must be the same; and every version must be the same,
 * by CSSC's get and by ours from either file.  (The bodies themselves may
 * differ where two shortest edit scripts are equally good: CSSC takes
 * GNU diff's.)  A step where CSSC is killed by a signal (it crashes on
 * some -x lists) is skipped.  CSSC's informative messages about missing
 * id keywords go to standard error and are not compared.  (Operands in
 * other directories, no operands, print and sact are not tried: CSSC's
 * sccs handles them otherwise than the standard says.)  The front end
 * sccs is compared with CSSC's ($CSSC_SCCS, by default /usr/bin/sccs)
 * when that is there.
 */
include "../../testlib/testlib.tl";

var DATE = "[0-9][0-9]/[0-9][0-9]/[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]";
var ENV = {LC_ALL: "C", TZ: "UTC0"};

func norm(b) {
    return gsub(b, DATE, "DATE");
}

/* the SCCS file up to the body, without the checksum and the dates */
func head(b) {
    var ls = split(b, "\n");
    var out = [];
    for (var i = 1; i < len(ls); i++) {
        push(out, ls[i]);
        if (ls[i] == "\001T")
            break;
    }
    return norm(join(out, "\n"));
}

/* the result of a program: [nil if killed by a signal, else whether it
   failed; its output] */
func prun(cmd, d, inp) {
    var r = run(cmd, {cwd: d, in: inp, env: ENV});
    return [r.signal ? nil : r.status != 0, r.out];
}

/* the SIDs of the deltas (not removed) in d/s.f, with the lines of prs */
func prs_lines(d) {
    var o = run([ours + "/prs", "-a", "-d:I: :DT:", "s.f"], {cwd: d, env: ENV}).out;
    var res = [];
    for (var l : split(o, "\n")) {
        var f = split(l);
        if (len(f) > 1 && f[1] == "D")
            push(res, l);
    }
    return res;
}

/* the text of every delta, by CSSC's get (so also checking that CSSC
   reads the file), and by ours */
func versions(d) {
    var res = [];
    for (var l : prs_lines(d))
        for (var prog : [cssc, ours]) {
            var r = run([prog + "/get", "-p", "-k", "-s", "-r" + split(l)[0], "s.f"], {cwd: d, env: ENV});
            push(res, [l, r.status, r.out]);
        }
    return res;
}

func sids() {
    return apply(prs_lines(da), func (l) { return split(l)[0]; });
}

func show(what, b) {
    printf("  %s: %s\n", what, b == nil ? "None" : sprintf("%q", b));
}

/* runs args with ours in da and with CSSC in db and compares them, and
   the files named; true if they agree */
func both(args, inp, files) {
    push(log, join(args, " "));
    var a = prun([ours + "/" + args[0]] + slice(args, 1), da, inp);
    var b = prun([cssc + "/" + args[0]] + slice(args, 1), db, inp);
    var sa = a[0], oa = a[1], sb = b[0], ob = b[1];
    if (sb == nil) {
        skipped++;      /* CSSC was killed by a signal (it crashes now and then) */
        return true;
    }
    if (args[0] == "val") {
        oa = "";        /* CSSC's val reports on standard error */
        ob = "";
    }
    var same = sa == sb && norm(oa) == norm(ob);
    var what = "output";
    if (same)
        for (var f : ["s.f"] + (files == nil ? [] : files)) {
            var pa = da + "/" + f, pb = db + "/" + f;
            var ea = exists(pa), eb = exists(pb);
            if (ea != eb) {
                same = false;
                what = f;
                break;
            }
            if (ea) {
                var ca = readfile(pa), cb = readfile(pb);
                if (f == "s.f") {
                    ca = head(ca);
                    cb = head(cb);
                    if (ca == cb) {
                        ca = versions(da);
                        cb = versions(db);
                    }
                } else {
                    ca = norm(ca);
                    cb = norm(cb);
                }
                if (ca != cb) {
                    same = false;
                    what = f;
                    break;
                }
            }
        }
    if (same)
        passed++;
    else {
        failed++;
        printf("FAIL run %d (%s): %s\n", run_no, what, join(args, " "));
        print("  history: " + join(slice(log, -8), " ; "));
        if (what == "output") {
            print("  status", sa ? "True" : "False", sb ? "True" : "False");
            show("ours", slice(oa, 0, 400));
            show("cssc", slice(ob, 0, 400));
        } else {
            var pa = readfile(da + "/" + what), pb = readfile(db + "/" + what);
            show("ours", pa == nil ? nil : slice(pa, 0, 600));
            show("cssc", pb == nil ? nil : slice(pb, 0, 600));
        }
    }
    return same;
}

/* lines that differ (a number each), so that the shortest edit scripts
   are not ambiguous */
var WORDS = ["alpha", "beta", "gamma", "delta", "x", "y", "%I%", "%M%", "%R%.%L%", "%W%",
             "%Z%", "%Y%", "%Q%", "%F%"];
func text(n) {
    var out = [];
    for (var i = 0; i < n; i++) {
        counter++;
        var w = [];
        for (var k = rand(0, 3); k > 0; k--)
            push(w, choice(WORDS));
        push(w, str(counter));
        push(out, join(w, " "));
    }
    return out;
}

func write(ls) {
    var data = len(ls) ? join(ls, "\n") + "\n" : "";
    for (var d : [da, db]) {
        var p = d + "/f";
        if (exists(p))
            chmod(p, 0644);
        writefile(p, data);
    }
}

func gfile() {
    var t = readfile(da + "/f");
    if (t == nil)
        return nil;
    var ls = split(t, "\n");
    pop(ls);
    return ls;
}

func any_prefix(args, prefixes) {
    for (var a : args)
        for (var p : prefixes)
            if (startswith(a, p))
                return true;
    return false;
}

/* the files under d: sorted [relative path, contents normalized] */
func walk(d, rel, out) {
    for (var f : listdir(d)) {
        var p = d + "/" + f;
        var r = rel == "" ? f : rel + "/" + f;
        if (isdir(p))
            walk(p, r, out);
        else {
            var data = readfile(p);
            if (startswith(f, "s."))
                data = head(data);
            push(out, [r, norm(data)]);
        }
    }
    return out;
}

func tree(d) {
    return sorted(walk(d, "", []));
}

var ours = abspath(argv[0]);
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 60;
var cssc = getenv("CSSC_DIR");
if (cssc == nil || cssc == "")
    cssc = "/usr/lib/x86_64-linux-gnu/cssc";
if (!exists(cssc + "/get")) {
    print("csscdiff: CSSC is needed (set CSSC_DIR)");
    exit(2);
}
srand(seed);
var top = mkdtemp("csscdiff.");
var passed = 0, failed = 0, skipped = 0;
var da, db, log, run_no, counter = 0;

for (run_no = 0; run_no < runs; run_no++) {
    da = sprintf("%s/a%d", top, run_no);
    db = sprintf("%s/b%d", top, run_no);
    mkdirs(da);
    mkdirs(db);
    log = [];

    /* the SCCS file */
    for (var d : [da, db])
        writefile(d + "/desc", "descriptive\ntext\n");
    write(text(rand(0, 8)));
    var opts = [];
    if (frand() < 0.4)
        push(opts, "-fb");
    if (frand() < 0.2)
        push(opts, "-fj");
    if (frand() < 0.2)
        push(opts, "-fttype");
    if (frand() < 0.2)
        push(opts, "-fqqq");
    if (frand() < 0.2)
        push(opts, sprintf("-r%d", rand(1, 3)));
    both(["admin", "-if", "-ycreated"] + opts + ["s.f"]);
    for (var d : [da, db])
        remove(d + "/f");

    for (var step = rand(3, 14); step > 0; step--) {
        var k = frand();
        var ss = sids();
        if (k < 0.45) {
            /* an edit */
            var args = ["get", "-e"];
            if (frand() < 0.4 && len(ss)) {
                var parts = split(choice(ss), ".");
                push(args, "-r" + join(slice(parts, 0, choice([1, 2, len(parts)])), "."));
            }
            if (frand() < 0.3)
                push(args, "-b");
            if (!both(args + ["s.f"], nil, ["f", "p.f"]))
                break;
            var g = gfile();
            if (g == nil)
                continue;
            for (var e = rand(0, 4); e > 0; e--) {
                var c = frand();
                if (c < 0.4 && len(g))
                    pop(g, rand(len(g)));
                else if (c < 0.8)
                    insert(g, rand(0, len(g)), choice(text(1)));
                else if (len(g))
                    g[rand(len(g))] = choice(text(1));
            }
            write(g);
            if (frand() < 0.15) {
                if (!both(["unget", "s.f"], nil, ["f", "p.f"]))
                    break;
                continue;
            }
            var dargs = ["delta", "-y" + choice(["fix", "a\nb", ""])];
            if (!both(dargs + ["s.f"], nil, ["f", "p.f"]))
                break;
        } else if (k < 0.55 && len(ss)) {
            both(["rmdel", "-r" + choice(ss), "s.f"]);
        } else if (k < 0.75) {
            var args = ["get", "-p"];
            for (var o : ["-m", "-n", "-k", "-s"])
                if (frand() < 0.25)
                    push(args, o);
            if (len(ss) && frand() < 0.5) {
                var parts = split(choice(ss), ".");
                push(args, "-r" + join(slice(parts, 0, rand(1, len(parts))), "."));
            }
            if (len(ss) && frand() < 0.2)
                push(args, "-t");
            /* CSSC is not consistent about the SID of %I% when the delta
               got is excluded: -x only without -r, of older deltas */
            if (len(ss) && frand() < 0.15 && !any_prefix(args, ["-r"]) && len(ss) > 1)
                push(args, "-x" + choice(slice(ss, 1)));
            if (len(ss) && frand() < 0.15)
                push(args, "-i" + choice(ss));
            /* CSSC leaves excluded and included deltas out of the l-file */
            if (frand() < 0.2 && !any_prefix(args, ["-i", "-x"]))
                push(args, "-l");
            both(args + ["s.f"], nil, ["l.f"]);
            for (var d : [da, db])
                if (exists(d + "/l.f"))
                    remove(d + "/l.f");
        } else if (k < 0.9) {
            var specs = [nil, ":I: :R: :L: :B: :S: :DT: :DS: :DP: :P:", ":Dt:", ":DL: :Li: :Ld: :Lu:",
                         ":D: :Dy: :Dm: :Dd: :T: :Th: :Tm: :Ts:",
                         ":C::MR:", ":FL:", ":Y: :MF: :MP: :KF: :BF: :J: :LK: :Q: :M: :FB: :CB: :Ds: :ND:",
                         ":UN:", ":FD:", ":BD:", ":GB:", ":W: :A: :Z: :F:", ":Dn: :Dx: :Dg: :DI:",
                         "text\\tand\\n:I:"];
            var args = ["prs"];
            var spec = choice(specs);
            if (spec != nil)
                push(args, "-d" + spec);
            var r = frand();
            if (len(ss) && r < 0.3)
                args += [choice(["-e", "-l"]), "-r" + choice(ss)];
            else if (len(ss) && r < 0.5)
                push(args, "-r" + choice(ss));
            if (frand() < 0.3)
                push(args, "-a");
            both(args + ["s.f"]);
        } else {
            var c = frand();
            if (c < 0.3)
                both(["sact", "s.f"]);
            else if (c < 0.5 && len(ss))
                both(["val", "-r" + choice(ss + ["1", "9.9", "1.0"]), "s.f"]);
            else if (c < 0.7) {
                var f = choice(["-fb", "-db", "-fj", "-dj", "-fttype", "-dt", "-fqtext", "-dq",
                                "-fmmod", "-dm", "-fd1.1", "-dd", "-fn", "-dn", "-fl1", "-dl1",
                                "-auser1", "-a!user2", "-euser1", "-a123", "-tdesc", "-t",
                                "-fc5", "-ff1", "-dc", "-df", "-fla", "-dla", "-fi", "-di"]);
                both(["admin", f, "s.f"]);
            } else if (c < 0.8)
                both(["get", "-p", "-s", "-c" + choice(["990101", "680101", "20", "991231235959"]), "s.f"]);
            else
                both(["get", "-g", "-L", "s.f"]);
        }
    }
    rmtree(da);
    rmtree(db);
}

/* the front end: the same commands with sccs, in a directory with SCCS */
var front = getenv("CSSC_SCCS");
if (front == nil || front == "")
    front = "/usr/bin/sccs";
if (exists(front)) {
    var fa = top + "/fa", fb = top + "/fb";
    for (var d : [fa, fb]) {
        mkdirs(d + "/SCCS");
        writefile(d + "/one.c", "int one;\n/* %W% */\n");
        writefile(d + "/two", "a\nb\nc\n");
    }
    var steps = [["create", "one.c", "two"], ["info"], ["check"],
                 ["tell"], ["edit", "one.c"], ["info"], ["check"], ["tell"], ["tell", "-u", "nobody"],
                 ["info", "-U"], ["edit", "two"], ["unedit", "two"], ["unedit", "two"],
                 ["delget", "-yfirst", "one.c"], ["deledit", "-ysecond", "one.c"],
                 ["diffs", "one.c"], ["delta", "-ythird", "one.c"], ["get", "one.c", "two"],
                 ["prs", "-d:I:", "one.c"], ["edit", "-r1.1", "two"], ["unget", "two"], ["fix", "-r1.3", "one.c"],
                 ["delta", "-yfixed", "one.c"], ["val", "one.c"], ["what", "one.c"], ["clean"],
                 ["get", "-k", "two"], ["clean"], ["get", "one.c"], ["check"], ["rmdel", "-r1.3", "one.c"],
                 ["prs", "-d:I: :DT:", "-a", "one.c"]];
    for (var st : steps) {
        var a = prun([ours + "/sccs"] + st, fa);
        var b = prun([front] + st, fb);
        if (b[0] == nil) {
            skipped++;
            continue;
        }
        var ta = tree(fa), tb = tree(fb);
        if (a[0] == b[0] && norm(a[1]) == norm(b[1]) && ta == tb)
            passed++;
        else {
            failed++;
            print("FAIL sccs " + join(st, " "));
            print("  status", a[0] ? "True" : "False", b[0] ? "True" : "False");
            show("ours", slice(a[1], 0, 300));
            show("cssc", slice(b[1], 0, 300));
            if (ta != tb) {
                var xa = [], xb = [];
                for (var e : ta)
                    if (!(e in tb) && len(xa) < 3)
                        push(xa, e[0]);
                for (var e : tb)
                    if (!(e in ta) && len(xb) < 3)
                        push(xb, e[0]);
                printf("  files: %q %q\n", xa, xb);
            }
        }
    }
}
rmtree(top);
printf("csscdiff: %d passed, %d failed%s\n", passed, failed,
       skipped ? sprintf(" (%d skipped: CSSC crashed)", skipped) : "");
exit(failed ? 1 : 0);
