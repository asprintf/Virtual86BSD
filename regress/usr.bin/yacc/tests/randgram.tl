/*
 * randgram.tl - random differential testing of yacc against bison -y and
 * byacc.
 *
 * usage: tl randgram.tl YACC SEED COUNT [--keep DIR] [-v]
 *
 * Each round generates a random grammar (literals, named tokens,
 * precedence declarations, %prec, mid-rule actions, error rules), builds
 * the parser with each implementation, and runs it on sentences of the
 * grammar, mutated sentences and random strings.  The actions print the
 * rule numbers and computed values, so the output shows the sequence of
 * reductions, the semantic values, the calls of yyerror and the result
 * of yyparse.  Conflict counts are compared with bison's report.
 */
include "../../../lib/testlib/testlib.tl";

var YACC = abspath(argv[0]);
var SEED = int(argv[1]);
var COUNT = int(argv[2]);
var VERBOSE = "-v" in argv;
var KEEP = find(argv, "--keep") >= 0 ? argv[find(argv, "--keep") + 1] : nil;
var ccenv = cc_env();

var LITS = ["a", "b", "c", "d", "e", "+", "-", "*"];
var NAMED = ["TA", "TB", "TC"];
var NAMEDCH = {TA: "A", TB: "B", TC: "C"};
var MID = "{mid}";   // a mid-rule action in a rule body (the other items are symbols)

