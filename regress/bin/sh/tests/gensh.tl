/*
 * gensh.tl - random shell programs for equiv.tl.
 *
 * program() builds a program as a list of tokens (so that mutate() can
 * delete, repeat, swap or insert tokens and make syntax errors anywhere)
 * and renders it.  Here-document bodies follow the next newline.  The
 * programs terminate: loops count, and background jobs print nothing.
 * The random numbers are tl's (srand() to seed them).
 */

var KEYWORDS = ["if", "then", "else", "elif", "fi", "do", "done", "case", "esac",
                "while", "until", "for", "in", "{", "}", "!"];
var OPS = [";", "&&", "||", "|", "(", ")", ";;", ";&", ">", "<", ">>", "2>&1", "<<", "\n"];
var NL = "\n";

/* a here-document operator and its body (a token that is not a string) */
func HD(op, delim, body) {
    return {op: op, delim: delim, body: body};
}

func ishd(t) {
    return typeof(t) == "map";
}

/* s with the first old replaced by new */
func replace1(s, old, new) {
    var i = index(s, old);
    return i < 0 ? s : substr(s, 0, i) + new + substr(s, i + len(old));
}

func p(x) {
    return frand() < x;
}

/* a generator: its depth and the names it uses */
func Gen(depth) {
    return {depth: depth, funcs: ["f1", "f2"], vars: ["v", "w", "n", "i"]};
}

/* ---------- words ---------- */

func g_word(g, d) {
    if (d == nil)
        d = 0;
    var r = frand();
    if (r < 0.30)
        return choice(["a", "b", "x", "hello", "1", "42", "foo=bar", "-n", "--", "%s",
                       "*.none", "a\\ b", "it''s", "z:y"]);
    if (r < 0.40)
        return choice(KEYWORDS);
    if (r < 0.50)
        return choice(["'single q'", "\"dq $v\"", "\"$w\"", "'if'", "\"fi\"", "\\if", "$'a\\tb'",
                       "\"a\\\"b\"", "''", "\"\""]);
    if (r < 0.60)
        return choice(["$v", "${w}", "${u:-def}", "${v:+alt}", "${#v}", "${v%a}", "${v#?}",
                       "$1", "$#", "$?", "\"$@\"", "${u-\"q d\"}", "$((n+1))", "$((i*2))"]);
    if (r < 0.70 && d < 2) {
        var inner = g_list_tokens(Gen(1), 1);
        if (p(0.5))
            return ["$("] + inner + [")"];
        var text = render(filter(inner, func (t) { return !ishd(t); }));
        text = replace(replace(text, "`", ""), "\\", "");
        return "`" + replace(text, NL, ";") + "`";
    }
    if (r < 0.75)
        return choice(g.funcs);
    return choice(["word", "x", "y", "p_q", "END", "EOF"]);
}

func flat(w) {
    return typeof(w) == "list" ? w : [w];
}

/* ---------- commands ---------- */

func g_redir(g) {
    var r = frand();
    if (r < 0.5)
        return [choice([">/dev/null", "2>/dev/null", "2>&1", ">>out", ">out", "<out", "<&-",
                        "3>&1", ">|out2", "<>rw", "1>&2"])];
    if (r < 0.65)
        return [choice([">", "<", ">>", "2>"]), choice(["out", "/dev/null", "f", "$v"])];
    var delim = choice(["EOF", "END", "'Q'", "\"D\"", "E\\F", "x"]);
    var op = choice(["<<", "<<-", "<<"]);
    var body = [];
    for (var k = rand(0, 3); k > 0; k--)
        push(body, choice(["line $v", "plain", "\tTabbed $w", "'quoted' \"dq\"", "back\\slash",
                           "$(echo sub)", "`echo bq`", "$((1+2))", "EOFX", "\\$v"]));
    return [HD(op, delim, body)];
}

