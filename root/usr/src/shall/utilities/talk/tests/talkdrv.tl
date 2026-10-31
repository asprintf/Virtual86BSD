/*
 * talkdrv.tl - the scenarios of the talk tests (test_talk.sh runs them)
 *
 *     tl talkdrv.tl TALK SCENARIO [ARG...]
 *
 * Each talk runs on a pseudo-terminal of its own, with TERM=tt, an entry
 * that test_talk.sh writes (cup, clear, el as in ANSI), and the screen size
 * from LINES and COLUMNS.  What talk writes goes through the terminal
 * emulator of tl; a scenario prints what it found, one fact a line, which
 * test_talk.sh compares (strings and lists as repr() writes them, truth as
 * yes or no).  The environment (POSIX_UTMPX, POSIX_DEVDIR, POSIX_TALKDIR,
 * TERMINFO, TALK_ME) is test_talk.sh's.
 */

var TALK = abspath(argv[0]);
var ME = getenv("TALK_ME");
var TALKDIR = getenv("POSIX_TALKDIR"), DEVDIR = getenv("POSIX_DEVDIR");

func yn(b) {
    return b ? "yes" : "no";
}

/* a line of up to four words: strings as they are, other values by
   repr(); nil: no word */
func out(a, b, c, d) {
    var w = [];
    for (var v : [a, b, c, d])
        if (v != nil)
            push(w, typeof(v) == "string" ? v : repr(v));
    print(join(w, " "));
    flush();
}

/* talk on a new terminal; opts: rows, cols, iexten, stdin_tty, stdout_tty */
func Talk(args, opts) {
    if (opts == nil)
        opts = {};
    var rows = opts.rows == nil ? 10 : opts.rows, cols = opts.cols == nil ? 40 : opts.cols;
    var fds = "2", o = {rows: rows, cols: cols, env: {TERM: "tt", LINES: str(rows), COLUMNS: str(cols)},
             termios: {erase: "\177", kill: "\025", werase: "\027", eof: "\004", intr: "\003",
                       iexten: opts.iexten == nil ? true : opts.iexten}};
    if (opts.stdin_tty == false)
        o.in = "null";
    else
        fds += "0";
    if (opts.stdout_tty == false)
        o.out = "pipe";
    else
        fds += "1";
    o.fds = fds;
    return {t: term([TALK] + args, o), rows: rows, status: nil};
}

/* reads for secs seconds */
func pump(a, secs) {
    tpump(a.t, secs);
}

func type(a, s) {
    tsend(a.t, s);
}

func line(a, r) {
    return tline(a.t, r);
}

func text(a) {
    return tscreen(a.t);
}

/* waits up to secs (5) for pred(a), reading the output of the others too */
func wait(a, pred, secs, others) {
    var end = now() + (secs == nil ? 5 : secs);
    while (now() < end) {
        pump(a, 0.05);
        if (others != nil)
            for (var o : others)
                pump(o, 0);
        if (pred(a))
            return true;
    }
    return false;
}

/* waits for talk to end: its exit status, or "hung" (then it is killed) */
func finish(a) {
    var st = twait(a.t, 5);
    if (st == nil) {
        tkill(a.t);
        a.status = "hung";
    } else
        a.status = st;
    return a.status;
}

func status_line(a) {
    return line(a, (a.rows - 1) / 2);
}

func upper(a) {
    var l = [];
    for (var r = 0; r < (a.rows - 1) / 2; r++)
        push(l, line(a, r));
    return l;
}

func lower(a) {
    var l = [];
    for (var r = (a.rows - 1) / 2 + 1; r < a.rows; r++)
        push(l, line(a, r));
    return l;
}

/* the status line without the dashes that fill it */
func status_of(a) {
    return rtrim(status_line(a), "-");
}

/* the screen as one line: a message folded at the edge, whole again */
func message(a) {
    return replace(trim(text(a)), "\n", "");
}

func tty0() {
    return readfile(DEVDIR + "/pts/0");
}

func talkdir() {
    return listdir(TALKDIR);
}

func count(s, sub) {
    var c = 0;
    for (var i = index(s, sub); i >= 0; i = index(s, sub, i + 1))
        c++;
    return c;
}

func waiting(a) { return "[Waiting" in status_line(a); }
func connected(a) { return "[Connection" in status_line(a); }

/* A calls ME (on pts/0); B answers: both connected, or nil */
func connect_pair(opts) {
    var a = Talk([ME], opts);
    if (!wait(a, waiting)) {
        out("A did not wait:", text(a));
        return nil;
    }
    var b = Talk([ME], opts);
    var ok = wait(b, connected, nil, [a]);
    ok = ok && wait(a, connected, nil, [b]);
    if (!ok) {
        out("not connected:", text(a), text(b));
        return nil;
    }
    return [a, b];
}

