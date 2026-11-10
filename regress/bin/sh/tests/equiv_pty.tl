/*
 * equiv_pty.tl - interactive equivalence: feed the same terminal sessions
 * to two builds of the shell and compare the transcripts byte for byte
 * (prompts included, which shows exactly when the parser asks for more
 * input).
 *
 *     tl equiv_pty.tl OLD NEW
 *
 * Both shells are run under one name (a symbolic link, written as "sh" in
 * the transcripts), so that argv[0] does not differ.
 */
include "quote.tl";

var SESSIONS = [
    ["echo hi\n", "if true\n", "then echo yes\n", "fi\n"],
    ["for i in 1 2\n", "do\n", "echo $i\n", "done\n"],
    ["echo a &&\n", "echo b\n"],
    ["echo a |\n", "\n", "cat\n"],
    ["case x in\n", "x) echo cx\n", ";;\n", "esac\n"],
    ["cat <<EOF\n", "body $HOME\n", "EOF\n", "echo after\n"],
    ["cat <<A; cat <<B\n", "one\n", "A\n", "two\n", "B\n"],
    ["echo \"multi\n", "line\"\n"],
    ["echo 'sq\n", "line'\n"],
    ["echo `echo bq\n", "`\n"],
    ["echo $(echo cs\n", "echo cs2)\n"],
    ["echo $((1 +\n", "2))\n"],
    ["echo cont\\\n", "inued\n"],
    ["f() {\n", "echo in-f\n", "}\n", "f\n"],
    ["f()\n", "\n", "{ echo nlbody; }\n", "f\n"],
    ["if true; then fi\n", "echo next\n"],
    ["echo ok; fi\n", "echo after-error\n"],
    ["{ echo a\n", ")\n", "echo recovered\n"],
    ["alias ll='echo aliased'\n", "ll x\n", "alias kw=if\n", "kw true; then echo kwok; fi\n"],
    ["alias a1='echo ' a2=word\n", "a1 a2\n"],
    ["while false\n", "do :\n", "done; echo w\n"],
    ["(\n", "echo sub\n", ")\n"],
    ["echo ${x:-\n", "def}\n"],
    ["for x\n", "in a b\n", "do echo $x; done\n"],
    ["case a in (a|b)\n", "echo pat;;\n", "esac\n"],
    ["! true\n", "echo $?\n"],
    ["echo \"unterminated\n", "\004"],
    ["if true\n", "\004"],
    ["cat <<E\n", "no end\n", "\004"],
    ["x=1 \\\n", "y=2; echo $x$y\n"],
    ["echo $((1/0))\n", "echo survived\n"],
    ["echo $((2+))\n", "echo $((x=3)) $x\n"],
    ["set -o vi\n", "echo vi\n"],
    ["PS2=\"cont> \"\n", "if true\n", "then echo ps2\n", "fi\n"],
    ["echo a; echo b &\n", "wait\n"],
    ["for i in 1; do\n", "case $i in\n", "1) echo one;;\n", "esac\n", "done\n"],
    ["echo `cat <<E\n", "in bq\n", "E\n", "`\n"],
    ["echo $(cat <<E\n", "in cs\n", "E\n", ")\n"],
    ["eval \"if true\n", "then echo ev; fi\"\n"],
    ["exit 3\n"],
];

/* the link through which the shells are run */
var BINDIR = mkdtemp("sh-equiv-bin.");
var SHLINK = BINDIR + "/sh";

func session(sh, inputs, wait) {
    if (wait == nil)
        wait = 0.35;
    remove(SHLINK);
    symlink(abspath(sh), SHLINK);
    var t = term([SHLINK, "-i"], {rows: 0, cols: 0,
                 env: {PS1: "$ ", PS2: "> ", HOME: "/tmp", TERM: "dumb", ENV: "", LC_ALL: "C",
                       HISTFILE: nil}});
    tpump(t, 0.4);
    for (var s : inputs) {
        tsend(t, s);
        tpump(t, wait);
        if (teof(t))
            break;
    }
    tpump(t, 0.2);
    tkill(t);
    tclose(t);
    var out = replace(traw(t), SHLINK, "sh");
    return gsub(out, "\\[[0-9]+\\] [0-9]+", "[N] PID");
}

/* the session as a list of quoted strings; ^D was given as bytes */
func showinputs(inp) {
    return "[" + join(apply(inp, func (s) { return s == "\004" ? quotedbytes(s) : quoted(s); }), ", ") + "]";
}

if (len(argv) != 2) {
    eprintf("usage: tl equiv_pty.tl OLD NEW\n");
    exit(2);
}
var old = argv[0], new = argv[1];
var diffs = 0;
for (var inp : SESSIONS) {
    var a = session(old, inp);
    var b = session(new, inp);
    if (a != b) {
        // a session that differs is run again: timing on a loaded
        // machine can move a prompt
        var same = false;
        for (var k = 0; k < 2 && !same; k++)
            same = session(old, inp) == session(new, inp);
        if (same)
            continue;
        diffs++;
        print("DIFF", showinputs(inp));
        print("  old:", quotedbytes(a));
        print("  new:", quotedbytes(b));
    }
}
printf("pty: %d sessions, %d differences\n", len(SESSIONS), diffs);
rmtree(BINDIR);
exit(diffs ? 1 : 0);
