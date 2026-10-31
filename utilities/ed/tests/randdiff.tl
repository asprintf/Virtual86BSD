/*
 * randdiff.tl - random differential testing against GNU ed: random files
 * and scripts made of commands whose behaviour both implementations share.
 *
 * usage: tl randdiff.tl ED SEED COUNT [undo]
 *
 * GNU ed forgets the undo after a failed command, so u is left out unless
 * the fourth argument is "undo".
 */

if (len(argv) < 3) {
    eprintf("usage: tl randdiff.tl ED SEED COUNT [undo]\n");
    exit(2);
}
var ED = abspath(argv[0]);
var REF = "/usr/bin/ed";
srand(int(argv[1]));
var COUNT = int(argv[2]);
var UNDO = len(argv) > 3 && argv[3] == "undo";

var WORDS = ["a", "b", "ab", "ba", "abc", "o", "oy", "foo", "bar", "a b", "b a", "", "$", "\\", ".", "a.b", "A", "B"];
var RES = ["a", "b", "ab", "^a", "b$", "a.", "[ab]", "[^a]", "oy", "foo", "o\\{2\\}", "\\(a\\)\\(b\\)", "\\(.\\)\\1",
           "a\\|b", " ", "a*b", "b[a-z]", "^.", ".$", "\\.", "\\$", "A", "[[:upper:]]", "oo*", "o"];
var REPL = ["X", "", "&", "[&]", "\\1", "\\2\\1", "\\&", "y&y", "a\\\nb", "%"];

var seen_u = 0;     /* the script has a u (then e is left out) */
var MARKS = 0;      /* marks a and b are set (the file is not empty) */

/* a random character of the string s */
func pick(s) {
    return chr(s[rand(len(s))]);
}

func addr(n) {
    var k = frand();
    if (k < 0.3)
        return str(rand(0, n + 1));
    if (k < 0.4)
        return ".";
    if (k < 0.5)
        return "$";
    if (k < 0.6)
        return choice(["+", "-", "+2", "-1", ".+1", ".-1", "$-1", "1+1"]);
    if (k < 0.7)
        return "/" + choice(RES) + "/";
    if (k < 0.75)
        return "?" + choice(RES) + "?";
    if (k < 0.8 && MARKS)
        return "'" + pick("ab");
    return "";
}

func addrs(n) {
    var k = frand();
    if (k < 0.35)
        return "";
    if (k < 0.65)
        return addr(n);
    var sep = choice([",", ";"]);
    return addr(n) + sep + addr(n);
}

func text() {
    var s = "";
    for (var i = rand(0, 3); i > 0; i--)
        s += choice(WORDS) + "\n";
    return s + ".\n";
}

func command(n, in_global) {
    var k = rand(22);
    var a = addrs(n);
    var suf = choice(["", "", "", "p", "n", "l"]);
    if (k == 0)
        return a + pick("pnl") + "\n";
    if (k == 1)
        return a + "d" + suf + "\n";
    if (k == 2 && !in_global)
        return a + pick("ai") + suf + "\n" + text();
    if (k == 3 && !in_global)
        return a + "c" + suf + "\n" + text();
    if (k == 4)
        return a + "j" + suf + "\n";
    if (k == 5)
        return a + pick("mt") + addr(n) + suf + "\n";
    if (k == 6 || k == 7 || k == 8) {
        var re = choice(RES);
        var rep = choice(REPL);
        if ("\\" in rep && len(rep) > 1 && rep[1] >= '0' && rep[1] <= '9' && !("\\(" in re))
            rep = "&";
        if (in_global && "\n" in rep)
            rep = "Z";
        var fl = choice(["", "g", "2", "p", "gp", "n", "l", "3"]);
        if (in_global)
            /* GNU ed prints the line for these flags even when nothing was substituted */
            fl = replace(replace(replace(fl, "p", ""), "n", ""), "l", "");
        if ("g" in fl && re == "a*b")
            fl = replace(fl, "g", "");
        return a + sprintf("s/%s/%s/%s\n", re, rep, fl);
    }
    if (k == 9 && MARKS)
        return a + "k" + pick("ab") + "\n";
    if (k == 10 && !in_global && UNDO) {
        seen_u = 1;
        return "u" + suf + "\n";
    }
    if (k == 11 && !in_global) {
        var re = choice(RES);
        var cmd = rtrim(command(n, 1), "\n");
        if ("\n" in cmd)
            cmd = "p";
        return a + pick("gv") + sprintf("/%s/%s\n", re, cmd);
    }
    if (k == 12)
        return a + "=\n";
    if (k == 13)
        return a + "\n";
    if (k == 14 && !in_global)
        return a + "w out\n";
    if (k == 15 && !in_global)
        return a + "r f\n";
    if (k == 16 && !in_global)
        return "w\n";
    if (k == 17 && !in_global)
        return a + sprintf("s/%s/%s\n", choice(RES), choice(["Q", "R", ""]));
    if (k == 18 && !in_global)
        return a + sprintf("g/%s/s/%s/%s/\\\n.p\n", choice(RES), choice(RES), choice(["1", "2", "&&"]));
    if (k == 19)
        return "/" + choice(RES) + "\n";
    if (k == 20 && !in_global && !seen_u)
        return "e\nE\n";
    return a + "p\n";
}

/* {out, status, files}: the files of d after the script */
func runed(ed, d, script) {
    var r = run([ed, "-s", "f"], {in: script, cwd: d, env: {LC_ALL: "C"}, timeout: 20});
    var files = {};
    for (var name : listdir(d))
        files[name] = readfile(d + "/" + name);
    return {out: r.out, status: r.status, files: files};
}

var bad = 0, it = 0;
var base = mkdtemp("edrand.");
for (it = 0; it < COUNT; it++) {
    var n = rand(0, 8);
    var content = "";
    for (var i = 0; i < n; i++)
        content += choice(WORDS) + "\n";
    seen_u = 0;
    /* marks are set first: GNU ed treats an unset mark as line 0 in an empty buffer */
    MARKS = n > 0;
    var script = n ? "1ka\n$kb\n" : "";
    for (var i = rand(1, 15); i > 0; i--)
        script += command(n, 0);
    script += "Q\n";
    var res = [];
    for (var ed : [ED, REF]) {
        var d = base + "/x";
        rmtree(d);
        mkdir(d);
        writefile(d + "/f", content);
        push(res, runed(ed, d, script));
    }
    if (res[0] != res[1]) {
        bad++;
        printf("=== DIFF %d\n", it);
        printf("file: %q\n", content);
        printf("script: %q\n", script);
        printf("mine: %q %d\n", substr(res[0].out, 0, 600), res[0].status);
        printf("ref:  %q %d\n", substr(res[1].out, 0, 600), res[1].status);
        if (res[0].files != res[1].files)
            printf("files differ %q %q\n", res[0].files, res[1].files);
        if (bad > 8) {
            it++;
            break;
        }
    }
}
rmtree(base);
printf("done, %d differences in %d\n", bad, it);
