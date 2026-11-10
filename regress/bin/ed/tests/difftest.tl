/*
 * difftest.tl - differential tests against GNU ed: each case of cases.tl is
 * a file, an ed script and options; standard output, the exit status and
 * the written files are compared (and the standard error where the case
 * asks).
 *
 * usage: tl difftest.tl ED [--ref=/usr/bin/ed] [-v] [name...]
 */
include "../../../lib/testlib/testlib.tl";
include "cases.tl";

var U8 = utf8_locale();

/* as in run.tl: a UTF-8 locale unless the case names another; nil if the
   case needs one and there is none */
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
    for (var name : listdir(d))
        if (isfile(d + "/" + name))
            fs[name] = readfile(d + "/" + name);
    return {out: r.out, status: r.status, files: fs, err: cs.stderr ? r.err : ""};
}

/* the differences of the outputs, as diff -U 1 shows them */
func showdiff(dir, ref, mine) {
    writefile(dir + "/out.ref", ref);
    writefile(dir + "/out.mine", mine);
    var r = run(["diff", "-U", "1", dir + "/out.ref", dir + "/out.mine"]);
    var ls = lines(r.out);
    print("    --- ref");
    print("    +++ mine");
    for (var i = 2; i < len(ls); i++)
        print("   ", ls[i]);
}

if (!len(argv)) {
    eprintf("usage: tl difftest.tl ED [--ref=/usr/bin/ed] [-v] [name...]\n");
    exit(2);
}
var ref = "/usr/bin/ed", verbose = 0, names = [];
for (var a : slice(argv, 1)) {
    if (startswith(a, "--ref="))
        ref = substr(a, 6);
    else if (a == "-v")
        verbose = 1;
    else
        push(names, a);
}
var mine = abspath(argv[0]);
var base = mkdtemp("eddiff.");
var fails = 0, n = 0, skipped = 0;
for (var i = 0; i < len(CASES); i++) {
    var cs = CASES[i];
    var name = cs.name == nil ? sprintf("case%d", i) : cs.name;
    if (len(names) && !(name in names))
        continue;
    if (cs.differs || locale_of(cs) == nil) {
        skipped++;
        continue;
    }
    n++;
    var a = runcase(mine, cs, sprintf("%s/%d.mine", base, i));
    var b = runcase(ref, cs, sprintf("%s/%d.ref", base, i));
    if (a != b) {
        fails++;
        printf("DIFF %s %q\n", name, substr(cs.script, 0, 200));
        if (a.out != b.out)
            showdiff(base, b.out, a.out);
        if (a.status != b.status)
            printf("    status: ref %d mine %d\n", b.status, a.status);
        if (a.files != b.files) {
            var all = keys(a.files);
            for (var k : keys(b.files))
                if (!(k in all))
                    push(all, k);
            for (var k : sorted(all)) {
                var x = get(a.files, k, nil), y = get(b.files, k, nil);
                if (x != y)
                    printf("    file %s: ref %q mine %q\n", k, substr(y == nil ? "" : y, 0, 200),
                           substr(x == nil ? "" : x, 0, 200));
            }
        }
        if (a.err != b.err)
            printf("    stderr: ref %q mine %q\n", b.err, a.err);
    } else if (verbose)
        print("same", name);
}
rmtree(base);
printf("%d/%d same as %s (%d skipped)\n", n - fails, n, ref, skipped);
exit(fails ? 1 : 0);
