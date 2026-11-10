/*
 * regress.tl - the regression suites of ex (tests/excases.txt) and vi
 * (tests/vicases.txt), compared with tests/expected.json; --update records
 * the current results instead.  --ex or --vi: that suite only.
 *
 * usage: tl regress.tl [--update] [--ex|--vi] [NAME...]
 *
 * An ex case is run by ex -s on f.txt (then w! f.txt and q! unless it
 * quits): its output, error output, exit status and the file.  A vi case:
 * its keys typed to vi on a terminal, then ESC :wq!: the file.  EX_BIN and
 * VI_BIN may name other binaries.
 */
include "vipty.tl";

func bin(var_, name) {
    var b = getenv(var_);
    return abspath(b != nil && b != "" ? b : scriptdir + "/../../../../usr.bin/vi/" + name);
}
var EX = bin("EX_BIN", "ex"), VI = bin("VI_BIN", "vi");

func fresh(work, c) {
    rmtree(work);
    mkdirs(work);
    writefile(work + "/f.txt", unlines(c.text));
}

func run_ex(c, work) {
    fresh(work, c);
    var script = unlines(c.cmds);
    if (!len(c.cmds) || !(trim(c.cmds[len(c.cmds) - 1]) in ["q", "q!", "x", "wq", "wq!"]))
        script += "w! f.txt\nq!\n";
    var r = run([EX, "-s", "f.txt"], {in: script, cwd: work, timeout: 10,
                env: {LC_ALL: "C", HOME: work, SHELL: "/bin/sh", COLUMNS: "80", LINES: "24", EXINIT: nil, TERM: nil}});
    if (r.timedout)
        return ["TIMEOUT"];
    var f = readfile(work + "/f.txt");
    return [latin1(replace(r.out, work, "$W")), latin1(r.err), r.signal ? -r.signal : r.status, f == nil ? nil : latin1(f)];
}

func run_vi(c, work) {
    fresh(work, c);
    var v = vt([VI, work + "/f.txt"], {cwd: work, env: {HOME: work, SHELL: "/bin/sh"}});
    for (var k : c.keys)
        vt_keys(v, keys_decode(k));
    vt_keys(v, "\033");
    vt_keys(v, ":wq!\r");
    var st = vt_wait(v, 5);
    vt_kill(v);
    if (st == nil)
        return ["TIMEOUT"];
    return [latin1(readfile(work + "/f.txt"))];
}

var update = "--update" in argv;
var suites = filter(["ex", "vi"], func (s) { return ("--" + s) in argv; });
if (!len(suites))
    suites = ["ex", "vi"];
var names = filter(argv, func (a) { return !startswith(a, "--"); });
var expf = scriptdir + "/expected.json";
var exp = exists(expf) ? unjson(readfile(expf)) : {};
var work = mkdtemp("viregress.") + "/w";
var fails = 0, n = 0;
for (var suite : suites) {
    var cases = parse_cases(scriptdir + "/" + suite + "cases.txt", suite == "ex" ? "cmds" : "keys");
    if (len(names))
        cases = filter(cases, func (c) { return c.name in names; });
    for (var c : cases) {
        var key = suite + ":" + c.name;
        var got = suite == "ex" ? run_ex(c, work) : run_vi(c, work);
        n++;
        if (update)
            exp[key] = got;
        else if (get(exp, key, nil) != got) {
            fails++;
            printf("FAIL %s\n", key);
            printf("  expected: %s\n", repr(get(exp, key, nil)));
            printf("  got:      %s\n", repr(got));
        }
    }
}
rmtree(dirname(work));
if (update) {
    writefile(expf, json(exp, 1));
    printf("recorded %d cases\n", n);
    exit(0);
}
printf("%d passed, %d failed\n", n - fails, fails);
exit(fails ? 1 : 0);
