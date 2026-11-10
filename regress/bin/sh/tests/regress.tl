/*
 * regress.tl - the regression suite of sh: every case of cases*.txt is run
 * with ../sh, and its standard output and exit status (the negative signal
 * number if a signal ended it) are compared with tests/expected.json.
 * --update records them instead.
 *
 * usage: tl regress.tl [--update]
 *
 * A case is the lines after "#### NAME" up to the next such line; it runs
 * as a script in a new directory ($WORK in the output) with LC_ALL=C, PATH
 * the standard utilities' (getconf PATH), HOME that directory and ENV empty.
 */

var SH = abspath(scriptdir + "/../../../../bin/sh/sh");
var TMP = getenv("TMPDIR");
var WORK = (TMP == nil || TMP == "" ? "/tmp" : TMP) + "/sh-regress-work";

/* the cases of a file: a list of [name, script] */
func parse(path) {
    var cases = [], name = nil, buf = "";
    for (var l : split(readfile(path), "\n")) {
        if (startswith(l, "#### ")) {
            if (name != nil)
                push(cases, [name, buf]);
            name = trim(substr(l, 5));
            buf = "";
        } else
            buf += l + "\n";
    }
    if (name != nil)
        push(cases, [name, substr(buf, 0, len(buf) - 1)]);     /* the file's last newline */
    return cases;
}

var r = run(["getconf", "PATH"], {err: "null"});
var STDPATH = r.status == 0 && trim(r.out) != "" ? trim(r.out) : "/usr/bin:/bin";

func runcase(script) {
    rmtree(WORK);
    mkdir(WORK);
    writefile(WORK + "/script.sh", script);
    var r = run([SH, WORK + "/script.sh"],
                {cwd: WORK, env: {LC_ALL: "C", PATH: STDPATH, HOME: WORK, ENV: ""}, stdin: "null", timeout: 20});
    if (r.timedout)
        return ["TIMEOUT", -1];
    return [replace(r.out, WORK, "$WORK"), r.signal ? -r.signal : r.status];
}

var update = "--update" in argv;
var expfile = scriptdir + "/expected.json";
var expected = update || !exists(expfile) ? {} : unjson(readfile(expfile));
var got = {}, fails = 0, n = 0;
for (var f : listdir(scriptdir)) {
    if (!startswith(f, "cases") || !endswith(f, ".txt"))
        continue;
    for (var c : parse(scriptdir + "/" + f)) {
        var key = f + ":" + c[0];
        n++;
        got[key] = runcase(c[1]);
        if (!update && get(expected, key, nil) != got[key]) {
            fails++;
            printf("FAIL %s\n", key);
            printf("  expected: %s\n", repr(get(expected, key, nil)));
            printf("  got:      %s\n", repr(got[key]));
        }
    }
}
rmtree(WORK);
if (update) {
    writefile(expfile, json(got, 1));
    printf("recorded %d cases\n", n);
    exit(0);
}
printf("%d passed, %d failed\n", n - fails, fails);
exit(fails ? 1 : 0);
