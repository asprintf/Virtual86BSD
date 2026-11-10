/*
 * expect.tl - interactive programs on a pseudo-terminal, for the tests
 * (include "../../testlib/expect.tl").  The first failure of a test is
 * kept and the operations after it do nothing, so that a test reads
 * straight on.
 *
 *   session(cmd[, opts])   a program on a new terminal (cmd and opts: those
 *                          of term(); the terminal has no size unless opts
 *                          give one)
 *   expect(s, re[, secs])  waits (5 s) for the regular expression (POSIX ERE)
 *                          in what the program wrote after the last match;
 *                          a failure if it does not come
 *   send(s, text)          types text
 *   readfor(s, secs)       reads the output for secs seconds
 *   output(s)              all the output so far
 *   sendsig(s, sig)        sends the program a signal
 *   wait(s[, secs])        waits (5 s) for the program to end: its exit
 *                          status (128+n: signal n); a failure if it does not
 *                          end (it is killed)
 *   verify(cond, what)     a failure (what) if cond is false; cond
 *   fail(what)             a failure
 *   failed()               has the test failed?
 *   pty_tests(tests[, setup[, cleanup]])
 *                          runs the tests (functions): "ok   name" or "FAIL
 *                          name - what"; last "N passed, M failed"; the exit
 *                          status.  setup() makes what each test is given
 *                          (its argument; nil without), cleanup(x) removes it
 */

var _failure = nil, _sessions = [];

func fail(what) {
    if (_failure == nil)
        _failure = what;
    return false;
}

func failed() {
    return _failure != nil;
}

func verify(cond, what) {
    if (!cond)
        fail(what);
    return cond;
}

func session(cmd, opts) {
    if (opts == nil)
        opts = {};
    if (opts.rows == nil) {
        opts = copy(opts);
        opts.rows = 0;
        opts.cols = 0;
    }
    var s = {t: term(cmd, opts), pos: 0};
    push(_sessions, s);
    return s;
}

func expect(s, re, secs) {
    if (failed())
        return false;
    var end = now() + (secs == nil ? 5 : secs);
    for (;;) {
        var out = traw(s.t);
        var m = matchat(substr(out, s.pos), re);
        if (m != nil) {
            s.pos += m[1];
            return true;
        }
        if (now() >= end || (teof(s.t) && !talive(s.t)))
            break;
        tpump(s.t, 0.05);
    }
    var rest = substr(traw(s.t), s.pos);
    return fail(sprintf("expected %q, got %q", re, len(rest) > 400 ? slice(rest, -400) : rest));
}

func send(s, text) {
    if (!failed())
        tsend(s.t, text);
}

func readfor(s, secs) {
    tpump(s.t, secs);
}

func output(s) {
    return traw(s.t);
}

func sendsig(s, sig) {
    if (!failed())
        kill(tpid(s.t), sig);
}

func wait(s, secs) {
    if (failed())
        return nil;
    var st = twait(s.t, secs == nil ? 5 : secs);
    if (st == nil) {
        tkill(s.t);
        fail("the program did not end");
    }
    return st;
}

func pty_tests(tests, setup, cleanup) {
    var passed = 0, failures = 0;
    for (var f : tests) {
        _failure = nil;
        _sessions = [];
        var x = setup == nil ? nil : setup();
        if (setup == nil)
            f();
        else
            f(x);
        for (var s : _sessions)
            if (talive(s.t))
                tkill(s.t);
        if (cleanup != nil)
            cleanup(x);
        if (failed()) {
            failures++;
            printf("FAIL %s - %s\n", nameof(f), _failure);
        } else {
            passed++;
            printf("ok   %s\n", nameof(f));
        }
    }
    printf("%d passed, %d failed\n", passed, failures);
    return failures ? 1 : 0;
}
