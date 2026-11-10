/*
 * exdiff.tl - the ex scripts (tests/excases.txt) under this ex, nvi's ex
 * and vim's ex mode: the output, exit status and resulting file shown
 * where this ex differs from both
 *
 * usage: tl exdiff.tl [-a] [NAME...]      (-a: all of them)
 */
include "vipty.tl";

var EDITORS = [["mine", [abspath(scriptdir + "/../../../../usr.bin/vi/ex"), "-s"]], ["nvi", ["/usr/bin/nex", "-s"]],
               ["vim", ["/usr/bin/vim", "-u", "NONE", "-i", "NONE", "-N", "-es"]]];

func runcase(argv0, c, work) {
    rmtree(work);
    mkdirs(work);
    writefile(work + "/f.txt", unlines(c.text));
    var script = unlines(c.cmds);
    if (!len(c.cmds) || !(trim(c.cmds[len(c.cmds) - 1]) in ["q", "q!", "x", "wq", "wq!"]))
        script += "w! f.txt\nq!\n";
    var r = run(argv0 + ["f.txt"], {in: script, cwd: work, timeout: 10,
                env: {LC_ALL: "C", HOME: work, SHELL: "/bin/sh", COLUMNS: "80", LINES: "24", EXINIT: nil, TERM: nil}});
    if (r.timedout)
        return {out: "TIMEOUT", rc: -1, file: nil, err: ""};
    return {out: replace(r.out, work, "$W"), rc: r.status, file: readfile(work + "/f.txt"), err: r.err};
}

/* what is compared: the output, the file, and whether it failed */
func key(r) {
    return [r.out, r.file, r.rc != 0];
}

var showall = "-a" in argv;
var names = filter(argv, func (a) { return a != "-a"; });
var cases = parse_cases(scriptdir + "/excases.txt", "cmds");
if (len(names))
    cases = filter(cases, func (c) { return c.name in names; });
var work = mkdtemp("exdiff.");
var nd = 0;
for (var c : cases) {
    var res = {};
    for (var e : EDITORS)
        res[e[0]] = runcase(e[1], c, work + "/" + e[0]);
    var m = key(res.mine);
    if (!showall && (m == key(res.nvi) || m == key(res.vim)))
        continue;
    nd++;
    printf("==== %s\n", c.name);
    for (var e : EDITORS) {
        var r = res[e[0]];
        printf("  %-4s rc=%d out=%q file=%q\n", e[0], r.rc, r.out, r.file);
        if (e[0] == "mine" && r.err != "")
            printf("       err=%q\n", r.err);
    }
}
printf("%d of %d differ from both\n", nd, len(cases));
rmtree(work);
