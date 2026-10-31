/*
 * run_diff.tl - differential tester: run each case with our sh and the
 * reference shells.  Cases file format: lines '#### name' start a case;
 * the rest is the script.  Compares stdout + exit status.  Prints cases
 * where ours differs from the reference majority (dash, bash --posix,
 * yash --posix, busybox ash, mksh).
 *
 * usage: tl run_diff.tl [CASEFILE...]      (default tests/cases.txt)
 */

var OURS = abspath(scriptdir + "/../sh");
var REFS = [["dash", ["dash"]], ["bash", ["bash", "--posix"]], ["yash", ["yash", "--posix"]],
            ["ash", ["busybox", "ash"]], ["mksh", ["mksh"]]];

/* the cases of a file: [name, script] */
func parse(path) {
    var text = readfile(path);
    if (text == nil)
        error("cannot read %s", path);
    var cases = [], name = nil, buf = "";
    var pieces = split(text, "\n");
    for (var i = 0; i < len(pieces); i++) {
        // the line with its newline, as Python reads it
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

/* [stdout, exit status] and the standard error; a signal is -N, as Python gives it */
func runsh(cmd, script, tmp) {
    var p = tmp + "/script.sh";
    writefile(p, script);
    var r = run(cmd + [p], {cwd: tmp, timeout: 10, stdin: "null",
                env: {LC_ALL: "C", PATH: "/usr/local/bin:/usr/bin:/bin", HOME: tmp, ENV: "",
                      BASH_ENV: nil}});
    if (r.timedout)
        return [["TIMEOUT", -1], ""];
    return [[r.out, r.signal ? -r.signal : r.status], r.err];
}

var WORK = mkdtemp("shdiff.") + "/work";

func fresh() {
    rmtree(WORK);
    mkdirs(WORK);
    return WORK;
}

/* Python's repr of a list of strings */
func pylist(l) {
    return "[" + join(apply(l, func (s) { return "'" + s + "'"; }), ", ") + "]";
}

var files = len(argv) ? argv : [scriptdir + "/cases.txt"];
var total = 0, bad = 0;
for (var f : files) {
    for (var c : parse(f)) {
        var name = c[0], script = c[1];
        total++;
        var o = runsh([OURS], script, fresh());
        var ours = o[0], oerr = o[1];
        var res = {};
        for (var ref : REFS)
            res[ref[0]] = runsh(ref[1], script, fresh())[0];
        // the most common result; of those as common, the first
        var majority = nil, mc = 0;
        for (var k : res) {
            var cnt = 0;
            for (var k2 : res)
                if (res[k2] == res[k])
                    cnt++;
            if (cnt > mc) {
                majority = res[k];
                mc = cnt;
            }
        }
        var agree = [], mk = [];
        for (var k : res) {
            if (res[k] == ours)
                push(agree, k);
            if (res[k] == majority)
                push(mk, k);
        }
        if (ours != majority && len(agree) < 2) {
            bad++;
            printf("=== %s   (agree: %s)\n", name, len(agree) ? join(agree, ",") : "none");
            printf("--- ours (rc=%d):\n%s", ours[1], ours[0]);
            if (oerr != "")
                printf("--- ours stderr:\n%s", oerr);
            printf("--- majority %s (rc=%d):\n%s", pylist(mk), majority[1], majority[0]);
        }
    }
}
printf("total=%d differing=%d\n", total, bad);
rmtree(dirname(WORK));
