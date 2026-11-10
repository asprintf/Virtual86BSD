/*
 * pty_tests.tl - interactive tests of mailx on a pseudo-terminal.
 *
 * usage: tl pty_tests.tl MAILX
 *
 * Each test starts mailx on a terminal in a new directory, types input and
 * waits for the output expected.  "ok" or "FAIL" for each test.
 */
include "../../../lib/testlib/testlib.tl";
include "../../../lib/testlib/expect.tl";

var MAILX = abspath(argv[0]);
var LOC = utf8_locale();
if (LOC == "")
    LOC = "C";

var FAKESEND = "#!/bin/sh\n{ echo \"== sendmail $*\"; cat; } >> \"$HOME/sent\"\n";

func mailx(args, home, env) {
    var e = {MAILRC: nil, MAIL: nil, MBOX: nil, DEAD: nil, PAGER: nil, EDITOR: nil,
             HOME: home, MAIL: home + "/sys", MAILRC: home + "/rc", TERM: "dumb", LC_ALL: LOC, TZ: "UTC"};
    if (env != nil)
        for (var k : env)
            e[k] = env[k];
    return session([MAILX] + args, {cwd: home, env: e});
}

func mkhome() {
    var home = mkdtemp("mailxpty.");
    writefile(home + "/fakesend", FAKESEND, 0755);
    writefile(home + "/rc", "set sendmail=" + home + "/fakesend sendwait quiet\n");
    return home;
}

func mkbox(home, n) {
    var box = "";
    for (var i = 1; i <= n; i++) {
        box += sprintf("From u%d@x Sat Oct 24 10:00:00 2026\nFrom: u%d@x\nSubject: subject %d\n\n", i, i, i);
        for (var k = 1; k <= 40; k++)
            box += sprintf("line %d of message %d\n", k, i);
        box += "\n";
    }
    writefile(home + "/sys", box);
}

/* a file's contents; "" if it is not there */
func content(path) {
    var t = readfile(path);
    return t == nil ? "" : t;
}

func sent(home) {
    return content(home + "/sent");
}

func rc(home, line) {
    appendfile(home + "/rc", line);
}

func count(s, sub) {
    var c = 0;
    for (var i = index(s, sub); i >= 0; i = index(s, sub, i + 1))
        c++;
    return c;
}

func send_subject_prompt(home) {
    var s = mailx(["a@b"], home);
    expect(s, "Subject: ", 3);
    send(s, "hello there\r");
    send(s, "body line\r");
    send(s, "\004");
    expect(s, "EOT", 3);
    verify(wait(s, 3) == 0, "status");
    var t = sent(home);
    verify("Subject: hello there" in t && "body line" in t, t);
}

func send_askcc_askbcc(home) {
    rc(home, "set askcc askbcc\n");
    var s = mailx(["-s", "x", "a@b"], home);
    send(s, "text\r");
    send(s, "\004");
    expect(s, "Cc: ", 3);
    send(s, "c@d\r");
    expect(s, "Bcc: ", 3);
    send(s, "e@f\r");
    verify(wait(s, 3) == 0, "status");
    var t = sent(home);
    verify("Cc: c@d" in t && "e@f" in split(t, "\n")[0] && !("Bcc" in t), t);
}

func send_two_interrupts(home) {
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "partial text\r");
    sleep(0.2);
    send(s, "\003");
    expect(s, "Interrupt -- one more to kill letter", 3);
    send(s, "\003");
    var st = wait(s, 3);
    verify(st != 0, sprintf("status %q", st));
    var dl = content(home + "/dead.letter");
    verify("partial text" in dl && startswith(dl, "From "), dl);
    verify(sent(home) == "", "sent");
}

func send_interrupt_then_typing(home) {
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "one\r");
    sleep(0.1);
    send(s, "\003");
    expect(s, "one more to kill letter", 3);
    send(s, "two\r");
    sleep(0.1);
    send(s, "\003");
    expect(s, "one more to kill letter", 3);
    send(s, "three\r");
    send(s, "\004");
    verify(wait(s, 3) == 0, "status");
    var t = sent(home);
    verify("one\ntwo\nthree" in t, t);
}

func send_ignore_at(home) {
    rc(home, "set ignore\n");
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "kept\r");
    send(s, "discar");
    sleep(0.1);
    send(s, "\003");
    expect(s, "@", 3);
    send(s, "after\r");
    send(s, "\004");
    verify(wait(s, 3) == 0, "status");
    var t = sent(home);
    verify("kept\nafter" in t && !("discar" in t), t);
}

func send_nosave(home) {
    rc(home, "unset save\n");
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "text\r");
    sleep(0.1);
    send(s, "\003");
    expect(s, "one more", 3);
    send(s, "\003");
    wait(s, 3);
    verify(!exists(home + "/dead.letter"), "dead.letter written");
}

func send_dot_and_ignoreeof(home) {
    rc(home, "set ignoreeof\n");
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "text\r");
    send(s, "\004");
    expect(s, "Use \".\" to terminate letter", 3);
    send(s, "more\r.\r");
    verify(wait(s, 3) == 0, "status");
    verify("text\nmore\n" in sent(home), sent(home));
}