/* the end of file typed to each: their exit statuses */
func end(ts) {
    for (var t : ts)
        type(t, "\004");
    return apply(ts, finish);
}

/* ---- the scenarios ---- */

func s_ring(args) {
    var a = Talk([ME]);
    wait(a, waiting);
    out("status:", status_of(a));
    var l = split(replace(tty0(), "\r", ""), "\n");
    out("first line empty:", yn(l[0] == ""));
    out("alert:", yn(startswith(l[1], "\007")));
    out("message:", sub(substr(l[1], 1), "talk@[^ ]+ at [0-9][0-9]:[0-9][0-9]", "talk@NODE at HH:MM"));
    out(replace(l[2], ME, "ME"));
    out(replace(l[3], ME, "ME"));
    out("socket:", yn(talkdir() == ["talk." + ME + "." + ME]));
    out("exit:", end([a]));
    out("socket removed:", yn(talkdir() == []));
}

func s_ring_again(args) {
    var a = Talk([ME]);
    wait(a, waiting);
    var n1 = count(tty0(), "connection requested");
    wait(a, func (t) { return "[Ringing" in status_line(t); }, 13);
    out("status:", status_of(a));
    out("rung:", str(n1), str(count(tty0(), "connection requested")));
    out("exit:", end([a]));
}

func s_connect(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    out("A:", status_of(a));
    out("B:", status_of(b));
    out("A alerted:", yn(tbells(a.t) >= 1));
    out("the socket gone:", yn(talkdir() == []));
    out("rung once:", str(count(tty0(), "connection requested")));
    out("exit:", end([a, b]));
}

func s_type(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "hello there\r");
    wait(b, func (t) { return "hello there" in lower(t)[0]; }, nil, [a]);
    type(a, "second");
    wait(b, func (t) { return "second" in lower(t)[1]; }, nil, [a]);
    pump(a, 0.2);
    out("A upper:", slice(upper(a), 0, 2));
    out("B lower:", slice(lower(b), 0, 2));
    out("B upper:", slice(upper(b), 0, 2));
    out("A cursor:", str(trow(a.t)), str(tcol(a.t)));
    type(b, "reply");
    wait(a, func (t) { return "reply" in lower(t)[0]; }, nil, [b]);
    out("A lower:", slice(lower(a), 0, 1));
    out("exit:", end([a, b]));
}

func s_edit(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "abcd\177\177x");
    wait(b, func (t) { return lower(t)[0] == "abx"; }, nil, [a]);
    out("erase:", repr(upper(a)[0]), repr(lower(b)[0]));
    type(a, "\025kill");
    wait(b, func (t) { return lower(t)[0] == "kill"; }, nil, [a]);
    out("kill:", repr(upper(a)[0]), repr(lower(b)[0]));
    type(a, " one two  \027");
    wait(b, func (t) { return lower(t)[0] == "kill one"; }, nil, [a]);
    pump(a, 0.2);
    out("werase:", repr(upper(a)[0]), repr(lower(b)[0]));
    type(a, repeat("\177", 12) + "Z");
    wait(b, func (t) { return lower(t)[0] == "Z"; }, nil, [a]);
    out("erase past the start:", repr(upper(a)[0]), repr(lower(b)[0]));
    out("exit:", end([a, b]));
}

func s_werase_noiexten(args) {
    var p = connect_pair({iexten: false});
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "one two\027.");
    wait(b, func (t) { return "." in lower(t)[0]; }, nil, [a]);
    out("no iexten:", repr(lower(b)[0]));
    out("exit:", end([a, b]));
}

func s_chars(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    var bells = tbells(b.t);
    type(a, "a\001b\033c\td\007\200.");
    wait(b, func (t) { return endswith(lower(t)[0], "."); }, nil, [a]);
    pump(a, 0.2);
    out("sent:", repr(lower(b)[0]));
    out("shown:", repr(upper(a)[0]));
    out("B alerted:", str(tbells(b.t) - bells));
    out("no escape reached B:", yn(!("\033c" in traw(b.t)) && !("\001" in traw(b.t))));
    out("exit:", end([a, b]));
}

func s_utf8(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "café 中.");
    wait(b, func (t) { return endswith(lower(t)[0], "."); }, nil, [a]);
    out("sent:", lower(b)[0]);
    out("exit:", end([a, b]));
}

func s_wrap(args) {
    var p = connect_pair({rows: 7, cols: 20});
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, repeat("x", 25));
    wait(b, func (t) { return lower(t)[1] == "xxxxx"; }, nil, [a]);
    out("wrapped:", slice(lower(b), 0, 2));
    type(a, "\r1\r2\r3");
    wait(b, func (t) { return lower(t)[1] == "3"; }, nil, [a]);
    pump(a, 0.2);
    out("again from the top:", lower(b));
    out("A:", upper(a));
    out("exit:", end([a, b]));
}

