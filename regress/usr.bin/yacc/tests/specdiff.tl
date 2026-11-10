/*
 * specdiff.tl - compare parsers for real grammars (tests/grammars/*.y) with
 * bison -y and byacc (the latter with byacc's choice of default reductions).
 *
 * usage: tl specdiff.tl YACC [N]
 *
 * For each grammar, the rules are read from the description file, random
 * sentences are derived from them (and mutated), and both parsers, built
 * with debugging code, are run on the token sequences.  The sequence of
 * rules reduced, the yyerror calls and the result must be the same.
 */
include "../../../lib/testlib/testlib.tl";

var YACC = abspath(argv[0]);
var N = len(argv) > 1 ? int(argv[1]) : 200;
var gdir = scriptdir + "/grammars";
var ccenv = cc_env();

/* the rules [lhs, [symbols]] of a y.output, rule 0 first */
func rules_of(output) {
    var rules = [];
    var lhs = nil, body;
    for (var line : split(output, "\n")) {
        var m = match(line, "^[[:space:]]*([0-9]+)[[:space:]]+([^[:space:]]+) :(.*)$");
        var m2 = match(line, "^[[:space:]]*([0-9]+)[[:space:]]+\\|(.*)$");
        if (m) {
            lhs = m[2];
            body = m[3];
        } else if (m2)
            body = m2[2];
        else {
            if (startswith(line, "state"))
                break;
            continue;
        }
        push(rules, [lhs, split(replace(body, "/* empty */", ""))]);
    }
    return rules;
}

/* group 1 of each match of re in s */
func findall(s, re) {
    var res = [];
    var p;
    while ((p = matchat(s, re)) != nil) {
        push(res, match(slice(s, p[0], p[1]), re)[1]);
        s = slice(s, p[1] > p[0] ? p[1] : p[0] + 1);
    }
    return res;
}

var DRIVER = R"(
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static struct { const char *n; int v; } names[] = { @NAMES@ { 0, 0 } };
int yylex(void)
{
    char buf[256];
    int i;
    if (!fgets(buf, sizeof buf, stdin)) return 0;
    buf[strcspn(buf, "\n")] = 0;
    if (buf[0] == '\'') return (unsigned char)buf[1] == '\\' ? (buf[2] == 'n' ? '\n' : buf[2] == 't' ? '\t' : buf[2]) : (unsigned char)buf[1];
    for (i = 0; names[i].n; i++) if (!strcmp(names[i].n, buf)) return names[i].v;
    return 255;
}
void yyerror(const char *s) { printf("E:%s\n", s); }
int main(void) { extern int yydebug; int r; yydebug = 1; r = yyparse(); printf("-> %d\n", r); return 0; }
)";

/* the parser of grammar g made by cmd in d, with the driver above: its path, or nil */
func build(cmd, d, g, exe, names, env) {
    var r = run(cmd + [g], {cwd: d, env: env == nil ? {} : env});
    if (r.status)
        return nil;
    var hdr = readfile(d + "/y.tab.h");
    var tab = "";
    for (var n : names)
        if (index(hdr, "#define " + n + " ") >= 0)
            tab += sprintf("{ \"%s\", %s }, ", n, n);
    writefile(d + "/" + exe + ".c", readfile(d + "/y.tab.c") + replace(DRIVER, "@NAMES@", tab));
    var c = run(ccenv.CC + " " + ccenv.CQUIET + " -DYYDEBUG=1 -o " + exe + " " + exe + ".c", {cwd: d});
    if (c.status) {
        print(substr(c.err, 0, 2000));
        return nil;
    }
    return d + "/" + exe;
}

func trace(exe, inp, kind) {
    var r = run([exe], {in: inp, timeout: 10});
    if (r.timedout)
        return "TIMEOUT";
    var red = findall(r.err, kind == "mine" ? "reducing by rule ([0-9]+)" : "Reducing stack by rule ([0-9]+)");
    return join(red, " ") + " | " + replace(r.out, "\n", " ");
}

