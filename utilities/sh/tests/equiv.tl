/*
 * equiv.tl - equivalence tests: run programs under two builds of this
 * shell (for example the hand-written-parser release and the yacc/lex
 * build) and report every difference in standard output, standard error
 * or exit status.
 *
 *     tl equiv.tl OLD NEW cases            the regression cases, three ways
 *     tl equiv.tl OLD NEW gen SEED N       N random (and mutated) programs
 *     tl equiv.tl OLD NEW arith SEED N     N random arithmetic expansions
 *     tl equiv.tl OLD NEW files F...       the given script files
 *     tl equiv.tl OLD NEW stdin            scripts that read their own input
 *     tl equiv.tl OLD NEW grid K N         part K of N of the error grid
 *
 * Each case is run as a script file, with -c and on standard input (except
 * where noted).  Both shells are run under one name (a symbolic link,
 * written as "sh" in what they print), so that argv[0] does not differ.
 */
include "gensh.tl";
include "pyrepr.tl";

var TMP = getenv("TMPDIR");
var WORK = (TMP != nil && TMP != "" ? TMP : "/tmp") + "/sh-equiv-work";
var ENV = {LC_ALL: "C", PATH: "/usr/local/bin:/usr/bin:/bin", ENV: "",
           PS1: nil, PS2: nil, PS4: nil, IFS: nil, CDPATH: nil, OPTIND: nil};

/* the link through which the shells are run, made in a directory of our own */
var BINDIR = mkdtemp("sh-equiv-bin.");
var SHLINK = BINDIR + "/sh";

/* [stdout, stderr, status]; stdout is ["TIMEOUT"] or ["SKIP"] when it did not run */
func runsh(sh, script, how, stdin_data, timeout) {
    if (stdin_data == nil)
        stdin_data = "";
    if (timeout == nil)
        timeout = 10;
    var w = WORK + "-" + str(getpid());
    rmtree(w);
    mkdirs(w);
    var env = copy(ENV);
    env.HOME = w;
    var p = w + "/script.sh";
    writefile(p, script);
    var args, inp;
    if (how == "file") {
        args = [SHLINK, p];
        inp = stdin_data;
    } else if (how == "c") {
        args = [SHLINK, "-c", script, "arg0"];
        inp = stdin_data;
    } else {
        args = [SHLINK];
        inp = script + stdin_data;
    }
    var res;
    if (how == "c" && index(script, "\0") >= 0)
        res = [["SKIP"], "", nil];      // a NUL cannot be in an argument
    else {
        remove(SHLINK);
        symlink(abspath(sh), SHLINK);
        var r = run(args, {cwd: w, in: inp, env: env, timeout: timeout});
        if (r.timedout) {
            res = [["TIMEOUT"], "", nil];
            killstrays();
        }
        else
            res = [norm(r.out, w), norm(r.err, w), r.signal ? -r.signal : r.status];
    }
    rmtree(w);
    return res;
}

/* after a timeout: the subshells left running (a timeout kills only the
   shell itself), known by the link they were started through */
func killstrays() {
    var r = run(["ps", "-A", "-o", "pid=", "-o", "args="], {err: "null"});
    for (var l : lines(r.out)) {
        var m = match(l, "^ *([0-9]+) (.*)$");
        if (m && (m[2] == SHLINK || startswith(m[2], SHLINK + " ")))
            kill(int(m[1]), SIGKILL);
    }
}

func norm(s, w) {
    return replace(replace(s, w, "$WORK"), SHLINK, "sh");
}

/* the value as Python's repr would show it (bytes for the outputs) */
func showval(x) {
    if (x == nil)
        return "None";
    if (typeof(x) == "list")
        return pyrepr(x[0]);
    if (typeof(x) == "string")
        return pybytes(slice(x, 0, 400));
    return str(x);
}

var n_runs = 0, n_diffs = 0, n_flaky = 0;

/*
 * Compares the results a (old) and b (new).  With again, [old, new,
 * script, how, timeout], the shells are run again: a difference is
 * reported only if the old shell is consistent with itself.
 */
func compare(name, a, b, script, isbytes, again) {
    n_runs++;
    if (a == b)
        return true;
    if (again != nil)
        for (var k = 0; k < 3; k++)
            if (runsh(again[0], again[2], again[3], "", again[4]) != a ||
                runsh(again[1], again[2], again[3], "", again[4]) == a) {
                n_flaky++;
                return true;
            }
    n_diffs++;
    print("DIFF", name);
    print("  script:", slice(isbytes ? pybytes(script) : pyrepr(script), 0, 600));
    var labels = ["stdout", "stderr", "status"];
    for (var i = 0; i < 3; i++)
        if (a[i] != b[i]) {
            printf("  %s old: %s\n", labels[i], showval(a[i]));
            printf("  %s new: %s\n", labels[i], showval(b[i]));
        }
    flush();
    return false;
}

