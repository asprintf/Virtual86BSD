/*
 * randlex.tl - random differential testing of lex against flex -X.
 *
 * usage: tl randlex.tl LEX SEED COUNT [--keep DIR] [-v]
 *
 * Each round makes a random lex source: patterns built from characters,
 * escapes, quoted strings, bracket expressions, '.', grouping,
 * alternation, * + ? and intervals, with ^, $, trailing context and start
 * conditions (inclusive and exclusive), and actions that print the rule
 * and yytext and sometimes use BEGIN, REJECT, yymore, yyless, unput or
 * input.  Both scanners run on random inputs and on texts made from the
 * patterns; the outputs must be the same.
 *
 * Trailing context is generated only where the split of the match is
 * determined (a fixed-length part, or r and x over disjoint characters),
 * since the standard leaves the other cases unspecified.
 */
include "../../testlib/testlib.tl";

var lex = abspath(argv[0]);
srand(int(argv[1]));
var count = int(argv[2]);
var k = find(argv, "--keep");
var keep = k >= 0 ? argv[k + 1] : nil;
var verbose = "-v" in argv;
var libdir = dirname(lex);
var ccenv = cc_env();
var scanflags = getenv("SCANFLAGS");

/*
 * A pattern: {text, smp, fixed, nullable, simple}.  fixed is its length
 * if that is fixed, else nil; simple: flex recognizes the fixed length (no
 * alternation or repetition).  smp describes the texts it matches, for
 * sample(): {k: "one", of: [chars]}, {k: "str", s}, {k: "cat", a, b},
 * {k: "alt", a, b}, {k: "rep", a, lo, hi}.
 */
func pat(text, smp, fixed, nullable, simple) {
    return {text: text, smp: smp, fixed: fixed, nullable: nullable, simple: simple && fixed != nil};
}

/* a random text matched by the pattern of the sample description s */
func sample(s) {
    if (s.k == "one")
        return choice(s.of);
    if (s.k == "str")
        return s.s;
    if (s.k == "cat")
        return sample(s.a) + sample(s.b);
    if (s.k == "alt")
        return sample(choice([s.a, s.b]));
    var t = "";
    for (var n = rand(s.lo, s.hi); n > 0; n--)
        t += sample(s.a);
    return t;
}

