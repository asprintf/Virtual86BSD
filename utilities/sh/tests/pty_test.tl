/*
 * pty_test.tl - interactive tests of sh through a pseudo-terminal
 *
 * usage: tl pty_test.tl
 *
 * Each check writes PASS or FAIL and its name (and the output, if it
 * failed); last "N passed, M failed".
 */

var SH = abspath(scriptdir + "/../sh");
var TMP = getenv("TMPDIR");
var PTYDIR = (TMP == nil || TMP == "" ? "/tmp" : TMP) + "/sh-pty-test";
mkdirs(PTYDIR);

/* sh -i on a new terminal (of no size); its output from buf(t) on */
func Term(args, env) {
    if (env == nil)
        env = {PS1: "$ ", HOME: PTYDIR, TERM: "dumb", ENV: ""};   /* no profile of the system */
    return {t: term([SH] + args, {rows: 0, cols: 0, env: env}), mark: 0};
}

/* the output since the mark */
func buf(t) {
    return substr(traw(t.t), t.mark);
}

/* forget the output so far */
func clear(t) {
    t.mark = len(traw(t.t));
}

func readfor(t, secs) {
    tpump(t.t, secs);
}

/* types s, then reads for wait seconds (0.4) */
func send(t, s, wait) {
    tsend(t.t, s);
    readfor(t, wait == nil ? 0.4 : wait);
}

/* waits up to secs (3) for the regular expression (POSIX ERE) in buf(t) */
func expect(t, re, secs) {
    var end = now() + (secs == nil ? 3 : secs);
    while (now() < end) {
        if (match(buf(t), re))
            return true;
        readfor(t, 0.1);
    }
    return match(buf(t), re) != nil;
}

func close_term(t) {
    tkill(t.t);
}

func count(s, sub) {
    var c = 0;
    for (var i = index(s, sub); i >= 0; i = index(s, sub, i + 1))
        c++;
    return c;
}

var passes = 0, fails = 0;
func check(name, cond, info) {
    if (cond)
        passes++;
    else
        fails++;
    printf("%s %s\n", cond ? "PASS" : "FAIL", name);
    if (!cond && info != nil)
        printf("   output: %q\n", len(info) > 600 ? slice(info, -600) : info);
}

func t_prompt() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "echo hello-$((1+1))\n");
    check("prompt and command", expect(t, "hello-2\r?\n\\$ "), buf(t));
    send(t, "PS1=\"[\\$PWD]> \"\n");
    send(t, "cd /\n");
    check("PS1 parameter expansion", expect(t, "\\[/\\]> "), buf(t));
    send(t, "PS1=\"!> \"\n");
    check("PS1 history number", expect(t, "\n[0-9]+> "), buf(t));
    send(t, "if true\n");
    check("PS2 continuation", expect(t, "> $"), buf(t));
    send(t, "then echo cont-ok; fi\n");
    check("continuation executes", expect(t, "cont-ok"), buf(t));
    close_term(t);
}

func t_sigint() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "sleep 10; echo not-printed\n", 0.5);
    send(t, "\003", 0.5);
    send(t, "echo alive-$?\n");
    check("SIGINT kills fg job, shell survives", expect(t, "alive-130") && count(buf(t), "not-printed") == 1, buf(t));
    send(t, "\003", 0.3);
    send(t, "echo still\n");
    check("SIGINT at prompt ignored", expect(t, "still"), buf(t));
    close_term(t);
}

func t_jobs() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "sleep 30 &\n");
    check("bg job message", expect(t, "\\[1\\] [0-9]+"), buf(t));
    send(t, "jobs\n");
    check("jobs listing", expect(t, "\\[1\\] \\+ Running[[:space:]]+sleep 30"), buf(t));
    send(t, "kill %1\n", 0.5);
    send(t, "\n", 0.5);
    check("done notification", expect(t, "\\[1\\] \\+ Terminated|\\[1\\].*Terminated"), buf(t));
    clear(t);
    send(t, "sleep 30\n", 0.6);
    send(t, "\032", 0.6);
    check("ctrl-z stops job", expect(t, "Stopped"), buf(t));
    send(t, "jobs\n");
    check("stopped job listed", expect(t, "\\[[0-9]\\] \\+ Stopped[[:space:]]+sleep 30"), buf(t));
    send(t, "bg\n");
    check("bg resumes", expect(t, "sleep 30 &"), buf(t));
    clear(t);
    send(t, "fg\n", 0.6);
    send(t, "\003", 0.6);
    send(t, "echo after-fg $?\n");
    check("fg brings to foreground", expect(t, "after-fg 130"), buf(t));
    close_term(t);
}

func t_history() {
    remove(PTYDIR + "/.sh_history");
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "echo first\n");
    send(t, "echo second\n");
    send(t, "fc -l\n");
    check("fc -l lists", expect(t, "[0-9]+\techo first\r?\n[0-9]+\techo second"), buf(t));
    clear(t);
    send(t, "fc -s first=again 1\n");
    check("fc -s substitution", expect(t, "again"), buf(t));
    clear(t);
    send(t, "fc -ln -1\n");
    check("fc -ln", expect(t, "\techo again"), buf(t));
    send(t, "exit\n", 0.5);
    close_term(t);
    check("history file written", exists(PTYDIR + "/.sh_history"));
}

