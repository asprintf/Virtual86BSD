/*
 * difftest.tl - differential tests: run each case with this make and with a
 * reference make (GNU make by default) in fresh directories and compare the
 * output and exit status after normalising the program name and the parts
 * of diagnostics that are implementation-specific.
 *
 * usage: tl difftest.tl MAKE [--ref=/usr/bin/gmake] [-v] [name...]
 */
include "cases.tl";

var MINE, REF;

func normalise(out, tmp) {
    out = replace(out, tmp, "$TMP");
    var res = [];
    for (var l : lines(out)) {
        l = sub(l, "^(g?make|bmake)(\\[[0-9]+\\])?:", "make:");
        if (match(l, "^make: (Entering|Leaving) directory"))
            continue;
        l = replace(replace(l, MINE, "$MAKE"), REF, "$MAKE");
        // GNU make puts the makefile and line in error reports
        l = gsub(l, "\\[[^][:space:]]*:[0-9]+: ([^]]*)\\]", "[\\1]");
        l = sub(l, "^make: [^[:space:]]+:[0-9]+: \\*\\*\\*", "make: ***");
        l = sub(l, "^make: [^[:space:]]+:[0-9]+: ", "make: ");
        // the signal of a killed command
        l = sub(l, "\\] (Killed|Terminated|Interrupt)$", "] Signal");
        push(res, l);
    }
    return join(res, "\n");
}

func run_case(make, c, tmp) {
    mkdirs(tmp);
    for (var name : c.files) {
        var p = tmp + "/" + name;
        mkdirs(dirname(p));
        writefile(p, c.files[name]);
    }
    var script = c.setup + "\n" + c.run;
    var env = {MAKEFLAGS: nil, MAKELEVEL: nil, MFLAGS: nil, MAKE: nil,
               M: make, LC_ALL: "C", TZ: "UTC"};
    for (var k : get(c, "env", {}))
        env[k] = c.env[k];
    var r = run(["sh", "-c", "set -u\n" + script], {cwd: tmp, env: env, err: "out", timeout: 60});
    var out = r.timedout ? "TIMEOUT" : r.out;
    return normalise(out, tmp);
}

/* the lines of s as a file, for diff */
func linesfile(path, s) {
    var l = lines(s);
    writefile(path, len(l) ? join(l, "\n") + "\n" : "");
}

/* a unified diff (one line of context) from the reference to mine */
func showdiff(a, b, dir) {
    linesfile(dir + "/reference", b);
    linesfile(dir + "/mine", a);
    var r = run(["diff", "-U", "1", "reference", "mine"], {cwd: dir});
    var dl = lines(r.out);
    if (len(dl) >= 2) {
        dl[0] = "--- reference";
        dl[1] = "+++ mine";
    }
    for (var l : dl)
        printf("    %s\n", l);
}

if (!len(argv)) {
    eprintf("usage: tl difftest.tl MAKE [--ref=/usr/bin/gmake] [-v] [name...]\n");
    exit(2);
}
var ref = "/usr/bin/gmake";
var verbose = false;
var names = [];
for (var a : slice(argv, 1)) {
    if (startswith(a, "--ref="))
        ref = substr(a, 6);
    else if (a == "-v")
        verbose = true;
    else
        push(names, a);
}
MINE = abspath(argv[0]);
REF = ref;
var fails = 0, skipped = 0, n = 0;
var base = mkdtemp("mkdiff.");
for (var c : CASES) {
    if (len(names) && !(c.name in names))
        continue;
    if (("gnu" in c.differs && "gmake" in ref) || ("bmake" in c.differs && "bmake" in ref)) {
        skipped++;
        continue;
    }
    n++;
    var a = run_case(MINE, c, base + "/" + c.name + ".mine");
    var b = run_case(ref, c, base + "/" + c.name + ".ref");
    if (a != b) {
        fails++;
        print("DIFF", c.name);
        showdiff(a, b, base);
    } else if (verbose)
        print("same", c.name);
}
rmtree(base);
printf("%d/%d same as %s (%d skipped as known differences)\n", n - fails, n, ref, skipped);
exit(fails ? 1 : 0);