func g_simple(g) {
    var t = [];
    while (p(0.25))
        push(t, choice(["v=1", "w=\"two words\"", "n=0", "x=$v", "u=", "v=$(echo cs)"]));
    if (p(0.1))
        t += g_redir(g);
    var name = choice(["echo", "echo", "echo", "printf", "true", "false", ":", "f1", "f2", "al1",
                       "al2", "eval", "cat", "set", "[", "alias", "unalias", "readonly", "export"]);
    if (name == "eval")
        t += ["eval", choice(["'echo ev'", "'if true; then echo et; fi'", "\"echo \\$v\"",
                              "'echo $((1+1))'", "'case a in a) echo c;; esac'", "'fi'",
                              "'echo $('", "'(echo s)'"])];
    else if (name == "printf")
        t += ["printf", "'%s|'"];
    else if (name == "[")
        t += ["[", choice(["\"$v\"", "1", "a"]), choice(["=", "-eq", "!="]), choice(["1", "a"]), "]"];
    else if (name == "alias")
        t += ["alias", choice(["al1='echo '", "al2='echo al2'", "al3=if", "al4='then echo t;'",
                               "al5='{'", "al1='printf %s-'", "fi=echo", "x='echo x '"])];
    else if (name == "cat")
        return t + ["cat"] + g_redir(g);
    else if (name == "set")
        return t + ["set", "--", "p1", choice(["p2", "\"p 3\""])];
    else
        push(t, name);
    for (var k = rand(0, 3); k > 0; k--)
        t += flat(g_word(g));
    if (p(0.2))
        t += g_redir(g);
    return t;
}

func g_compound(g, d) {
    var k = rand(0, 9), t;
    if (k == 0) {
        t = ["if"] + g_clist(g, d) + ["then"] + g_clist(g, d);
        while (p(0.3))
            t += ["elif"] + g_clist(g, d) + ["then"] + g_clist(g, d);
        if (p(0.5))
            t += ["else"] + g_clist(g, d);
        return t + ["fi"];
    }
    if (k == 1) {
        var v = choice(["i", "n"]);
        t = [v + "=0;", choice(["while", "until"])];
        t += p(0.5) ? ["[", "$((" + v + "+=1))", "-le", "2", "]"] : ["false"];
        return t + [choice([";", NL]), "do"] + g_clist(g, d) + ["done"];
    }
    if (k == 2) {
        t = ["for", choice(["i", "v", "x"])];
        if (p(0.8)) {
            push(t, choice(["in", NL + "in"]));
            for (var j = rand(0, 3); j > 0; j--)
                t += flat(g_word(g, 1));
            push(t, choice([";", NL, ";" + NL]));
        } else
            push(t, choice([";", NL, ""]));
        return t + ["do"] + g_clist(g, d) + ["done"];
    }
    if (k == 3) {
        t = ["case", choice(["a", "$v", "hello", "\"x y\"", "esac", "in"]), choice(["in", NL + "in"])];
        for (var j = rand(0, 3); j > 0; j--) {
            if (p(0.3))
                push(t, NL);
            var pats = [choice(["a", "*", "h*", "[ab]", "\"x y\"", "esac", "in", "?", "$v"])];
            while (p(0.3))
                pats += ["|", choice(["b", "c*", "x"])];
            if (p(0.3))
                pats = ["("] + pats;
            t += pats + [")"] + g_clist(g, d) + [choice([";;", ";;", ";&", ";;" + NL])];
        }
        if (p(0.3))
            t += ["z)"] + g_clist(g, d);
        return t + ["esac"];
    }
    if (k == 4 || k == 5)
        return ["{"] + g_clist(g, d) + [choice([";", NL]), "}"];
    if (k == 6)
        return ["("] + g_clist(g, d) + [")"];
    if (k == 7) {
        var name = choice(g.funcs);
        return [name, "()", choice(["", NL])] + g_compound(g, d);
    }
    if (k == 8)
        return g_simple(g) + ["|"] + g_simple(g);
    return ["!"] + g_simple(g);
}

func g_command(g, d) {
    if (d < g.depth && p(0.45)) {
        var t = g_compound(g, d + 1);
        if (p(0.15))
            t += g_redir(g);
        return t;
    }
    return g_simple(g);
}

func g_and_or(g, d) {
    var t = g_command(g, d);
    while (p(0.25))
        t += [choice(["&&", "||", "|", "&&" + NL, "|" + NL])] + g_command(g, d);
    if (p(0.05))
        t = ["!"] + t;
    return t;
}