func t_vi() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "set -o vi\n");
    clear(t);
    send(t, "echo abc def\0330wcwXYZ\033\r");
    check("vi cw", expect(t, "XYZ def"), buf(t));
    clear(t);
    send(t, "\033k\r");
    check("vi k recalls history", expect(t, "\r\nXYZ def\r\n", 3), buf(t));
    clear(t);
    send(t, "echo one two three\033bbdw\r");
    check("vi dw", expect(t, "one three"), buf(t));
    clear(t);
    send(t, "echo hello\033x$aX\033\r");
    check("vi x and a", expect(t, "hellX"), buf(t));
    clear(t);
    send(t, "echo aaa\033rb\r");
    check("vi r", expect(t, "aab"), buf(t));
    clear(t);
    send(t, "echo xyz\033hhDaQ\033\r");
    check("vi D", expect(t, "\r\nQ\r\n"), buf(t));
    clear(t);
    send(t, "echo word1 word2\0330fwi_\033;i-\033\r");
    check("vi f and ;", expect(t, "\r\n_-word1 word2\r\n"), buf(t));
    clear(t);
    send(t, "echo undo\033xu\r");
    check("vi u", expect(t, "\nundo"), buf(t));
    clear(t);
    send(t, "\033/second\r\r");
    send(t, "echo marker\r");
    clear(t);
    send(t, "echo abc\033xp\r");
    check("vi x p", expect(t, "abc|acb"), buf(t));
    clear(t);
    send(t, "ec\033Aho dot\0330dw.iecho \033\r");
    check("vi dot repeat", expect(t, "\r?\n?(dot|ho dot)"), buf(t));
    close_term(t);
}

func t_ignoreeof() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "set -o ignoreeof\n");
    send(t, "\004", 0.5);
    send(t, "echo still-alive\n");
    check("ignoreeof", expect(t, "still-alive"), buf(t));
    send(t, "exit\n");
    close_term(t);
}

func t_env() {
    writefile(PTYDIR + "/envfile", "ENV_LOADED=yes\nalias hi=\"echo hi-alias\"\n");
    var t = Term(["-i"], {PS1: "$ ", HOME: PTYDIR, ENV: PTYDIR + "/envfile"});
    readfor(t, 0.5);
    send(t, "echo $ENV_LOADED; hi\n");
    check("ENV file processed", expect(t, "yes") && expect(t, "hi-alias"), buf(t));
    close_term(t);
}

func t_errors_interactive() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "echo ${x?unset-error}\n");
    send(t, "echo after-error\n");
    check("expansion error does not exit interactive", expect(t, "after-error"), buf(t));
    send(t, "if then\n");
    send(t, "echo after-syntax\n");
    check("syntax error does not exit interactive", expect(t, "after-syntax"), buf(t));
    send(t, "readonly R=1; R=2\n");
    send(t, "echo after-ro\n");
    check("readonly error does not exit interactive", expect(t, "after-ro"), buf(t));
    close_term(t);
}

func t_notify() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "set -b\n");
    send(t, "sleep 1 &\n", 2.5);   /* POSIX sleep: whole seconds */
    check("set -b asynchronous notification", expect(t, "Done[[:space:]]+sleep 1"), buf(t));
    close_term(t);
}

func t_mail() {
    writefile(PTYDIR + "/mbox", "");
    var t = Term(["-i"], {PS1: "$ ", HOME: PTYDIR, MAIL: PTYDIR + "/mbox", MAILCHECK: "0", ENV: ""});
    readfor(t, 0.5);
    sleep(1.1);
    appendfile(PTYDIR + "/mbox", "From x\n");
    send(t, "\n", 0.5);
    check("MAIL notification", expect(t, "you have mail"), buf(t));
    close_term(t);
}

/* each ^D gives one message and the shell reads the terminal again */
func t_ignoreeof_repeat() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "set -o ignoreeof\n");
    for (var i = 0; i < 3; i++)
        send(t, "\004", 0.3);
    send(t, "echo after-three\n");
    var ok = expect(t, "after-three") && count(buf(t), "Use \"exit\" to leave the shell.") == 3;
    check("ignoreeof: one message per end-of-file", ok, buf(t));
    send(t, "exit\n");
    close_term(t);
}

/* with SIGHUP ignored, a terminal that goes away ends the shell instead
   of an endless series of end-of-files */
func t_ignoreeof_hangup() {
    var t = Term(["-i"]);
    readfor(t, 0.5);
    send(t, "trap '' HUP; set -o ignoreeof\n", 0.5);
    tclose(t.t);
    var done = false;
    for (var i = 0; i < 50 && !done; i++) {
        if (!talive(t.t))
            done = true;
        else
            sleep(0.1);
    }
    close_term(t);
    check("ignoreeof: hangup ends the shell", done);
}

for (var f : [t_notify, t_mail, t_prompt, t_sigint, t_jobs, t_history, t_vi, t_ignoreeof,
              t_ignoreeof_repeat, t_ignoreeof_hangup, t_env, t_errors_interactive])
    f();
printf("%d passed, %d failed\n", passes, fails);
exit(fails ? 1 : 0);
