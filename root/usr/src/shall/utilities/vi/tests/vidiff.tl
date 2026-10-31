/*
 * vidiff.tl - the keystroke cases (tests/vicases.txt) under this vi, nvi
 * and vim (vi-compatible), the resulting files compared
 *
 * usage: tl vidiff.tl [CASES-FILE] [--from NAME | NAME...]
 */
include "vipty.tl";

var EDITORS = [["mine", [abspath(scriptdir + "/../vi")]], ["nvi", ["/usr/bin/nvi"]],
               ["vim", ["/usr/bin/vim", "-u", "NONE", "-i", "NONE", "--noplugin"]]];

func runcase(argv0, c, work) {
    var path = work + "/f.txt";
    writefile(path, unlines(c.text));
    var v = vt(argv0 + [path], {cwd: work, env: {HOME: work, SHELL: "/bin/sh"}});
    vt_settle(v, 0.3);
    for (var k : c.keys)
        vt_keys(v, keys_decode(k));
    vt_keys(v, "\033");
    vt_keys(v, ":wq!\r");
    var st = vt_wait(v, 5);
    vt_kill(v);
    return st == nil ? "TIMEOUT" : readfile(path);
}

var args = argv;
var cf = scriptdir + "/vicases.txt";
if (len(args) && exists(args[0])) {
    cf = args[0];
    args = slice(args, 1);
}
var cases = parse_cases(cf, "keys");
if (len(args) && args[0] == "--from") {
    var i = 0;
    while (i < len(cases) && cases[i].name != args[1])
        i++;
    cases = slice(cases, i);
} else if (len(args))
    cases = filter(cases, func (c) { return c.name in args; });
var work = mkdtemp("vidiff.");
var ndiff = 0;
for (var c : cases) {
    var res = {};
    for (var e : EDITORS)
        res[e[0]] = runcase(e[1], c, work);
    if (res.mine == res.nvi || res.mine == res.vim) {
        var tag = res.mine == res.nvi && res.mine == res.vim ? "ok" : res.mine == res.nvi ? "ok(nvi)" : "ok(vim)";
        printf("%-30s %s\n", c.name, tag);
    } else {
        ndiff++;
        printf("%-30s DIFF\n", c.name);
        for (var e : EDITORS)
            printf("   %-5s %q\n", e[0], res[e[0]]);
    }
}
rmtree(work);
printf("%d differ of %d\n", ndiff, len(cases));