func s_redraw(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "mine");
    type(b, "theirs");
    wait(a, func (t) { return "theirs" in lower(t)[0]; }, nil, [b]);
    /* spoil the emulated screen, then ask for a redraw */
    tfill(a.t, "#");
    type(a, "\014");
    wait(a, func (t) { return !("#" in text(t)); });
    out("redrawn:", upper(a)[0], lower(a)[0], status_of(a));
    out("exit:", end([a, b]));
}

func s_leave(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "\004");
    out("A exit:", str(finish(a)));
    wait(b, func (t) { return "has left" in status_line(t); });
    out("B:", status_of(b));
    type(b, "ignored");
    pump(b, 0.3);
    out("B input ignored:", yn(!("ignored" in text(b))));
    out("B still running:", yn(talive(b.t)));
    type(b, "\004");
    out("B exit:", str(finish(b)));
}

func s_interrupt(args) {
    var p = connect_pair();
    if (p == nil)
        return;
    var a = p[0], b = p[1];
    type(a, "\003");
    out("A exit:", str(finish(a)));
    wait(b, func (t) { return "has left" in status_line(t); });
    out("B:", status_of(b));
    type(b, "\003");
    out("B exit:", str(finish(b)));
}

func s_interrupt_waiting(args) {
    var a = Talk([ME]);
    wait(a, waiting);
    type(a, "\003");
    out("exit:", str(finish(a)));
    out("socket removed:", yn(talkdir() == []));
    var b = Talk([ME]);
    out("B calls (no stale call):", yn(wait(b, waiting)));
    out("B exit:", end([b]));
}

func s_terminal_restored(args) {
    var a = Talk([ME]);
    wait(a, waiting);
    type(a, "\004");
    finish(a);
    /* the modes: talk had the only descriptor besides ours, so ask the master */
    var m = tattr(a.t);
    out("canonical again:", yn(m.icanon), "echo again:", yn(m.echo));
}

/* a connection that cannot show the proof is refused; the real one taken */
func s_fake_callee(args) {
    var a = Talk([ME]);
    wait(a, waiting);
    var fd = unixconnect(TALKDIR + "/talk." + ME + "." + ME);
    var l = fdread(fd, 100, 5);
    out("the challenge:", repr(gsub(l, "[0-9a-f]{32}", "N")));
    fdwrite(fd, "ok\n");
    out("refused:", repr(fdread(fd, 100, 5)));
    close(fd);
    pump(a, 0.3);
    out("A still waiting:", yn("Waiting for your party" in status_line(a)));
    var b = Talk([ME]);
    out("the real one connects:", yn(wait(b, connected, nil, [a])));
    out("exit:", end([a, b]));
}

/* both call at once (one name): the second answers the first */
func s_both(args) {
    var a = Talk([ME]);
    var b = Talk([ME]);
    var ok = wait(a, connected, 8, [b]);
    ok = ok && wait(b, connected, nil, [a]);
    out("connected:", yn(ok));
    out("exit:", end([a, b]));
}

func s_terminal_operand(args) {
    var a = Talk([ME, "pts/1"]);
    wait(a, waiting);
    out("pts/1 rung:", yn("connection requested" in readfile(DEVDIR + "/pts/1")));
    out("pts/0 not:", yn(!("connection requested" in tty0())));
    out("exit:", end([a]));
}

func s_not_tty_in(args) {
    var t = Talk([ME], {stdin_tty: false});
    out("exit:", str(finish(t)));
    out("message:", message(t));
}

func s_not_tty_out(args) {
    var t = Talk([ME], {stdout_tty: false});
    out("exit:", str(finish(t)));
    out("message:", trim(tpiped(t.t)));
}

func s_error(args) {
    var t = Talk(args);
    out("exit:", str(finish(t)));
    out("message:", replace(message(t), ME, "ME"));
}

var scenarios = {ring: s_ring, ring_again: s_ring_again, connect: s_connect, type: s_type, edit: s_edit,
                 werase_noiexten: s_werase_noiexten, chars: s_chars, utf8: s_utf8, wrap: s_wrap,
                 redraw: s_redraw, leave: s_leave, interrupt: s_interrupt,
                 interrupt_waiting: s_interrupt_waiting, terminal_restored: s_terminal_restored,
                 fake_callee: s_fake_callee, both: s_both, terminal_operand: s_terminal_operand,
                 not_tty_in: s_not_tty_in, not_tty_out: s_not_tty_out, error: s_error};
signal(SIGPIPE, "dfl");
scenarios[argv[1]](slice(argv, 2));