/* k different elements of l, in random order */
func pick(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

func lit(chars) {
    var c = choice(chars);
    // (not \x0a: a hex escape takes all the hex digits that follow, as the
    // standard says, where flex stops after two)
    var t;
    if (c == "\n")
        t = choice(["\\n", "\"\\n\"", "\\012"]);
    else if (c == " ")
        t = choice(["\" \"", "\\ ", "[ ]"]);
    else
        t = choice([c, c, "\"" + c + "\"", index("abfnrtvx0123456789", c) < 0 ? "\\" + c : c, "[" + c + "]"]);
    return pat(t, {k: "one", of: [c]}, 1, false, true);
}

var RESTRICTED = false;   // only the given characters: no '.' or [^...]

func bracket(chars) {
    var s = pick(chars, rand(1, len(chars)));
    var neg = frand() < 0.25 && !RESTRICTED;
    var body = join(apply(s, func (c) { return c == "\n" ? "\\n" : c; }), "");
    if (neg) {
        var allc = [];
        for (var c : ["a", "b", "c", "x", "\n"])
            if (!(c in s))
                push(allc, c);
        if (!len(allc))
            allc = ["x"];
        return pat("[^" + body + "]", {k: "one", of: allc}, 1, false, true);
    }
    return pat("[" + body + "]", {k: "one", of: s}, 1, false, true);
}

func rep(a, lo, hi) {
    return {k: "rep", a: a.smp, lo: lo, hi: hi};
}

func gen(chars, depth) {
    var r = frand();
    if (depth > 3 || r < 0.3)
        return lit(chars);
    if (r < 0.4)
        return bracket(chars);
    if (r < 0.45 && !("\n" in chars) && !RESTRICTED)
        return pat(".", {k: "one", of: len(chars) ? chars : ["a"]}, 1, false, true);
    if (r < 0.55) {
        var nonl = filter(chars, func (c) { return c != "\n"; });
        if (!len(nonl))
            nonl = ["a"];
        var s = "";
        for (var n = rand(1, 3); n > 0; n--)
            s += choice(nonl);
        return pat("\"" + s + "\"", {k: "str", s: s}, len(s), false, true);
    }
    if (r < 0.7) {
        var a = gen(chars, depth + 1), b = gen(chars, depth + 1);
        // an interval applies to the whole concatenation before it: group b
        var bt = index(b.text, "{") >= 0 ? "(" + b.text + ")" : b.text;
        return pat(a.text + bt, {k: "cat", a: a.smp, b: b.smp},
                   a.fixed != nil && b.fixed != nil ? a.fixed + b.fixed : nil,
                   a.nullable && b.nullable, a.simple && b.simple);
    }
    if (r < 0.8) {
        var a = gen(chars, depth + 1), b = gen(chars, depth + 1);
        return pat("(" + a.text + "|" + b.text + ")", {k: "alt", a: a.smp, b: b.smp},
                   a.fixed == b.fixed ? a.fixed : nil, a.nullable || b.nullable, false);
    }
    var a = gen(chars, depth + 1);
    var op = choice(["*", "+", "?", "{2}", "{1,2}", "{2,}"]);
    var inner = "(" + a.text + ")";
    if (op == "*")
        return pat(inner + "*", rep(a, 0, 3), nil, true, true);
    if (op == "+")
        return pat(inner + "+", rep(a, 1, 3), nil, a.nullable, true);
    if (op == "?")
        return pat(inner + "?", rep(a, 0, 1), nil, true, true);
    if (op == "{2}")
        return pat(inner + "{2}", rep(a, 2, 2), a.fixed != nil ? a.fixed * 2 : nil, a.nullable, false);
    if (op == "{1,2}")
        return pat(inner + "{1,2}", rep(a, 1, 2), nil, a.nullable, true);
    return pat(inner + "{2,}", rep(a, 2, 3), nil, a.nullable, true);
}

/* [the source, the sample descriptions of its rules] */
func make_source() {
    var nrules = rand(1, 7);
    var conds = [];
    if (frand() < 0.4)
        for (var i = 0, n = rand(1, 2); i < n; i++)
            push(conds, [sprintf("S%d", i), choice(["%s", "%x"])]);
    var feature = choice(["none", "none", "reject", "yymore", "yyless", "unput", "input", "begin"]);
    if (len(conds))
        feature = choice([feature, "begin"]);
    var condnames = apply(conds, func (c) { return c[0]; });
    var lines = ["%{", "#include <stdio.h>", "#include <string.h>", "%}"];
    for (var c : conds)
        push(lines, c[1] + " " + c[0]);
    if (frand() < 0.2)
        push(lines, "%array");
    push(lines, "%%");
    var samples = [];
    for (var i = 0; i < nrules; i++) {
        var chars = ["a", "b", "c"] + (frand() < 0.2 ? ["\n"] : []);
        // (no trailing context with REJECT, nor with yymore: flex drops the
        // yymore text when the next match has trailing context)
        var tc = frand() < 0.25 && feature != "reject" && feature != "yymore";
        var bol = frand() < 0.15;
        var kind, r, x;
        if (tc) {
            kind = choice(["xfixed", "rfixed", "disjoint", "dollar"]);
            if (kind == "disjoint") {
                RESTRICTED = true;
                r = gen(["a", "b"], 1);
                x = gen(["c"], 2);
                RESTRICTED = false;
            } else {
                r = gen(chars, 1);
                x = kind != "dollar" ? gen(chars, 2) : nil;
            }
            if (r.nullable || (x != nil && x.nullable))
                tc = false;
            else if (kind == "xfixed" && !x.simple)
                tc = false;
            else if (kind == "rfixed" && !r.simple)
                tc = false;
        }
        var p, smp;
        if (tc) {
            p = r.text + (kind == "dollar" ? "$" : "/" + x.text);
            smp = {k: "cat", a: r.smp, b: x != nil ? x.smp : {k: "str", s: "\n"}};
        } else {
            var g = gen(chars, 0);
            p = g.text;
            smp = g.smp;
        }
        if (bol)
            p = "^" + p;
        if (len(conds) && frand() < 0.5) {
            var sel = pick(condnames + ["INITIAL"], rand(1, 2));
            p = "<" + join(sel, ",") + ">" + p;
        }
        var act = sprintf("printf(\"<%d:%%s>\", yytext);", i);
        if (feature == "reject" && frand() < 0.5)
            act += " REJECT;";
        else if (feature == "yymore" && frand() < 0.4)
            act += " yymore();";
        else if (feature == "yyless" && frand() < 0.5)
            act = "if (yyleng > 1) { yyless(yyleng - 1); printf(\"(less)\"); } " + act;
        else if (feature == "unput" && frand() < 0.4)
            act = "if (yyleng >= 2) { char c0 = yytext[0]; " + act + " unput(c0); } else " + act;
        else if (feature == "input" && frand() < 0.4)
            act += " { int c = input(); printf(\"(%d)\", c); }";
        if (feature == "begin" && len(conds) && frand() < 0.6)
            act += " BEGIN " + choice(condnames + ["INITIAL"]) + ";";
        push(lines, p + "\t{ " + act + " }");
        push(samples, smp);
    }
    if (frand() < 0.5)
        push(lines, ".|\\n\tprintf(\"{%s}\", yytext[0] == '\\n' ? \"NL\" : yytext);");
    return [join(lines, "\n") + "\n", samples];
}

/* runs CMD on d/t.l and compiles lex.yy.c into d/exe: [path or nil, messages] */
func build(cmd, d, exe) {
    var r = run(cmd + ["t.l"], {cwd: d});
    if (r.status)
        return [nil, r.err];
    var c = run(ccenv.CC + " " + ccenv.CQUIET + " " + (scanflags == nil ? "" : scanflags) + " -o " + exe +
                " lex.yy.c -L" + shquote(libdir) + " -ll", {cwd: d});
    if (c.status)
        return [nil, c.err];
    return [d + "/" + exe, r.err];
}

func scan(exe, inp) {
    var r = run([exe], {in: inp, timeout: 5});
    if (r.timedout)
        return "TIMEOUT";
    var out = r.out;
    if (len(out) > 20000)
        out = substr(out, 0, 20000) + "...";
    return out + (r.status == 0 ? "" : sprintf("[status %d]", r.status));
}

var STATS = ["rounds", "runs", "diffs", "flex_rejects", "mine_rejects", "loops"];
var stats = {};
for (var s : STATS)
    stats[s] = 0;

/* the statistics as {'name': count, ...} */
func showstats() {
    return "{" + join(apply(STATS, func (s) { return sprintf("'%s': %d", s, stats[s]); }), ", ") + "}";
}

var d = mkdtemp("randlex.");
for (var rnd = 1; rnd <= count; rnd++) {
    var ms = make_source();
    var src = ms[0], samples = ms[1];
    writefile(d + "/t.l", src);
    var m = build([lex], d, "mine"), f = build(["flex", "-X"], d, "ref");
    var mine = m[0], ref = f[0];
    if (!ref) {
        stats.flex_rejects++;
        continue;
    }
    if (!mine) {
        stats.mine_rejects++;
        printf("MINE REJECTS (round %d): %s\n", rnd, trim(m[1]));
        if (keep)
            writefile(sprintf("%s/rej%d.l", keep, rnd), src);
        continue;
    }
    stats.rounds++;
    var inputs = {};
    for (var i = 0; i < 12; i++) {
        var s = "";
        for (var n = rand(0, 25); n > 0; n--)
            s += choice(["a", "b", "c", "x", "\n"]);
        inputs[s] = 1;
    }
    for (var i = 0; i < 12; i++) {
        var s = "";
        for (var n = rand(1, 6); n > 0; n--)
            s += sample(choice(samples));
        inputs[s] = 1;
    }
    for (var inp : sorted(keys(inputs))) {
        stats.runs++;
        var a = scan(mine, inp), b = scan(ref, inp);
        if (a == "TIMEOUT" && b == "TIMEOUT") {
            stats.loops++;
            continue;
        }
        if (a != b) {
            stats.diffs++;
            printf("DIFF (round %d) input %q\n mine: %q\n flex: %q\n", rnd, inp, substr(a, 0, 300), substr(b, 0, 300));
            if (verbose)
                print(src);
            if (keep)
                writefile(sprintf("%s/diff%d.l", keep, rnd), src);
            break;
        }
    }
    if (rnd % 50 == 0) {
        print("progress", rnd, showstats());
        flush();
    }
}
rmtree(d);
print(showstats());
exit(stats.diffs || stats.mine_rejects ? 1 : 0);