func g_clist(g, d) {
    var t = [];
    for (var k = rand(1, 3); k > 0; k--) {
        t += g_and_or(g, d);
        push(t, choice([";", NL, ";", NL, NL + NL, "; " + NL]));
    }
    if (p(0.05))
        t += [":", "&"];
    return t;
}

func g_list_tokens(g, n) {
    var t = [];
    for (var k = 0; k < n; k++) {
        t += g_and_or(g, 0);
        push(t, choice([";", NL, NL]));
    }
    return len(t) && t[len(t) - 1] == ";" && p(0.5) ? slice(t, 0, len(t) - 1) : t;
}

/* Join tokens; here-document bodies go after the next newline. */
func render(tokens) {
    var out = "", pending = [];
    for (var t : tokens) {
        if (ishd(t)) {
            out += t.op + t.delim + " ";
            push(pending, t);
            continue;
        }
        if (NL in t) {
            out += t;
            for (var h : pending) {
                var d = replace(replace(replace(h.delim, "'", ""), "\"", ""), "\\", "");
                var pre = h.op == "<<-" ? "\t" : "";
                for (var l : h.body)
                    out += pre + l + NL;
                out += pre + d + NL;
            }
            pending = [];
            continue;
        }
        out += t + " ";
    }
    return out;
}

var ALIASES = ["alias al1='echo '", "alias al2='echo al2'", "alias kif='if'", "alias kthen='then echo kt;'",
               "alias kbr='{ echo br;'", "alias al3='al2 '", "alias fi='echo not-fi'", "alias e2=''",
               "alias rd='>/dev/null echo r'", "alias semi='echo s;'"];

func program() {
    var g = Gen(rand(1, 3));
    var head = "";
    if (frand() < 0.3) {
        // a sample of 1 to 3 different aliases
        var a = shuffle(copy(ALIASES));
        for (var x : slice(a, 0, rand(1, 3)))
            head += x + "\n";
    }
    var toks = g_list_tokens(g, rand(1, 4));
    if (frand() < 0.3)
        for (var i = 0; i < len(toks); i++) {
            var x = toks[i];
            if (!ishd(x) && x in ["echo", "if", "then"] && frand() <= 0.3)
                toks[i] = choice(["al1", "al2", "kif", "kthen", "kbr", "rd", "e2", "semi", "al3"]);
        }
    var body = render(toks);
    if (frand() < 0.15)
        body = replace1(body, " ", " \\\n");
    if (frand() < 0.15)
        body = replace1(body, NL, " # comment\n");
    return head + "f1() { echo in-f1 \"$@\"; }\nf2() ( echo in-f2 )\nv=val w=wv\n" + body + "\n";
}

var MUT_POOL = KEYWORDS + OPS + ["word", "$(", "`", "\"", "'", "${", "$((", "))", "f()", "{ ", "}"];

/* Token-level damage: delete, repeat, swap or insert tokens. */
func mutate(script) {
    var lines = split(script, "\n");
    var k = rand(len(lines));
    var toks = split(lines[k], " ");
    for (var m = rand(1, 3); m > 0; m--) {
        var op = rand(0, 3);
        var i = rand(len(toks));
        if (op == 0 && len(toks) > 1)
            pop(toks, i);
        else if (op == 1)
            insert(toks, i, toks[i]);
        else if (op == 2 && i + 1 < len(toks)) {
            var x = toks[i];
            toks[i] = toks[i + 1];
            toks[i + 1] = x;
        } else
            insert(toks, i, choice(MUT_POOL));
    }
    lines[k] = join(toks, " ");
    if (frand() < 0.2) {
        var cut = rand(len(script));
        return slice(join(lines, "\n"), 0, cut);
    }
    return join(lines, "\n");
}

/* ---------- arithmetic ---------- */

var AOPS = ["+", "-", "*", "/", "%", "<<", ">>", "<", "<=", ">", ">=", "==", "!=", "&", "^", "|",
            "&&", "||", ","];
var ASSIGN = ["=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|="];
var ANAMES = ["x", "y", "z", "h", "bad", "e", "u", "r", "n", "neg", "sp", "big", "oct"];