func send_dot(home) {
    rc(home, "set dot\n");
    var s = mailx(["-s", "x", "a@b"], home);
    sleep(0.2);
    send(s, "text\r.\r");
    verify(wait(s, 3) == 0, "status");
    verify("text\n" in sent(home) && !("\n.\n" in sent(home)), sent(home));
}

func receive_prompt_and_interrupt(home) {
    mkbox(home, 3);
    var s = mailx([], home);
    expect(s, "3 messages 3 new", 3);
    expect(s, "\\? $", 3);
    send(s, "p 1\r");
    expect(s, "line 40 of message 1", 3);
    expect(s, "\\? ", 3);
    send(s, "pipe 2 \"sleep 5\"\r");
    sleep(0.4);
    send(s, "\003");
    expect(s, "\\? ", 2);
    send(s, "=\r");
    expect(s, "\r\n2\r\n", 3);
    send(s, "q\r");
    verify(wait(s, 3) == 0, "status");
}

func receive_reply_interrupted(home) {
    mkbox(home, 1);
    var s = mailx([], home);
    expect(s, "\\? ", 3);
    send(s, "r\r");
    expect(s, "To: u1@x", 3);
    send(s, "draft reply\r");
    sleep(0.1);
    send(s, "\003");
    expect(s, "one more", 3);
    send(s, "\003");
    expect(s, "\\? ", 3);
    send(s, "x\r");
    wait(s, 3);
    verify("draft reply" in content(home + "/dead.letter"), "dead.letter");
    verify(sent(home) == "", "sent");
}

func receive_no_prompt(home) {
    rc(home, "set noprompt\n");
    mkbox(home, 1);
    var s = mailx([], home);
    expect(s, "1 message", 3);
    readfor(s, 0.3);
    verify(!("? " in output(s)), output(s));
    send(s, "x\r");
    wait(s, 3);
}

func tilde_h_and_p(home) {
    var s = mailx(["-s", "subj", "a@b"], home);
    sleep(0.2);
    send(s, "body\r~h\r");
    expect(s, "To: ", 3);
    send(s, "\r");
    expect(s, "Subject: ", 3);
    send(s, "\r");
    expect(s, "Cc: ", 3);
    send(s, "cc@x\r");
    expect(s, "Bcc: ", 3);
    send(s, "\r");
    send(s, "~p\r");
    expect(s, "Message contains:", 3);
    expect(s, "Cc: ", 3);
    expect(s, "\\(continue\\)", 3);
    send(s, "\004");
    verify(wait(s, 3) == 0, "status");
    var t = sent(home);
    verify("cc@x" in t && "Subject: subj" in t && "To: a@b" in t, t);
}

func tilde_e_editor(home) {
    var ed = home + "/ed.sh";
    writefile(ed, "#!/bin/sh\necho \"edited line\" >> \"$1\"\n", 0755);
    var s = mailx(["-s", "x", "a@b"], home, {EDITOR: ed});
    sleep(0.2);
    send(s, "original\r~e\r");
    expect(s, "\\(continue\\)", 3);
    send(s, "\004");
    verify(wait(s, 3) == 0, "status");
    verify("original\nedited line\n" in sent(home), sent(home));
}

func pager_with_crt(home) {
    mkbox(home, 2);
    var pg = home + "/pg.sh";
    writefile(pg, "#!/bin/sh\necho \"[paged]\"; cat\n", 0755);
    rc(home, "set crt=10\n");
    var s = mailx([], home, {PAGER: pg});
    expect(s, "\\? ", 3);
    send(s, "p 1\r");
    expect(s, "\\[paged\\]", 3);
    expect(s, "line 40 of message 1", 3);
    send(s, "top 2\r");
    readfor(s, 0.3);
    send(s, "x\r");
    wait(s, 3);
    verify(count(output(s), "[paged]") == 1, output(s));
}

func edit_command(home) {
    mkbox(home, 1);
    var ed = home + "/ed.sh";
    writefile(ed, "#!/bin/sh\ngrep -c line \"$1\"; echo junk >> \"$1\"\n", 0755);
    var s = mailx([], home, {EDITOR: ed});
    expect(s, "\\? ", 3);
    send(s, "edit 1\r");
    expect(s, "40", 3);
    send(s, "x\r");
    wait(s, 3);
    verify(!("junk" in content(home + "/sys")), "the mailbox changed");
}

func shell_bang_tty(home) {
    mkbox(home, 1);
    var s = mailx([], home);
    expect(s, "\\? ", 3);
    send(s, "!echo hi from shell\r");
    expect(s, "hi from shell", 3);
    expect(s, "!", 3);
    send(s, "x\r");
    wait(s, 3);
}

exit(pty_tests([send_subject_prompt, send_askcc_askbcc, send_two_interrupts, send_interrupt_then_typing,
                send_ignore_at, send_nosave, send_dot_and_ignoreeof, send_dot, receive_prompt_and_interrupt,
                receive_reply_interrupted, receive_no_prompt, tilde_h_and_p, tilde_e_editor, pager_with_crt,
                edit_command, shell_bang_tty],
               mkhome, func (home) { rmtree(home); }));
