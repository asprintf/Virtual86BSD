/*
 * term.tl - the tests of more on a terminal: more on a pseudo-terminal of
 * 10 rows of 40 columns (TERM=xterm), keys typed, the screen looked at.
 *
 * usage: tl term.tl MORE     (in a directory with the files f30 and blanks)
 *
 * Writes FAIL lines, then "PASSES n FAILS m".  U8 in the environment names
 * a UTF-8 locale (the wide-character test is left out without one).
 */

var MORE = argv[0];
var U8 = getenv("U8");
if (U8 == nil)
    U8 = "";

/* more (or a shell command) on a new terminal, its first screen read */
func Run(args, opts) {
    if (opts == nil)
        opts = {};
    var env = {LINES: nil, COLUMNS: nil, MORE: nil, TERM: "xterm"};
    if (opts.env != nil)
        for (var k : opts.env)
            env[k] = opts.env[k];
    var cmd = opts.shell != nil ? opts.shell : [MORE] + args;
    var r = {t: term(cmd, {rows: opts.rows == nil ? 10 : opts.rows, cols: opts.cols == nil ? 40 : opts.cols, env: env}),
             mark: 0, status: nil};
    drain(r);
    return r;
}

func drain(r) {
    tsettle(r.t, 0.25, 3);
}

/* types k, then reads until the output settles */
func key(r, k) {
    r.mark = len(traw(r.t));
    tsend(r.t, k);
    drain(r);
    return r;
}

/* the output since the last key */
func since(r) {
    return substr(traw(r.t), r.mark);
}

func text(r, n) {
    var l = [];
    for (var i = 0; i < n; i++)
        push(l, tline(r.t, i));
    return l;
}

func screen(r) {
    return split(tscreen(r.t), "\n");
}

/* the line of the cursor */
func prompt(r) {
    return tline(r.t, trow(r.t));
}

/* waits for more to end: true if it did (its status in r.status) */
func done(r) {
    r.status = twait(r.t, 2);
    return r.status != nil;
}

func quit(r) {
    key(r, "q");
    done(r);
    tkill(r.t);
}

var passes = 0, fails = 0;
func check(name, cond, info) {
    if (cond)
        passes++;
    else {
        fails++;
        printf("FAIL %s %s\n", name, info == nil ? "" : repr(info));
    }
}

func L(a, b) {
    var l = [];
    for (var i = a; i <= b; i++)
        push(l, sprintf("l%02d", i));
    return l;
}

/* first screen: rows-1 lines and a prompt with the name */
var r = Run(["f30"]);
check("first screen", text(r, 9) == L(1, 9), screen(r));
check("prompt names the file", "f30" in prompt(r), prompt(r));
key(r, " ");
check("space: next screenful", text(r, 9) == L(10, 18), screen(r));
check("space scrolls (no clear)", !("\033[2J" in since(r)) && !("\033[H" in since(r)), substr(since(r), 0, 80));
key(r, "b");
check("b: back a screenful", text(r, 9) == L(1, 9), screen(r));
key(r, "j");
check("j: one line", text(r, 9) == L(2, 10), screen(r));
key(r, "k");
check("k: back one line", text(r, 9) == L(1, 9), screen(r));
key(r, "k");
check("k at the top: alert, no change", text(r, 9) == L(1, 9) && "\007" in since(r));
key(r, "3j");
check("3j", text(r, 9) == L(4, 12), screen(r));
key(r, "2k");
check("2k (reverse scroll)", text(r, 9) == L(2, 10), screen(r));
key(r, "15g");
check("15g", text(r, 9) == L(15, 23), screen(r));
key(r, "g");
check("g", text(r, 9) == L(1, 9), screen(r));
key(r, "G");
check("G: last screenful", text(r, 9) == L(22, 30), screen(r));
check("prompt at EOF", "END" in prompt(r), prompt(r));
quit(r);

