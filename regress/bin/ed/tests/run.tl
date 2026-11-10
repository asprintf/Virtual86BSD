/*
 * run.tl - the regression suite of ed: each case of cases.tl (a file, an ed
 * script and options) is run in a new directory; the standard output, the
 * exit status, the files written and (where the case asks) the standard
 * error are compared with tests/expected.json.  --freeze records the
 * current results instead.
 *
 * usage: tl run.tl ED [--freeze] [NAME...]
 * The cases are shared with difftest.tl, which compares with GNU ed instead.
 */
include "../../../lib/testlib/testlib.tl";
include "cases.tl";

var U8 = utf8_locale();
var EXPECTED = scriptdir + "/expected.json";

/* the case's locale: a UTF-8 one unless it names another; nil if the case
   needs a UTF-8 locale and there is none */
func locale_of(cs) {
    var loc = cs.locale == nil ? "C.UTF-8" : cs.locale;
    if (replace(lower(loc), "-", "") == "c.utf8") {
        if (U8 != "")
            return U8;
        if (non_ascii(cs.script))
            return nil;
        if (cs.files != nil)
            for (var f : cs.files)
                if (non_ascii(cs.files[f]))
                    return nil;
        return "C";
    }
    return loc;
}

func runcase(ed, cs, d) {
    mkdir(d);
    var files = cs.files == nil ? {f: "one\ntwo\nthree\nfour\nfive\n"} : cs.files;
    for (var name : files)
        writefile(d + "/" + name, files[name]);
    var args = cs.args == nil ? ["-s", "f"] : cs.args;
    var r = run([ed] + args, {in: cs.script, cwd: d, env: {LC_ALL: locale_of(cs), HOME: d}, timeout: 20});
    var fs = {};
    for (var name : listdir(d)) {
        var p = d + "/" + name;
        if (isfile(p)) {
            var data = readfile(p);
            /* small files as they are (base64), large ones by their digest */
            fs[name] = len(data) > 4096 ? "sha256:" + sha256(data) : base64(data);
        }
    }
    return {out: r.out, status: r.status, files: fs, err: cs.stderr ? r.err : ""};
}

if (!len(argv)) {
    eprintf("usage: tl run.tl ED [--freeze] [NAME...]\n");
    exit(2);
}
var ed = abspath(argv[0]);
var freeze = "--freeze" in argv;
var names = filter(slice(argv, 1), func (a) { return !startswith(a, "--"); });
var expected = {};
if (!freeze && exists(EXPECTED))
    expected = unjson(readfile(EXPECTED));
var base = mkdtemp("edrun.");
var results = {}, n = 0, fails = 0;
for (var i = 0; i < len(CASES); i++) {
    var cs = CASES[i];
    var name = cs.name == nil ? sprintf("case%d", i) : cs.name;
    if (len(names) && !(name in names))
        continue;
    if (locale_of(cs) == nil)
        continue;           /* needs a UTF-8 locale, and there is none */
    n++;
    var got = runcase(ed, cs, base + "/" + str(i));
    results[name] = got;
    if (freeze)
        continue;
    var want = expected[name];
    if (got == want)
        continue;
    fails++;
    printf("FAIL %s\n", name);
    if (want == nil) {
        print("    no expected result (run with --freeze)");
        continue;
    }
    for (var k : ["status", "out", "err", "files"])
        if (got[k] != want[k]) {
            printf("    %s: want %q\n", k, k == "out" ? substr(want[k], 0, 300) : want[k]);
            printf("    %s: got  %q\n", k, k == "out" ? substr(got[k], 0, 300) : got[k]);
        }
}
rmtree(base);
if (freeze) {
    if (len(names) && exists(EXPECTED)) {
        var old = unjson(readfile(EXPECTED));
        for (var k : results)
            old[k] = results[k];
        results = old;
    }
    writefile(EXPECTED, json(results, 1) + "\n");
    printf("froze %d cases\n", n);
    exit(0);
}
printf("%d passed, %d failed\n", n - fails, fails);
exit(fails ? 1 : 0);
