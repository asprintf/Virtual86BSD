/*
 * tputdiff.tl - compare tput with ncurses tput ("tput" in PATH).
 *
 *     tl tests/tputdiff.tl ./tput
 *
 * For every terminal entry in the system's terminfo directories and every
 * capability name tput knows (from terminfo.c), the output and exit status
 * of both: booleans and numbers, strings as they are, and the strings that
 * take parameters with as many numbers as they use.  clear, init and reset
 * are left out (ncurses tput wants a terminal for init and reset, and adds
 * the extended capability E3 to clear), and so are lines and cols (ncurses
 * asks the terminal).
 */

var ours = abspath(len(argv) > 0 ? argv[0] : "./tput");
var src = readfile(dirname(ours) + "/../lib/terminfo.c");

/* the quoted strings of the initializer of the array NAME in src */
func cnames(name) {
    var start = index(src, name + "[] = {");
    var body = substr(src, start, index(src, "};", start) - start);
    var l = [];
    for (var m = matchat(body, "\"[^\"]+\""); m != nil; m = matchat(body, "\"[^\"]+\"")) {
        push(l, substr(body, m[0] + 1, m[1] - m[0] - 2));
        body = substr(body, m[1]);
    }
    return l;
}

/* the highest N of the %pN in s, 0 if none */
func nparams(s) {
    var np = 0;
    for (var m = match(s, "%p([1-9])"); m != nil; m = match(s, "%p([1-9])")) {
        np = max(np, int(m[1]));
        s = substr(s, matchat(s, "%p[1-9]")[1]);
    }
    return np;
}

var names = [];
for (var arr : ["boolnames", "numnames", "strnames"])
    for (var n : cnames(arr))
        if (!(n in ["lines", "cols", "clear"]))
            push(names, n);

var termset = {};
for (var d : ["/etc/terminfo", "/lib/terminfo", "/usr/share/terminfo"])
    if (isdir(d))
        for (var sub : listdir(d))
            if (isdir(d + "/" + sub))
                for (var t : listdir(d + "/" + sub))
                    termset[t] = 1;
var terms = sorted(keys(termset));

func tput(prog, args) {
    return run([prog] + args, {env: {LINES: nil, COLUMNS: nil}, stdin: "null"});
}

var passed = 0, failed = 0;
for (var t : terms) {
    for (var n : names) {
        var cases = [[n]];
        /* a string with parameters: as many numbers as it uses */
        var np = nparams(tput(ours, ["-T", t, n]).out);
        if (np) {
            var c = [n];
            for (var i = 0; i < np; i++)
                push(c, str(3 + 2 * i));
            push(cases, c);
        }
        for (var args : cases) {
            var a = tput(ours, ["-T", t] + args), b = tput("tput", ["-T", t] + args);
            if (a.out == b.out && a.status == b.status)
                passed++;
            else {
                failed++;
                printf("DIFF: tput -T %s %s: %q %d, ours %q %d\n", t, join(args, " "), b.out, b.status,
                       a.out, a.status);
            }
        }
    }
}
printf("%d passed, %d failed (%d terminals)\n", passed, failed, len(terms));
exit(failed ? 1 : 0);