func done(what) {
    printf("%s: %d runs, %d differences, %d nondeterministic\n", what, n_runs, n_diffs, n_flaky);
    return n_diffs == 0;
}

/* [name, script] of the cases of a file */
func parse_cases(path) {
    var text = readfile(path);
    var cases = [], name = nil, buf = "";
    var pieces = split(text, "\n");
    for (var i = 0; i < len(pieces); i++) {
        var line = pieces[i] + (i < len(pieces) - 1 ? "\n" : "");
        if (line == "")
            continue;
        if (startswith(line, "#### ")) {
            if (name)
                push(cases, [name, buf]);
            name = trim(substr(line, 5));
            buf = "";
        } else
            buf += line;
    }
    if (name)
        push(cases, [name, buf]);
    return cases;
}

func cmd_cases(old, new) {
    for (var fn : listdir(scriptdir))
        if (startswith(fn, "cases") && endswith(fn, ".txt"))
            for (var c : parse_cases(scriptdir + "/" + fn))
                for (var how : ["file", "c", "stdin"]) {
                    var a = runsh(old, c[1], how);
                    var b = runsh(new, c[1], how);
                    compare(sprintf("%s [%s]", c[0], how), a, b, c[1], false, [old, new, c[1], how, 10]);
                }
}

func cmd_gen(old, new, seed, n) {
    srand(seed);
    for (var i = 0; i < n; i++) {
        var script = program();
        if (frand() < 0.5)
            script = mutate(script);
        var how = choice(["file", "file", "c", "stdin"]);
        var a = runsh(old, script, how, "", 5);
        var b = runsh(new, script, how, "", 5);
        compare(sprintf("gen %d/%d [%s]", seed, i, how), a, b, script, false, [old, new, script, how, 5]);
    }
}

func cmd_arith(old, new, seed, n) {
    srand(seed);
    for (var i = 0; i < n; i++) {
        var script = arith_program();
        var a = runsh(old, script, "file", "", 5);
        var b = runsh(new, script, "file", "", 5);
        compare(sprintf("arith %d/%d", seed, i), a, b, script, false, [old, new, script, "file", 5]);
    }
}

/* the error grid (gensh.tl's grid()), split into parts for parallel runs */
func cmd_grid(old, new, part, parts) {
    var g = grid();
    for (var k = 0; k < len(g); k++) {
        if (k % parts != part)
            continue;
        var name = g[k][0], script = g[k][1];
        var how = k % 5 ? "c" : "file";
        var a = runsh(old, script, how, "", 5);
        var b = runsh(new, script, how, "", 5);
        compare(sprintf("grid %s [%s]", name, how), a, b, script, false, [old, new, script, how, 5]);
    }
}

func cmd_files(old, new, files) {
    for (var fn : files) {
        var script = readfile(fn);
        if (script == nil)
            error("cannot read %s", fn);
        for (var how : ["file", "c", "stdin"]) {
            var a = runsh(old, script, how);
            var b = runsh(new, script, how);
            compare(sprintf("%s [%s]", fn, how), a, b, script, true);
        }
    }
}

/* The shell must not read past the current command: the rest of the
   input belongs to the commands it runs. */
func cmd_stdin(old, new) {
    for (var c : STDIN_CASES) {
        var a = runsh(old, c[0], "stdin", c[1]);
        var b = runsh(new, c[0], "stdin", c[1]);
        compare("stdin", a, b, c[0], false);
    }
}

if (len(argv) < 3) {
    eprintf("usage: tl equiv.tl OLD NEW cases|gen SEED N|arith SEED N|files F...|stdin|grid K N\n");
    exit(2);
}
var old = argv[0], new = argv[1], what = argv[2];
if (what == "cases")
    cmd_cases(old, new);
else if (what == "gen")
    cmd_gen(old, new, int(argv[3]), int(argv[4]));
else if (what == "arith")
    cmd_arith(old, new, int(argv[3]), int(argv[4]));
else if (what == "grid")
    cmd_grid(old, new, int(argv[3]), int(argv[4]));
else if (what == "files")
    cmd_files(old, new, slice(argv, 3));
else if (what == "stdin")
    cmd_stdin(old, new);
var ok = done(what);
rmtree(BINDIR);
exit(ok ? 0 : 1);
