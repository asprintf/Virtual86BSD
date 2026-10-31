/*
 * fuzz.tl - mutation fuzzing of the lex source reader and the automaton
 * construction.
 *
 * usage: tl fuzz.tl LEX SEED COUNT
 *
 * The seeds are the sources in tests/cases.tl, tests/lexers and tests/cases.
 * Each mutant (bytes inserted, deleted, duplicated or replaced, often with
 * lex syntax) is given to LEX (built with sanitizers); any sanitizer
 * report, signal or hang is a failure, and the mutant is saved in
 * /tmp/lfuzz-fail.
 */
include "../../testlib/testlib.tl";
include "cases.tl";

var lex = abspath(argv[0]);
srand(int(argv[1]));
var count = int(argv[2]);

var seeds = [];
for (var f : listdir(scriptdir + "/lexers"))
    push(seeds, readfile(scriptdir + "/lexers/" + f));
/* the here-documents <<'L' ... L of the case scripts */
for (var f : listdir(scriptdir + "/cases")) {
    var t = readfile(scriptdir + "/cases/" + f);
    var p = 0;
    while ((p = index(t, "<<'L'\n", p)) >= 0) {
        var s = p + 6;
        var e = index(t, "\nL\n", s);
        if (e < 0)
            break;
        push(seeds, slice(t, s, e));
        p = e + 3;
    }
}
for (var c : CASES)
    push(seeds, c.src);

var tokens = ["%%", "%{", "%}", "%s", "%x", "%array", "%pointer", "%p 10", "[", "]", "[^", "(",
              ")", "{", "}", "{2,3}", "{0}", "{1000}", "{D}", "\"", "\\", "\\x", "\\0", "/", "$", "^",
              "<", ">", "<*>", "|", "*", "+", "?", ".", "\n", "\t", " ", "[:alpha:]", "[.a.]",
              "REJECT;", "yymore();", "BEGIN 0;", "/*", "*/", "D [0-9]\n"];

func mutate(s) {
    for (var n = rand(1, 6); n > 0; n--) {
        var op = rand(5);
        var pos = rand(0, len(s));
        if (op == 0)
            s = slice(s, 0, pos) + choice(tokens) + slice(s, pos);
        else if (op == 1 && len(s))
            s = slice(s, 0, pos) + slice(s, pos + rand(1, 20));
        else if (op == 2 && len(s)) {
            var a = rand(len(s));
            var b = min(len(s), a + rand(1, 200));
            s = slice(s, 0, pos) + slice(s, a, b) + slice(s, pos);
        } else if (op == 3 && len(s)) {
            var k = min(pos, len(s) - 1);
            s = slice(s, 0, k) + chr(rand(256)) + slice(s, k + 1);
        } else
            s = slice(s, 0, pos) + chr(rand(256)) + slice(s, pos);
    }
    return s;
}

mkdirs("/tmp/lfuzz-fail");
var d = mkdtemp("lfuzz.");
var fails = 0, accepted = 0;
var env = {ASAN_OPTIONS: "detect_leaks=0:abort_on_error=0", UBSAN_OPTIONS: "print_stacktrace=1"};
for (var i = 0; i < count; i++) {
    var m = mutate(choice(seeds));
    writefile(d + "/g.l", m);
    var opts = choice([[], ["-t"], ["-v"], ["-tv"], ["-n"]]);
    var r = run([lex] + opts + ["g.l"], {cwd: d, timeout: 30, env: env, out: "null"});
    var bad, why;
    if (r.timedout) {
        bad = true;
        why = "timeout";
    } else {
        bad = r.status > 2 || index(r.err, "Sanitizer") >= 0 || index(r.err, "runtime error") >= 0 ||
              index(r.err, "internal error") >= 0;
        why = slice(r.err, -1500);
        accepted += r.status == 0;
    }
    if (bad) {
        fails++;
        var p = sprintf("/tmp/lfuzz-fail/f%d.y", i);
        writefile(p, m);
        print("FAIL", p, why);
    }
}
rmtree(d);
printf("%d mutants, %d accepted, %d failures\n", count, accepted, fails);
