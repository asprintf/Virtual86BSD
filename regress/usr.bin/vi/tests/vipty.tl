/*
 * vipty.tl - a screen editor on a pseudo-terminal, and the case files of
 * the vi tests (include "vipty.tl")
 *
 *   vt(argv[, opts])     the editor on a new terminal (TERM=xterm, LC_ALL=C,
 *                        24 by 80 unless opts give rows and cols; opts env
 *                        and cwd as for term()); its first screen read
 *   vt_settle(v[, quiet[, max]])   reads until the output settles
 *   vt_send(v, s)        types s in pieces of 64 bytes
 *   vt_keys(v, s)        types s, with a pause after each ESC (so that it is
 *                        not taken for the start of a sequence)
 *   vt_line(v, n), vt_text(v), vt_cursor(v): the screen ([row, col])
 *   vt_wait(v[, secs])   waits for the editor to end: its exit status, or
 *                        nil (it is killed)
 *   vt_kill(v)
 *   keys_decode(s)       <ESC> <CR> <NL> <TAB> <BS> <SP> <NUL> <DEL> and ^X
 *                        in a case's keys as the characters
 *   parse_cases(path, second)  the cases of a file: "#### NAME", the lines
 *                        of the file, "----", the lines of second (keys or
 *                        cmds): a list of {name, text, <second>}
 */

func vt(argv, opts) {
    if (opts == nil)
        opts = {};
    var env = {TERM: "xterm", LC_ALL: "C", LINES: nil, COLUMNS: nil, EXINIT: nil, VIMINIT: nil, NEXINIT: nil};
    if (opts.env != nil)
        for (var k : opts.env)
            env[k] = opts.env[k];
    var o = {rows: opts.rows == nil ? 24 : opts.rows, cols: opts.cols == nil ? 80 : opts.cols, env: env};
    if (opts.cwd != nil)
        o.cwd = opts.cwd;
    var v = {t: term(argv, o)};
    vt_settle(v, 0.5);
    return v;
}

func vt_settle(v, quiet, max) {
    tsettle(v.t, quiet == nil ? 0.15 : quiet, max == nil ? 5 : max);
}

func vt_send(v, s) {
    for (var i = 0; i < len(s); i += 64) {
        tsend(v.t, substr(s, i, 64));
        sleep(0.01);
    }
    vt_settle(v, 0.12);
}

func vt_keys(v, s) {
    var b = "";
    for (var i = 0; i < len(s); i++) {
        b += chr(s[i]);
        if (s[i] == 033) {
            tsend(v.t, b);
            b = "";
            sleep(0.06);
        }
    }
    if (b != "")
        tsend(v.t, b);
    vt_settle(v, 0.12);
}

func vt_line(v, n) {
    return tline(v.t, n);
}

func vt_text(v) {
    return tscreen(v.t);
}

func vt_cursor(v) {
    return [trow(v.t), tcol(v.t)];
}

func vt_wait(v, secs) {
    var end = now() + (secs == nil ? 5 : secs);
    while (now() < end) {
        var st = twait(v.t, 0.2);
        if (st != nil)
            return st;
    }
    tkill(v.t);
    return nil;
}

func vt_kill(v) {
    tkill(v.t);
}

func keys_decode(s) {
    var rep = [["<ESC>", "\033"], ["<CR>", "\r"], ["<NL>", "\n"], ["<TAB>", "\t"], ["<BS>", "\b"],
               ["<SP>", " "], ["<NUL>", "\0"], ["<DEL>", "\177"]];
    for (var r : rep)
        s = replace(s, r[0], r[1]);
    var out = "";
    for (var i = 0; i < len(s); i++) {
        if (s[i] == '^' && i + 1 < len(s) && index("ABCDEFGHIJKLMNOPQRSTUVWXYZ@[]^_\\", chr(s[i + 1])) >= 0) {
            out += chr(s[i + 1] - 64);
            i++;
        } else
            out += chr(s[i]);
    }
    return out;
}

func parse_cases(path, second) {
    var cases = [], cur = nil;
    var ls = split(readfile(path), "\n");
    if (len(ls) && ls[len(ls) - 1] == "")
        pop(ls);
    for (var l : ls) {
        if (startswith(l, "#### ")) {
            cur = {name: trim(substr(l, 5)), text: [], part: 0};
            cur[second] = [];
            push(cases, cur);
        } else if (cur != nil) {
            if (l == "----" && cur.part == 0)
                cur.part = 1;
            else if (cur.part == 0)
                push(cur.text, l);
            else
                push(cur[second], l);
        }
    }
    return cases;
}

/* the lines, each with its newline */
func unlines(l) {
    var s = "";
    for (var x : l)
        s += x + "\n";
    return s;
}

/* bytes as the characters of ISO 8859-1, in UTF-8 (byte n is U+00nn) */
func latin1(s) {
    if (!match(s, "[\x80-\xff]"))
        return s;
    var out = "";
    for (var i = 0; i < len(s); i++)
        out += s[i] < 0x80 ? chr(s[i]) : utf8(s[i]);
    return out;
}