/* k different elements of l, in random order */
func pick(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

func isnt(s) {
    return startswith(s, "s");
}

/* a random grammar, or nil if a nonterminal derives no sentence */
func gen_grammar() {
    var nnt = rand(1, 6);
    var nts = apply(range(nnt), func (i) { return sprintf("s%d", i); });
    var use_err = frand() < 0.35;
    var terms = pick(LITS, rand(2, len(LITS))) + pick(NAMED, rand(0, 3));
    var rules = [];   // [lhs, [items]]
    for (var nt : nts)
        for (var j = rand(1, 4); j > 0; j--) {
            var body = [];
            for (var n = choice([0, 1, 1, 2, 2, 3, 3, 4]); n > 0; n--) {
                var r = frand();
                if (r < 0.45)
                    push(body, choice(nts));
                else if (r < 0.93)
                    push(body, choice(terms));
                else
                    push(body, MID);
            }
            push(rules, [nt, body]);
        }
    if (use_err)
        for (var j = rand(1, 2); j > 0; j--) {
            var nt = choice(nts);
            push(rules, [nt, ["error"] + (frand() < 0.8 ? [choice(terms)] : [])]);
        }
    // productivity of s0
    var prod = {};
    var changed = true;
    while (changed) {
        changed = false;
        for (var r : rules) {
            if (haskey(prod, r[0]))
                continue;
            var all = true;
            for (var it : r[1])
                if (isnt(it) && !haskey(prod, it))
                    all = false;
            if (all) {
                prod[r[0]] = 1;
                changed = true;
            }
        }
    }
    for (var nt : nts)
        if (!haskey(prod, nt))
            return nil;
    var prec = [];
    if (frand() < 0.6) {
        var pt = copy(terms);
        shuffle(pt);
        var k = 0;
        while (k < len(pt) && frand() < 0.7) {
            var m = rand(1, 2);
            push(prec, [choice(["%left", "%right", "%nonassoc"]), slice(pt, k, k + m)]);
            k += m;
        }
    }
    var precs = {};
    for (var p : prec)
        for (var t : p[1])
            precs[t] = 1;
    var g = {nts: nts, terms: terms, rules: rules, prec: prec, precs: precs,
             use_yyerrok: frand() < 0.5, rng_prec: [], prec_choice: []};
    for (var r : rules)
        push(g.rng_prec, frand());
    for (var r : rules)
        push(g.prec_choice, len(precs) ? choice(sorted(keys(precs))) : nil);
    return g;
}

func sym_text(s) {
    if (s in NAMED || isnt(s) || s == "error")
        return s;
    return "'" + s + "'";
}

func render(g) {
    var out = ["%{", "#include <stdio.h>", "#include <stdlib.h>",
               "void yyerror(const char *s);", "int yylex(void);", "%}"];
    var named = filter(g.terms, func (t) { return t in NAMED; });
    if (len(named))
        push(out, "%token " + join(named, " "));
    for (var p : g.prec)
        push(out, p[0] + " " + join(apply(p[1], sym_text), " "));
    push(out, "%%");
    push(out, "top : s0 { printf(\"=%d\\n\", $1); $$ = 0; } ;");
    var mid = 0;
    for (var i = 0; i < len(g.rules); i++) {
        var lhs = g.rules[i][0], body = g.rules[i][1];
        var parts = [];
        var pos = 0;
        var terms_v = [];
        for (var it : body) {
            pos++;
            if (it == MID) {
                mid++;
                push(parts, sprintf("{ printf(\"m%d \", %d); $$ = %d; }", mid, mid, mid * 7));
                push(terms_v, pos);
            } else {
                push(parts, sym_text(it));
                if (it != "error")
                    push(terms_v, pos);
            }
        }
        var expr = [sprintf("%d", i + 1)];
        for (var k = 0; k < len(terms_v); k++)
            push(expr, sprintf("$%d * %d", terms_v[k], (k % 5) + 2));
        var act = sprintf("{ $$ = (%s) %% 10007; printf(\"r%d \", %d);%s }", join(expr, " + "), i, i,
                          len(body) && body[0] == "error" && g.use_yyerrok ? " yyerrok;" : "");
        var precpart = "";
        if (g.prec_choice[i] != nil && g.rng_prec[i] < 0.15)
            precpart = " %prec " + sym_text(g.prec_choice[i]);
        push(out, sprintf("%s : %s%s %s ;", lhs, join(parts, " "), precpart, act));
    }
    push(out, "%%");
    push(out, "int yylex(void) {", "  int c;", "  do c = getchar(); while (c == ' ' || c == '\\n');",
         "  if (c == EOF) return 0;", "  yylval = c;");
    for (var t : named)
        push(out, sprintf("  if (c == '%s') return %s;", NAMEDCH[t], t));
    push(out, "  return c;", "}");
    push(out, "void yyerror(const char *s) { printf(\"E:%s \", s); }");
    push(out, "int main(void) { int r = yyparse(); printf(\" ->%d\\n\", r); return 0; }");
    return join(out, "\n") + "\n";
}

func tokch(t) {
    return get(NAMEDCH, t, t);
}

/* the number of nonterminals in a rule body */
func ntcount(b) {
    var n = 0;
    for (var it : b)
        n += isnt(it);
    return n;
}

/* a random sentence derived from sym, or nil if the derivation goes too deep */
func derive(g, sym, depth) {
    if (!(sym in g.nts))
        return tokch(sym);
    if (depth > 200)
        return nil;
    var alts = [], all = [];
    for (var r : g.rules)
        if (r[0] == sym) {
            push(all, r[1]);
            if (!("error" in r[1]))
                push(alts, r[1]);
        }
    if (!len(alts))
        alts = all;
    if (depth > 7)
        alts = slice(sorted(alts, func (a, b) { return ntcount(a) - ntcount(b); }), 0, 1);
    var s = "";
    for (var it : choice(alts)) {
        if (it == MID)
            continue;
        if (it == "error")
            s += choice(["a", "b", "c", "d", "e", "+", "-", "*"]);
        else {
            var t = derive(g, it, depth + 1);
            if (t == nil)
                return nil;
            s += t;
        }
    }
    return s;
}

func inputs(g) {
    var res = {};
    var alphabet = apply(g.terms, tokch) + ["x"];
    for (var i = 0; i < 25; i++) {
        var s = derive(g, "s0", 0);
        if (s == nil || len(s) > 60)
            continue;
        res[s] = 1;
        for (var j = 0; j < 2; j++) {
            var op = rand(0, 2);
            var pos = rand(0, len(s));
            var m = s;
            if (op == 0)
                m = slice(s, 0, pos) + choice(alphabet) + slice(s, pos);
            else if (op == 1 && len(s)) {
                var k = min(pos, len(s) - 1);
                m = slice(s, 0, k) + slice(s, k + 1);
            } else if (len(s)) {
                var k = min(pos, len(s) - 1);
                m = slice(s, 0, k) + choice(alphabet) + slice(s, k + 1);
            }
            res[m] = 1;
        }
    }
    for (var i = 0; i < 8; i++) {
        var s = "";
        for (var n = rand(0, 8); n > 0; n--)
            s += choice(alphabet);
        res[s] = 1;
    }
    return sorted(keys(res));
}

/* runs cmd on d/g.y and compiles y.tab.c into d/name: [path or nil, messages] */
func build(cmd, d, name, env) {
    var r = run(cmd + ["g.y"], {cwd: d, env: env == nil ? {} : env});
    if (r.status != 0)
        return [nil, r.err];
    var c = run(ccenv.CC + " " + ccenv.CQUIET + " -o " + name + " y.tab.c", {cwd: d});
    if (c.status != 0)
        return [nil, c.err];
    return [d + "/" + name, r.err];
}

/* [shift/reduce, reduce/reduce] in a report */
func conflicts(stderr) {
    var m = match(stderr, "([0-9]+) shift/reduce");
    var sr = m ? int(m[1]) : 0;
    m = match(stderr, "([0-9]+) reduce/reduce");
    var rr = m ? int(m[1]) : 0;
    return [sr, rr];
}

func parse(exe, inp) {
    var r = run([exe], {in: inp, timeout: 2});
    if (r.timedout)
        return "TIMEOUT";
    var out = r.out + (r.status == 0 ? "" : sprintf(" [status %d]", r.status));
    if (index(out, "E:yacc stack overflow") >= 0 || index(out, "E:memory exhausted") >= 0)
        return "STACK OVERFLOW";   // the depth differs by one between implementations
    return out;
}

func showconf(c) {
    return sprintf("(%d, %d)", c[0], c[1]);
}

var STATS = ["grammars", "runs", "diff_bison", "diff_bison_errorrules", "diff_byacc", "byacc_accept_quirk",
             "conf_diff", "skipped", "loops"];
var stats = {};
for (var s : STATS)
    stats[s] = 0;

/* the statistics as {'name': count, ...} */
func showstats() {
    return "{" + join(apply(STATS, func (s) { return sprintf("'%s': %d", s, stats[s]); }), ", ") + "}";
}

func keep(d, name) {
    if (KEEP)
        writefile(KEEP + "/" + name, readfile(d + "/g.y"));
}

srand(SEED);
var d = mkdtemp("ygram.");
var rounds = 0;
while (rounds < COUNT) {
    var g = gen_grammar();
    if (!g)
        continue;
    rounds++;
    var text = render(g);
    writefile(d + "/g.y", text);
    var m = build([YACC], d, "mine");
    var bis = build(["bison", "-y", "-Wno-other", "-Wno-conflicts-sr", "-Wno-conflicts-rr", "-Wno-yacc",
                     "-Wno-empty-rule", "-Wno-midrule-values", "-Wno-precedence"], d, "bis")[0];
    var berr2 = run(["bison", "-y", "g.y", "-o", "/dev/null"], {cwd: d}).err;
    var bya = build(["byacc"], d, "bya")[0];
    var mineb = build([YACC], d, "mineb", {YACC_TEST_DEFAULTS: "byacc"})[0];
    var mine = m[0], merr = m[1];
    if (!mine) {
        print("MINE FAILED seed round", rounds, merr);
        keep(d, sprintf("fail%d.y", rounds));
        stats.skipped++;
        continue;
    }
    if (!bis) {
        stats.skipped++;
        continue;
    }
    stats.grammars++;
    if (rounds % 20 == 0) {
        print("progress", rounds, showstats());
        flush();
    }
    if (conflicts(merr) != conflicts(berr2)) {
        stats.conf_diff++;
        printf("conflicts differ (round %d): mine %s bison %s\n", rounds, showconf(conflicts(merr)),
               showconf(conflicts(berr2)));
        keep(d, sprintf("conf%d.y", rounds));
    }
    var uses_error = index(split(text, "%%")[1], "error") >= 0;
    for (var inp : inputs(g)) {
        stats.runs++;
        var a = parse(mine, inp);
        var b = parse(bis, inp);
        if ((a == "TIMEOUT" || a == "STACK OVERFLOW") && a == b) {
            stats.loops++;   // a cyclic grammar: both parsers loop
            continue;
        }
        if (a != b) {
            if (uses_error) {
                stats.diff_bison_errorrules++;
                if (VERBOSE)
                    printf("diff bison, error rules (round %d) input %q\n mine:  %s\n bison: %s\n", rounds, inp, a, b);
            } else {
                stats.diff_bison++;
                printf("DIFF bison (round %d) input %q\n mine:  %s\n bison: %s\n", rounds, inp, a, b);
                keep(d, sprintf("diff%d.y", rounds));
            }
        }
        if (bya && mineb) {
            var a2 = parse(mineb, inp);
            var c = parse(bya, inp);
            if (a2 != c && endswith(a2, " ->0\n") && c == slice(a2, 0, -5) + " ->1\n" && index(a2, "E:") >= 0)
                // byacc accepts only right after the reduction to the start
                // symbol, not after error recovery discards a token there
                stats.byacc_accept_quirk++;
            else if (a2 != c) {
                stats.diff_byacc++;
                printf("DIFF byacc (round %d) input %q\n mine:  %s\n byacc: %s\n", rounds, inp, a2, c);
                keep(d, sprintf("bdiff%d.y", rounds));
            }
        }
    }
}
rmtree(d);
print(showstats());
