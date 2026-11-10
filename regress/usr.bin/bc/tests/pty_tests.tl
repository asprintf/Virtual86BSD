/*
 * pty_tests.tl - interactive tests of bc on a pseudo-terminal.
 *
 * usage: tl pty_tests.tl BC
 */
include "../../../lib/testlib/expect.tl";

var BC = abspath(argv[0]);

func immediate_output() {
    var s = session([BC]);
    send(s, "2+3\n");
    expect(s, "\n5\r?\n");
    send(s, "x = 4; x * x\n");
    expect(s, "\n16\r?\n");
    send(s, "quit\n");
    wait(s);
}

func interrupt_loop() {
    var s = session([BC]);
    send(s, "i = 0; while (1) { i = i + 1 }\n");
    sleep(0.5);
    sendsig(s, SIGINT);
    readfor(s, 0.5);
    send(s, "7 * 6\n");
    expect(s, "\n42\r?\n");
    send(s, "quit\n");
    wait(s);
    verify(tsignal(s.t) == 0, "ended by a signal");
}

func error_recovery() {
    var s = session([BC]);
    send(s, "1 +\n");
    readfor(s, 0.3);
    send(s, "1/0\n");
    readfor(s, 0.3);
    send(s, "8 + 1\n");
    expect(s, "\n9\r?\n");
    send(s, "quit\n");
    wait(s);
}

func eof_exits() {
    var s = session([BC]);
    send(s, "5\n");
    expect(s, "\n5\r?\n");
    send(s, "\004");
    wait(s);
    verify(tsignal(s.t) == 0, "ended by a signal");
}

func function_across_lines() {
    var s = session([BC, "-l"]);
    send(s, "define f(x) {\n");
    send(s, "  return (x * 10)\n");
    send(s, "}\n");
    send(s, "f(4); scale\n");
    expect(s, "\n40\r?\n20\r?\n");
    send(s, "quit\n");
    wait(s);
}

exit(pty_tests([immediate_output, interrupt_loop, error_recovery, eof_exits, function_across_lines]));