r = Run(["f30"]);
key(r, "d");
check("d: half a screen", text(r, 9) == L(5, 13), screen(r));
key(r, "u");
check("u", text(r, 9) == L(1, 9), screen(r));
key(r, "2d");
check("2d", text(r, 9) == L(3, 11), screen(r));
key(r, "d");
check("d: count is the new default", text(r, 9) == L(5, 13), screen(r));
key(key(r, "g"), "s");
check("s: the screen count lines after the last", text(r, 9) == L(10, 18), screen(r));
key(key(r, "g"), "/l2\n");
check("/pattern", text(r, 9) == L(20, 28), screen(r));
key(r, "n");
check("n", text(r, 9) == L(21, 29), screen(r));
key(r, "N");
check("N", text(r, 9) == L(20, 28), screen(r));
key(r, "?l1\n");
check("?pattern", text(r, 9) == L(19, 27), screen(r));
key(key(r, "g"), "/!l0\n");
check("/!pattern", text(r, 9) == L(10, 18), screen(r));
key(key(r, "g"), "/zzz\n");
check("not found: message", "not found" in prompt(r), prompt(r));
key(r, "x");
check("not found: screen unchanged", text(r, 9) == L(1, 9), screen(r));
key(key(key(key(r, "12g"), "ma"), "g"), "'a");
check("mark and return", text(r, 9) == L(12, 20), screen(r));
key(key(key(r, "g"), "15g"), "''");
check("'' returns before the large move", text(r, 9) == L(1, 9), screen(r));
key(r, "=");
check("= shows the position", "f30" in prompt(r) && "line 10" in prompt(r), prompt(r));
key(key(r, "x"), "q");
check("q exits", done(r) && r.status == 0, r.status);
check("terminal restored", tattr(r.t).icanon);
tkill(r.t);

/* -n: lines per screenful, one of them the prompt */
r = Run(["-n", "5", "f30"]);
check("-n 5: 4 lines", text(r, 4) == L(1, 4), screen(r));
check("-n 5: prompt below them", trow(r.t) == 4, trow(r.t));
quit(r);

/* MORE and LINES */
r = Run(["f30"], {env: {MORE: "-n 4"}});
check("MORE=-n 4", text(r, 3) == L(1, 3) && trow(r.t) == 3, screen(r));
quit(r);
r = Run(["f30"], {env: {LINES: "6"}});
check("LINES=6", text(r, 5) == L(1, 5) && trow(r.t) == 5, screen(r));
quit(r);
r = Run(["-n", "4", "f30"], {env: {LINES: "6"}});
check("-n over LINES", text(r, 3) == L(1, 3) && trow(r.t) == 3, screen(r));
quit(r);

/* -e: exit after the last line of the last file */
r = Run(["-e", "blanks"]);
check("-e exits after a short file", done(r) && r.status == 0, r.status);
tkill(r.t);
r = Run(["-e", "f30"]);
key(r, "G");
check("-e exits at the end", done(r));
tkill(r.t);

/* the end of the last file: any other command exits */
r = Run(["blanks"]);
check("short file: END prompt", "END" in prompt(r), prompt(r));
key(r, "b");
check("at the end of the last file, b exits", done(r));
tkill(r.t);

/* two files */
writefile("two", "second\n");
r = Run(["blanks", "two"]);
check("EOF prompt names the next file", "two" in prompt(r) && "END" in prompt(r), prompt(r));
key(r, " ");
check("space goes to the next file", text(r, 1) == ["second"], screen(r));
tkill(r.t);
writefile("g30", readfile("f30"));
r = Run(["f30", "g30"]);
key(r, ":n");
check(":n", text(r, 1) == ["l01"] && "g30" in prompt(r), prompt(r));
key(r, ":p");
check(":p", "f30" in prompt(r), prompt(r));
key(r, "2:n");
check("2:n beyond the last: message", "no next" in prompt(r), prompt(r));
key(r, "x");
key(r, ":e blanks\n");
check(":e file", text(r, 1) == ["a"], screen(r));
tkill(r.t);
r = Run(["f30", "g30"]);
key(key(r, ":n"), ":e #\n");
check(":e #", "f30" in prompt(r), prompt(r));
key(key(r, "5g"), ":e nosuch\n");
check(":e inaccessible: message", "nosuch" in prompt(r), prompt(r));
key(r, "x");
check(":e inaccessible: file and screen unchanged", text(r, 9) == L(5, 13) && "f30" in prompt(r), screen(r));
quit(r);

/* a missing file among others: the status shows it */
r = Run(["nosuch", "two"]);
check("missing file skipped", text(r, 3)[2] == "second" || "second" in screen(r), screen(r));
key(r, "q");
check("missing file: status 1", done(r) && r.status == 1, r.status);
tkill(r.t);

