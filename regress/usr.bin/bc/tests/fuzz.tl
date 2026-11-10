/*
 * fuzz.tl - mutation fuzzing of bc: the scanner, the parser and the
 * interpreter.
 *
 * usage: tl fuzz.tl BC SEED COUNT
 *
 * The seeds are the programs of tests/cases.tl.  Each mutant (bytes
 * inserted, deleted, duplicated or replaced, often with bc tokens) is run
 * by BC (built with sanitizers), with and without -l; a sanitizer report
 * or a crash is a failure, and the mutant is saved in /tmp/bcfuzz-fail.
 * Mutants that run longer than the time limit are only counted.
 */
include "cases.tl";

if (len(argv) < 3) {
    eprintf("usage: tl fuzz.tl BC SEED COUNT\n");
    exit(2);
}
var bc = abspath(argv[0]);
srand(int(argv[1]));
var count = int(argv[2]);
var seeds = apply(CASES, func (cs) { return cs.prog; });
var tokens = ["\n", ";", "{", "}", "(", ")", "[", "]", "define f(x) {\n", "auto a, b[]\n", "return (",
              "for (i = 0; i < 3; i++) ", "while (", "if (", "else ", "break", "quit", "scale = ", "ibase = ",
              "obase = ", "sqrt(", "length(", "scale(", "^", "%", "/", "*", "-", "+", "++", "--", "=",
              "+=", "^=", "\"", "/*", "*/", "\\\n", ".", "0", "99999999999", "A", "F", "a[", "[]", ",",
              "<", "==", "!", "&&", "e(", "l(", "j(", "print ", "#"];

/* s with t put in at pos */
func ins(s, pos, t) {
    return slice(s, 0, pos) + t + slice(s, pos);
}

func mutate(s) {
    for (var n = rand(1, 6); n > 0; n--) {
        var op = rand(5);
        var pos = rand(0, len(s));
        if (op == 0)
            s = ins(s, pos, choice(tokens));
        else if (op == 1 && len(s))
            s = slice(s, 0, pos) + slice(s, min(len(s), pos + rand(1, 10)));
        else if (op == 2 && len(s)) {
            var a = rand(len(s));
            var b = min(len(s), a + rand(1, 100));
            s = ins(s, pos, slice(s, a, b));
        } else if (op == 3 && len(s)) {
            var p = min(pos, len(s) - 1);
            s = slice(s, 0, p) + chr(rand(256)) + slice(s, p + 1);
        } else
            s = ins(s, pos, chr(rand(256)));
    }
    return s;
}

/* does the standard error hold a report of the undefined behavior sanitizer? */
func runtime_error(err) {
    for (var l : split(replace(err, "\0", ""), "\n"))
        if (match(l, "\\.c:[0-9]+:[0-9]+: runtime error") != nil)
            return 1;
    return 0;
}

var faildir = "/tmp/bcfuzz-fail";
mkdirs(faildir);
var fails = 0, slow = 0;
var env = {ASAN_OPTIONS: "detect_leaks=0", UBSAN_OPTIONS: "print_stacktrace=1"};
for (var i = 0; i < count; i++) {
    var m = mutate(choice(seeds));
    var args = choice([[], ["-l"]]);
    var r = run([bc] + args, {in: m, env: env, timeout: 10});
    if (r.timedout) {
        slow++;
        continue;
    }
    /* status above 2: an abnormal exit, or 128 + the number of a signal */
    if (r.status > 2 || "Sanitizer" in r.err || runtime_error(r.err)) {
        fails++;
        var p = sprintf("%s/f%d.bc", faildir, i);
        writefile(p, m);
        printf("FAIL %s %q %s\n", p, args, slice(r.err, max(0, len(r.err) - 1500)));
    }
}
printf("%d mutants, %d over the time limit, %d failures\n", count, slow, fails);