func aexpr(d) {
    if (d == nil)
        d = 0;
    var r = frand();
    if (d > 4 || r < 0.25)
        return frand() < 0.6 ?
            choice(["0", "1", "7", "42", "010", "0x1F", "0X10", "08", "0x", "1a", "00", "9",
                    "2147483648", "9223372036854775807", "9223372036854775808", "0xffffffffffffffff",
                    "0777", "1_", "_x"]) : choice(ANAMES);
    if (r < 0.55)
        return aexpr(d + 1) + choice(["", " ", "  ", "\n", "\t"]) + choice(AOPS) +
            choice(["", " "]) + aexpr(d + 1);
    if (r < 0.62)
        return choice(["-", "+", "!", "~", "- ", "--", "++", "+ +", "-+"]) + aexpr(d + 1);
    if (r < 0.70) {
        var v = choice(ANAMES);
        return choice([v + "++", v + "--", "++" + v, "--" + v, v + " ++", "++ " + v, v + "\n++"]);
    }
    if (r < 0.78)
        return choice(ANAMES) + choice(["", " "]) + choice(ASSIGN) + choice(["", " "]) + aexpr(d + 1);
    if (r < 0.86)
        return "(" + aexpr(d + 1) + choice([")", ")", ", " + aexpr(d + 1) + ")", ""]);
    if (r < 0.93)
        return aexpr(d + 1) + " ? " + aexpr(d + 1) + choice([" : ", ":", " ", " , "]) + aexpr(d + 1);
    return choice(["@", "$", " ", "", "))", "(", "#", ".", "1.5", "=", "?", ":", "x y", "3 4",
                   "1 ++ 2", "x+++y", "x---y", "+++x", "5++x", "5--x", "(x)++", "++(x)", "x++ ++",
                   "++x++", "x++=1", "1+=2", "!=3", "~-1", "x=y=3", "a?b:c=1", "1?2:x=3"]);
}

func arith_program() {
    var e = aexpr(0);
    if (frand() < 0.15)
        e = slice(e, 0, rand(len(e) + 1));
    var pre = "x=5 y=' 7 ' z=-3 h=0x10 bad=abc e= n=2 neg=-9223372036854775808 sp=' ' big=99999999999999999999 oct=011\n"
              "readonly r=4\nunset u\n";
    if (frand() < 0.1)
        pre += "set -u\n";
    var form = choice(["echo $((%s))", "echo \"$((%s))\"", ": $((%s)); echo ok", "echo $(( %s ))",
                       "eval 'echo $((%s))'", "v=$((%s)); echo \"$v\""]);
    return pre + replace(form, "%s", e) + "\necho \"x=$x y=$y z=$z u=$u n=$n h=$h e=$e\"\n";
}

/* ---------- scripts that read their own standard input ---------- */

var STDIN_CASES = [
    ["read a\necho \"got [$a]\"\n", "data line\n"],
    ["if true; then\nread a\nfi\necho \"[$a]\"\n", "more\n"],
    ["cat <<EOF\nhd\nEOF\nread b; echo \"b=$b\"\n", "after\n"],
    ["x=$(head -n 1); echo \"x=$x\"\n", "first\nsecond\n"],
    ["f() {\n read l\n echo \"f:$l\"\n}\nf\nf\n", "one\ntwo\n"],
    ["alias r=read\nr a\necho \"$a\"\n", "aliased\n"],
    ["while read l; do echo \"<$l>\"; done\n", "p\nq\n"],
    ["echo a && \\\nread z; echo \"z=$z\"\n", "zz\n"],
    ["case x in x) read q;; esac; echo \"q=$q\"\n", "qq\n"],
    ["{ read m; echo \"m=$m\"; }\n", "mm\n"],
    ["cat\n", "rest\nof\ninput\n"],
    ["head -n 1\necho tail\n", "h1\nh2\n"],
    ["echo $((1+\n2)); read t; echo \"t=$t\"\n", "tt\n"],
    ["for i in 1 2; do read v; echo \"$i:$v\"; done\n", "x1\nx2\n"],
    ["read a <<E\nfromdoc\nE\necho \"$a\"; read b; echo \"$b\"\n", "fromstdin\n"],
    ["echo \"multi\nline\"; read c; echo \"c=$c\"\n", "cc\n"],
    ["eval \"read e\"; echo \"e=$e\"\n", "ee\n"],
    ["(read s; echo \"s=$s\")\n", "ss\n"],
    ["echo `read k; echo k=$k`\n", "kk\n"],
    ["if read y\nthen echo \"y=$y\"\nfi\n", "yy\n"],
];