/* characters */
writefile("chars", "a\001b\177c\n\377\nx\ty\nb\010bold\n_\010u\né日本\n" + repeat("q", 50) + "\nend\r\n");
r = Run(["chars"], {env: {LC_ALL: U8 != "" ? U8 : "C"}});
var t = screen(r);
check("control chars as ^X", t[0] == "a^Ab^?c", t[0]);
check("a byte that is not a character as \\ooo", t[1] == "\\377", t[1]);
check("tab to column 8", t[2] == "x       y", t[2]);
check("overstrike bold", t[3] == "bold" && tcell(r.t, 3, 0).bold, t[3]);
check("overstrike underline", t[4] == "u" && tcell(r.t, 4, 0).underline, t[4]);
if (U8 != "")
    check("wide characters", t[5] == "é日本", t[5]);
check("long line folded", t[6] == repeat("q", 40) && t[7] == repeat("q", 10), slice(t, 6, 8));
check("CR at the end ignored", t[8] == "end", t[8]);
quit(r);
r = Run(["-u", "chars"], {env: {LC_ALL: U8 != "" ? U8 : "C"}});
t = screen(r);
check("-u: backspace shown", t[3] == "b^Hbold", t[3]);
check("-u: CR shown", t[8] == "end^M", t[8]);
quit(r);

/* -s on a terminal */
r = Run(["-s", "blanks"]);
check("-s squeezes", text(r, 3) == ["a", "", "b"], screen(r));
quit(r);

/* a pipe that does not end is paged as it arrives */
r = Run([], {shell: "yes | \"$U\"", env: {U: MORE}});
check("yes | more: first screen", text(r, 9) == repeat(["y"], 9), screen(r));
key(r, " ");
check("yes | more: forward", text(r, 9) == repeat(["y"], 9) && !("END" in prompt(r)));
key(r, "q");
check("yes | more: q", done(r));
tkill(r.t);

/* -p and -t */
r = Run(["-p", "G", "f30"]);
check("-p G", text(r, 9) == L(22, 30), screen(r));
check("-p: no intermediate screen", !("l01" in traw(r.t)), substr(traw(r.t), 0, 60));
quit(r);
r = Run(["-p", "/l15\n", "f30"]);
check("-p with a pattern", text(r, 9) == L(15, 23), screen(r));
quit(r);
writefile("tags", "mytag\tf30\t/^l12$/\nnum\tf30\t20\n");
r = Run(["-t", "mytag"]);
check("-t pattern tag", text(r, 9) == L(12, 20), screen(r));
key(r, ":tnum\n");
check(":t line tag", text(r, 9) == L(20, 28), screen(r));
quit(r);
r = Run(["-t", "nosuchtag"]);
check("-t missing tag: error", done(r) && r.status == 1, r.status);
tkill(r.t);

/* v: the editor with -c line */
writefile("vi", "#!/bin/sh\necho \"$@\" > edargs\n", 0755);
r = Run(["f30"], {env: {EDITOR: abspath("vi")}});
key(key(r, "5g"), "v");
sleep(0.3);
var ea = readfile("edargs");
check("v runs vi -c line file", ea != nil && trim(ea) == "-c 5 f30", ea == nil ? "none" : ea);
check("v: same screen after", text(r, 9) == L(5, 13), screen(r));
quit(r);

/* h: help, then back */
r = Run(["f30"], {rows: 30, cols: 80});
key(r, "h");
check("h shows help", len(filter(screen(r), func (l) { return "search" in l; })) > 0, screen(r));
key(r, " ");
check("h: back to the file", text(r, 3) == L(1, 3), screen(r));
quit(r);

/* SIGWINCH: the new size is used (tresize sends it) */
r = Run(["f30"]);
tresize(r.t, 6, 40);
drain(r);
key(r, "r");
check("SIGWINCH: smaller screen", text(r, 5) == L(1, 5), screen(r));
quit(r);

/* SIGINT: default action, terminal restored first */
r = Run(["f30"]);
tsend(r.t, "\003");
done(r);
check("^C: killed by SIGINT", r.status != nil && tsignal(r.t) == SIGINT, r.status);
check("^C: terminal restored", tattr(r.t).echo);
tkill(r.t);

printf("PASSES %d FAILS %d\n", passes, fails);
exit(fails ? 1 : 0);