/* the globals of derive() */
var nts, user;

/* a random sentence derived from sym, or nil if it grows too long */
func derive(sym, depth) {
    if (!haskey(nts, sym))
        return sym == "error" ? [] : [sym];
    if (depth > 500)
        return nil;
    var alts = [];
    for (var r : user)
        if (r[0] == sym)
            push(alts, r[1]);
    if (depth > 12) {
        // the alternative with the fewest nonterminals
        sort(alts, func (a, b) {
            var na = 0, nb = 0;
            for (var s : a)
                na += haskey(nts, s);
            for (var s : b)
                nb += haskey(nts, s);
            return na - nb;
        });
        alts = slice(alts, 0, 1);
    }
    var out = [];
    for (var s : choice(alts)) {
        var t = derive(s, depth + 1);
        if (t == nil)
            return nil;
        out += t;
        if (len(out) > 300)
            return nil;
    }
    return out;
}

var total = 0, diffs = 0;
srand(1);
for (var f : listdir(gdir)) {
    if (!endswith(f, ".y"))
        continue;
    var d = mkdtemp("yspec.");
    var src = readfile(gdir + "/" + f);
    writefile(d + "/" + f, src);
    run([YACC, "-v", f], {cwd: d});
    var rules = rules_of(readfile(d + "/y.output"));
    nts = {};
    for (var r : rules)
        nts[r[0]] = 1;
    var tset = {};
    for (var r : rules)
        for (var s : r[1])
            if (!haskey(nts, s) && s != "$end")
                tset[s] = 1;
    var terms = sorted(keys(tset));
    var names = filter(terms, func (t) { return match(t, "^[A-Za-z_][A-Za-z0-9_]*$") != nil && t != "error"; });
    var mine = build([YACC, "-d", "-t"], d, f, "mine", names);
    // a one-character string literal is the character literal here,
    // a separate token in bison
    writefile(d + "/b_" + f, gsub(src, "\"([^\"\\\\])\"", "'\\1'"));
    var bis = build(["bison", "-y", "-d", "-t", "-Wnone"], d, "b_" + f, "bis", names);
    var mineb = build([YACC, "-d", "-t"], d, f, "mineb", names, {YACC_TEST_DEFAULTS: "byacc"});
    // byacc makes a string literal "name" the same token as name
    var bya = match(src, "\"[^\"\n]{2,}\"") ? nil : build(["byacc", "-d", "-t"], d, "b_" + f, "bya", names);
    user = filter(rules, func (r) { return r[0] != "$accept"; });
    var start = rules[0][1][0];
    var n = 0, nd = 0;
    for (var i = 0; i < N; i++) {
        var toks = derive(start, 0);
        if (toks == nil)
            continue;
        if (i % 3 == 1 && len(toks)) {
            var k = rand(len(toks));
            var op = rand(3);
            if (op == 0)
                pop(toks, k);
            else if (op == 1)
                insert(toks, k, choice(terms));
            else
                toks[k] = choice(terms);
        }
        var inp = join(apply(toks, func (t) { return t + "\n"; }), "");
        n++;
        var a = "", b = "";
        if (bis) {
            a = trace(mine, inp, "mine");
            b = trace(bis, inp, "bison");
        }
        if (mineb && bya && a == b) {
            a = trace(mineb, inp, "mine");
            b = trace(bya, inp, "mine");
        }
        if (a != b) {
            nd++;
            if (nd <= 3)
                printf("%s: DIFF on %q\n mine:  %s\n other: %s\n", f, toks, slice(a, -300), slice(b, -300));
        }
    }
    printf("%s: %d inputs, %d differences\n", f, n, nd);
    total += n;
    diffs += nd;
    rmtree(d);
}
printf("total %d inputs, %d differences\n", total, diffs);
exit(diffs ? 1 : 0);