/* ---------- the error grid: every token in every place ---------- */

var GRID_TEMPLATES = [
    "if echo A ; then echo B ; fi",
    "if echo A ; then echo B ; elif echo C ; then echo D ; else echo E ; fi",
    "while false ; do echo B ; done ; echo W",
    "until true ; do echo B ; done",
    "for i in x y ; do echo $i ; done",
    "for i ; do echo $i ; done",
    "for i do echo $i ; done",
    "for i \n in x \n do echo $i \n done",
    "case w in a ) echo A ;; ( w | d ) echo W ;& f ) echo F ;; esac",
    "case w in esac",
    "case w \n in \n w ) echo A \n esac",
    "{ echo A ; echo B ; }",
    "( echo A ; echo B )",
    "f ( ) { echo F ; } ; f",
    "f ( ) \n ( echo F ) ; f",
    "! echo A | cat && echo C || echo D & wait ; echo F",
    "echo A > f 2>&1 < /dev/null ; cat f",
    "x=1 y=2 echo $x $y",
    "echo $( echo A ; echo B ) ` echo C `",
    "cat << E ; echo after \n body \n E",
    "echo A && \n echo B",
    "if ( true ) then echo B ; fi",
    "while { false ; } do echo B ; done ; echo W",
    "{ ( echo A ) }",
    "if true ; then { echo A ; } fi",
    "for i in a ; do ( echo $i ) done",
    "case a in a ) ( echo A ) esac",
    "echo $( if true ; then echo T ; fi ) $( case a in a ) echo C ;; esac )",
    "eval \"if true ; then echo E ; fi\"",
    "T echo A ; F",
    "X Y Z",
];
var GRID_ALIASES = "alias T='if true; then' F=fi D=do E=esac B='{' S=';' N='' X='echo ' Y='y ' Z=then\n";
var GRID_TOKENS = KEYWORDS + [";", "&", "|", "&&", "||", "(", ")", ";;", ";&", ">", "<<", "2>", "\n",
                              "x=1", "word", "f()", "()", "T", "F", "D", "E", "B", "S", "N", "X", "Z",
                              "$(", ")", "`", "{", "}", "#", "\\\n", "echo"];

func grid_show(ts) {
    return GRID_ALIASES + replace(join(ts, " "), " \n ", "\n") + "\n";
}

/* [name, script] for every edit of every template */
func grid() {
    var res = [];
    for (var ti = 0; ti < len(GRID_TEMPLATES); ti++) {
        var toks = filter(split(replace(GRID_TEMPLATES[ti], "\n", " \n "), " "),
                          func (t) { return t != ""; });
        var n = len(toks);
        push(res, [sprintf("%d/orig", ti), grid_show(toks)]);
        for (var i = 0; i <= n; i++) {
            push(res, [sprintf("%d/cut%d", ti, i),
                       GRID_ALIASES + replace(join(slice(toks, 0, i), " "), " \n ", "\n")]);
            if (i < n)
                push(res, [sprintf("%d/del%d", ti, i), grid_show(slice(toks, 0, i) + slice(toks, i + 1))]);
            for (var k = 0; k < len(GRID_TOKENS); k++) {
                var tok = GRID_TOKENS[k];
                push(res, [sprintf("%d/ins%d.%d", ti, i, k),
                           grid_show(slice(toks, 0, i) + [tok] + slice(toks, i))]);
                if (i < n)
                    push(res, [sprintf("%d/rep%d.%d", ti, i, k),
                               grid_show(slice(toks, 0, i) + [tok] + slice(toks, i + 1))]);
            }
        }
    }
    return res;
}
