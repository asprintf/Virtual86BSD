/*
 * rediff.tl - the regular expression matcher of ex compared with sed
 * (POSIX BREs, leftmost-longest): the matched text, global substitution
 * and back references in replacements
 *
 * usage: tl rediff.tl
 */

var EX = abspath(scriptdir + "/../../../../usr.bin/vi/ex");
var PATH = getenv("PATH");
var ENV = {LC_ALL: "C", PATH: PATH == nil ? "/bin:/usr/bin" : PATH, TERM: nil, EXINIT: nil, HOME: nil};

var pats = ["a*", "a\\{2,3\\}", "\\(ab\\)*", "\\(a*\\)*b", "a\\{0,1\\}\\(ab\\)*", "x*y*", "\\(a\\)\\1",
            "\\(.\\)\\(.\\)\\2\\1", "[^a]*", "^a", "a$", ".*", "\\(a*\\)\\(b*\\)", "[]a]", "[a-c]*d",
            "\\(a\\{1,2\\}\\)\\{2\\}", "b*\\(ab\\)*", "\\(x*\\)\\(y*\\)\\2", "[[:alpha:]][[:alnum:]]*", "a.c",
            "\\.", "a\\{3\\}", "\\(\\(a\\)b\\)*c", "\\(a\\)\\(b\\)*", "[[:digit:]]*", "^\\(a*\\)$", "\\(a\\|b\\)"];
var strs = ["aaa", "abab", "aabab", "xxyy", "abba", "baaac", "aaaa", "ab]c", "abcd", "xyy", "a.c",
            "zz9a1", "ababc", "aaab", "yyx", ""];

func count_groups(p) {
    var c = 0;
    for (var i = index(p, "\\("); i >= 0; i = index(p, "\\(", i + 1))
        c++;
    return c;
}

func ex_subst(s, p, repl, flags) {
    var script = sprintf("a\n%s\n.\n1s/%s/%s/%s\n1p\nq!\n", s, p, repl, flags);
    var r = run([EX, "-s", "/dev/null"], {in: script, env: ENV});
    return r.status == 0 ? r.out : s + "\n";
}

func sed_subst(s, p, repl, flags) {
    var r = run(["sed", sprintf("s/%s/%s/%s", p, repl, flags)], {in: s + "\n", env: ENV});
    return r.status == 0 ? r.out : nil;
}

var n = 0, bad = 0;
for (var p : pats) {
    if ("\\|" in p)
        continue;       /* not a basic regular expression */
    for (var rf : [["[&]", ""], ["[&]", "g"], ["<\\1|\\2>", ""]]) {
        var repl = rf[0], flags = rf[1];
        if ("\\2" in repl && count_groups(p) < 2)
            continue;
        for (var s : strs) {
            var want = sed_subst(s, p, repl, flags);
            if (want == nil)
                continue;
            n++;
            var got = ex_subst(s, p, repl, flags);
            if (got != want) {
                bad++;
                printf("DIFF s/%s/%s/%s on %q: sed %q ex %q\n", p, repl, flags, s, want, got);
            }
        }
    }
}
printf("%d cases, %d differ\n", n, bad);
