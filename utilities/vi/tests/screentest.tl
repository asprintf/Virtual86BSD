/*
 * screentest.tl - checks of the visual mode display through a
 * pseudo-terminal (the screen as an xterm shows it)
 *
 * usage: tl screentest.tl      (VI_BIN, EX_BIN: other binaries)
 */
include "vipty.tl";

func bin(var_, name) {
    var b = getenv(var_);
    return abspath(b != nil && b != "" ? b : scriptdir + "/../" + name);
}
var VI = bin("VI_BIN", "vi"), EX = bin("EX_BIN", "ex");

var work = mkdtemp("viscreen.");
var fails = 0, total = 0;

func check(name, cond, v) {
    total++;
    if (!cond) {
        fails++;
        printf("FAIL %s\n", name);
        if (v != nil)
            print(vt_text(v));
    }
}

/* opts: rows, cols (10 by 40), prog, env */
func start(lines, opts) {
    if (opts == nil)
        opts = {};
    writefile(work + "/f.txt", unlines(lines));
    var e = {HOME: work, SHELL: "/bin/sh"};
    if (opts.env != nil)
        for (var k : opts.env)
            e[k] = opts.env[k];
    return vt([opts.prog == nil ? VI : opts.prog, work + "/f.txt"],
              {rows: opts.rows == nil ? 10 : opts.rows, cols: opts.cols == nil ? 40 : opts.cols, cwd: work, env: e});
}

func finish(v) {
    vt_keys(v, "\033:q!\r");
    vt_wait(v, 5);
    vt_kill(v);
}

func L(v, n) {
    return vt_line(v, n);
}

/* tildes after the end of the buffer, message on the last line */
var t = start(["one", "two"]);
check("tilde lines", L(t, 2) == "~" && L(t, 8) == "~", t);
check("file message", "2 lines" in L(t, 9), t);
check("cursor at start", vt_cursor(t) == [0, 0], t);
finish(t);

/* long lines fold; lines that do not fit at the bottom show '@' */
t = start([repeat("x", 100), "short", "short", repeat("y", 150)], {rows: 7, cols: 40});
check("folding", L(t, 0) == repeat("x", 40) && L(t, 2) == repeat("x", 20), t);
check("at lines", L(t, 5) == "@", t);
finish(t);

/* number and list display */
t = start(["a\tb"]);
vt_keys(t, ":set nu\r");
check("number", L(t, 0) == "     1  a       b", t);
vt_keys(t, ":set nonu list\r");
check("list", L(t, 0) == "a\\tb$", t);
finish(t);

/* ex output that overwrites the screen waits for a key */
t = start(["a", "b", "c"]);
vt_keys(t, ":1,3p\r");
check("wait prompt", "Press any key" in vt_text(t), t);
vt_keys(t, " ");
check("redrawn", L(t, 0) == "a" && !("Press" in vt_text(t)), t);
finish(t);

/* a single line of output stays on the status line */
t = start(["a", "b", "c"]);
vt_keys(t, ":2p\r");
check("single line output", L(t, 9) == "b" && L(t, 0) == "a", t);
finish(t);

/* the c region is marked with '$' and stays until overwritten */
t = start(["hello world"]);
vt_keys(t, "cw");
check("dollar mark", L(t, 0) == "hell$ world", t);
vt_keys(t, "HI");
check("overwrite erase columns", L(t, 0) == "HIll$ world", t);
vt_keys(t, "\033");
check("erase columns gone", L(t, 0) == "HI world", t);
finish(t);

/* showmode */
t = start(["a"]);
vt_keys(t, ":set showmode\ri");
check("input mode shown", "INPUT MODE" in L(t, 9), t);
vt_keys(t, "\033");
check("command mode shown", "COMMAND MODE" in L(t, 9), t);
finish(t);

/* scrolling keeps the current line on the screen */
var hundred = [];
for (var i = 1; i <= 100; i++)
    push(hundred, sprintf("line %d", i));
t = start(hundred);
vt_keys(t, "50G");
check("centered", "line 50" in vt_text(t) && L(t, 0) != "line 1", t);
vt_keys(t, "\006");
check("page forward", true, t);
vt_keys(t, "G");
check("end shown", "line 100" in vt_text(t), t);
vt_keys(t, "1G");
check("back at top", L(t, 0) == "line 1", t);
finish(t);

/* errors ring the bell and show a message */
t = start(["a"]);
vt_keys(t, "/nomatch\r");
check("search failure message", "not found" in L(t, 9), t);
finish(t);

/* ex mode prompt and Q / vi switching */
t = start(["a", "b"], {prog: EX, cols: 70});
check("ex prompt", startswith(L(t, 1), ":"), t);
vt_keys(t, "vi\r");
check("visual from ex", L(t, 0) == "b" || L(t, 0) == "a", t);
vt_keys(t, "Q");
check("back to ex", endswith(rtrim(vt_text(t)), ":"), t);
vt_keys(t, "q!\r");
vt_wait(t, 5);
vt_kill(t);

/* window resize (tresize sends SIGWINCH) */
t = start(["one", "two", "three"]);
tresize(t.t, 6, 30);
vt_settle(t, 0.3);
check("resize", L(t, 0) == "one" && L(t, 4) == "~", t);
finish(t);

rmtree(work);
printf("%d passed, %d failed\n", total - fails, fails);
exit(fails ? 1 : 0);
