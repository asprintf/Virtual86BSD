/*
 * pty_tests.tl - interactive and signal tests of ed on a pseudo-terminal.
 *
 * usage: tl pty_tests.tl ED
 */
include "../../../lib/testlib/testlib.tl";
include "../../../lib/testlib/expect.tl";

var ED = abspath(argv[0]);
var LOC = utf8_locale();
if (LOC == "")
    LOC = "C";

func ed(args, d) {
    return session([ED] + args, {cwd: d, env: {HOME: d, LC_ALL: LOC, TERM: "dumb"}});
}

func big(d, n) {
    var s = "";
    for (var i = 0; i < n; i++)
        s += sprintf("line %d of a long file\n", i);
    writefile(d + "/f", s);
}

func interrupt_printing(d) {
    big(d, 300000);
    var s = ed(["-p", "*", "f"], d);
    expect(s, "\\*");
    send(s, ",p\n");
    expect(s, "line 1000 ");
    sendsig(s, SIGINT);
    expect(s, "\\?\r?\n");
    send(s, ".=\n");
    expect(s, "[0-9]+\r?\n\\*");
    send(s, "Q\n");
    wait(s);
}

func interrupt_input(d) {
    writefile(d + "/f", "one\n");
    var s = ed(["-s", "f"], d);
    send(s, "=\n");    /* ed is ready: its signal handling set up */
    expect(s, "\n1\r?\n");
    send(s, "a\ntyped text\n");
    sleep(0.2);
    sendsig(s, SIGINT);
    expect(s, "\\?");
    send(s, ",p\n");
    expect(s, "one");
    send(s, "$=\n");
    expect(s, "\n1\r?\n");
    send(s, "Q\n");
    wait(s);
}

func interrupt_global(d) {
    big(d, 400000);
    var s = ed(["-s", "f"], d);
    send(s, "=\n");    /* the file is read */
    expect(s, "\n400000\r?\n");
    send(s, "g/line/s/a/A/\n");
    sleep(0.15);
    sendsig(s, SIGINT);
    expect(s, "\\?");
    send(s, "Q\n");
    wait(s);
}

func interrupt_at_prompt(d) {
    var s = ed(["-p", "> "], d);
    expect(s, "> ");
    sendsig(s, SIGINT);
    expect(s, "\\?\r?\n> ");
    send(s, "q\n");
    wait(s);
    verify(tsignal(s.t) == 0, "ended by a signal");
}

func hangup_saves(d) {
    writefile(d + "/f", "original\n");
    var s = ed(["-s", "f"], d);
    send(s, "a\nadded\n.\n");
    send(s, "$=\n");   /* the lines are in the buffer */
    expect(s, "\n2\r?\n");
    sendsig(s, SIGHUP);
    wait(s);
    var t = readfile(d + "/ed.hup");
    verify(t == "original\nadded\n", sprintf("ed.hup: %q", t));
    verify(readfile(d + "/f") == "original\n", "f changed");
}

func hangup_unmodified(d) {
    writefile(d + "/f", "original\n");
    var s = ed(["-s", "f"], d);
    send(s, "=\n");    /* ed is ready */
    expect(s, "\n1\r?\n");
    sendsig(s, SIGHUP);
    wait(s);
    verify(!exists(d + "/ed.hup"), "ed.hup written");
}

func hangup_home(d) {
    var sub = d + "/ro";
    mkdir(sub);
    writefile(sub + "/f", "x\n");
    chmod(sub, 0555);
    /* ed in the directory it cannot write */
    var s = session([ED, "-s", "f"], {cwd: sub, env: {HOME: d}});
    send(s, "=\n");    /* ed is ready */
    expect(s, "\n1\r?\n");
    send(s, "a\nmore\n.\n");
    send(s, "$=\n");   /* the lines are in the buffer */
    expect(s, "\n2\r?\n");
    sendsig(s, SIGHUP);
    wait(s);
    chmod(sub, 0755);
    if (geteuid() == 0)
        /* root can write anywhere: the file is in the working directory */
        verify(exists(sub + "/ed.hup"), "no ro/ed.hup");
    else
        verify(readfile(d + "/ed.hup") == "x\nmore\n", "$HOME/ed.hup");
}

func error_flushes_typeahead(d) {
    writefile(d + "/f", "one\ntwo\n");
    var s = ed(["-s", "f"], d);
    send(s, "=\n");    /* ed is ready */
    expect(s, "\n2\r?\n");
    /* an error, then input already typed: it is discarded */
    send(s, "nonsense\n1p\n");
    expect(s, "\\?");
    readfor(s, 0.3);
    verify(!("one" in output(s)), sprintf("the typed-ahead line was done: %q", output(s)));
    send(s, "2p\n");
    expect(s, "two");
    send(s, "Q\n");
    wait(s);
}

func quit_ignored(d) {
    writefile(d + "/f", "one\n");
    var s = ed(["-s", "f"], d);
    send(s, "=\n");    /* ed is ready */
    expect(s, "\n1\r?\n");
    sendsig(s, SIGQUIT);
    send(s, "p\n");
    expect(s, "one");
    send(s, "Q\n");
    var st = wait(s);
    verify(st == 0 && tsignal(s.t) == 0, sprintf("status %q", st));
}

func prompt_toggle(d) {
    var s = ed([], d);
    sleep(0.2);
    send(s, "P\n");
    expect(s, "\\*");
    send(s, "P\n");
    readfor(s, 0.3);
    send(s, "a\nx\n.\nP\n");
    expect(s, "\\*");
    send(s, "Q\n");
    wait(s);
}

func interactive_global(d) {
    writefile(d + "/f", "a1\nb\na2\na3\n");
    var s = ed(["-s", "f"], d);
    send(s, "G/a/\n");
    expect(s, "a1");
    send(s, "s/a/A/\n");
    expect(s, "a2");
    send(s, "&\n");
    expect(s, "a3");
    send(s, "\n");
    send(s, ",p\n");
    expect(s, "A1\r?\nb\r?\nA2\r?\na3");
    send(s, "Q\n");
    wait(s);
}

func interactive_global_interrupt(d) {
    writefile(d + "/f", "a1\na2\na3\n");
    var s = ed(["-s", "f"], d);
    send(s, "G/a/\n");
    expect(s, "a1");
    sendsig(s, SIGINT);
    expect(s, "\\?");
    send(s, ".=\n");
    expect(s, "\n1\r?\n");
    send(s, "Q\n");
    wait(s);
}

func shell_command(d) {
    writefile(d + "/f", "x\n");
    var s = ed(["f"], d);
    expect(s, "2\r?\n");
    send(s, "!echo hello\n");
    expect(s, "hello\r?\n!\r?\n");
    send(s, "!!\n");
    expect(s, "echo hello\r?\nhello\r?\n!");
    send(s, "Q\n");
    wait(s);
}

exit(pty_tests([interrupt_printing, interrupt_input, interrupt_global, interrupt_at_prompt, hangup_saves,
                hangup_unmodified, hangup_home, error_flushes_typeahead, quit_ignored, prompt_toggle,
                interactive_global, interactive_global_interrupt, shell_command],
               func () { return mkdtemp("edpty."); }, func (d) { rmtree(d); }));
