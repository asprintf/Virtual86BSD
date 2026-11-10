/*
 * gencatdiff.tl - gencat compared with the GNU C library's gencat
 *
 *     tl gencatdiff.tl GENCAT [SEED [RUNS]]
 *
 * Random message text sources ($set in ascending order, messages in
 * ascending order in each, deleted messages (in a later msgfile: GNU
 * gencat stores an empty message for a number that is not there), empty
 * ones, $quote and quoted texts, the escapes, continuation lines,
 * comments; not $delset, which GNU gencat does not carry out) are made
 * into catalogs by both, also merged into a catalog that exists; the
 * catalogs must be the same, byte for byte.  Then every message of the
 * catalog is read back with the C library's catopen() and catgets() (a
 * small C program, tests/catread.c, built here) and compared with what was
 * written.  When a catalog is merged into, no empty messages are made (GNU
 * gencat loses them then).  It runs in a UTF-8 locale (GNU gencat rejects
 * bytes that are not characters of the locale), or without non-ASCII text.
 */
include "../../../lib/testlib/testlib.tl";

var GNU = nil;
for (var d : ["/usr/bin", "/usr/local/bin", "/bin"])
    if (GNU == nil && access(d + "/gencat", "x"))
        GNU = d + "/gencat";

var WORDS = ["hello", "world", "x", "Message", "na\303\257ve", "tab\\there", "new\\nline", "back\\\\slash",
             "\\101\\102", "\\7", "a\\qb", "  spaced", "end  "];

func text(quote) {
    var w = [];
    for (var n = rand(0, 4); n > 0; n--)
        push(w, choice(WORDS));
    var t = join(w, " ");
    if (quote != nil && frand() < 0.5)
        t = quote + replace(t, quote, "\\" + quote) + quote;
    else if (quote != nil && startswith(t, quote))
        t = "z" + t;
    if (len(t) > 6 && frand() < 0.15) {
        /* a continuation line, not in an escape */
        var k = rand(1, len(t) - 1);
        var from = max(0, k - 4);
        if (!("\\" in substr(t, from, k + 1 - from)) && (quote == nil || !(quote in t)))
            t = substr(t, 0, k) + "\\\n" + substr(t, k);
    }
    return t;
}

/* a source; sets_seen and existing (maps) are what earlier ones defined */
func source(sets_seen, existing, empty) {
    var out = [];
    var quote = nil;
    var s = 0;
    for (var ns = rand(1, 4); ns > 0; ns--) {
        s += rand(1, 5);
        if (frand() < 0.2) {
            quote = choice(["\"", "'", nil]);
            push(out, "$quote" + (quote != nil ? " " + quote : ""));
        }
        if (!(len(sets_seen) == 0 && s == 1 && frand() < 0.5))   /* without it: NL_SETD, 1 */
            push(out, sprintf("$set %d%s", s, choice(["", " a comment"])));
        sets_seen[s] = 1;
        var m = 0;
        for (var nm = rand(0, 8); nm > 0; nm--) {
            m += rand(1, 4);
            var c = frand();
            var key = sprintf("%d,%d", s, m);
            if (haskey(existing, key) && c < 0.3) {
                push(out, str(m));              /* a deletion of one that exists */
                continue;
            }
            existing[key] = 1;
            if (c < 0.15 && empty)
                push(out, sprintf("%d ", m));
            else {
                var t = text(quote);
                if (!empty && (t in ["", "\"\"", "''"]))
                    t = "x";
                push(out, sprintf("%d %s", m, t));
            }
            if (frand() < 0.1)
                push(out, "$ a comment");
            if (frand() < 0.05)
                push(out, "");
        }
    }
    return join(out, "\n") + "\n";
}

func stlist(st) {
    return "[" + join(apply(st, str), ", ") + "]";
}

if (len(argv) < 1) {
    print("usage: gencatdiff.tl GENCAT [SEED [RUNS]]");
    exit(2);
}
var ours = abspath(argv[0]);
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 300;
if (GNU == nil) {
    print("gencatdiff: no GNU gencat");
    exit(2);
}
/* GNU gencat takes only the characters of the locale: a UTF-8 one */
var u8 = nil;
var locs = split(run(["locale", "-a"]).out);
for (var l : locs)
    if (u8 == nil && replace(lower(l), "-", "") == "c.utf8")
        u8 = l;
for (var l : locs)
    if (u8 == nil && endswith(replace(lower(l), "-", ""), ".utf8"))
        u8 = l;
if (u8 != nil)
    setenv("LC_ALL", u8);
else
    WORDS = filter(WORDS, func (w) { return !non_ascii(w); });
srand(seed);
var fails = 0;
var tmp = mkdtemp("gencatdiff.");
var reader = tmp + "/catread";
var cc = getenv("CC");
if (run([cc == nil || cc == "" ? "cc" : cc, "-o", reader, scriptdir + "/catread.c"], {out: "inherit", err: "inherit"}).status != 0)
    error("cannot compile catread.c");
for (var i = 0; i < runs; i++) {
    var seen = {}, existing = {};
    var srcs = [];
    var merge = frand() < 0.3;
    for (var k = 0, n = merge ? choice([1, 2]) : 1; k < n; k++) {
        var p = sprintf("%s/m%d.msg", tmp, k);
        writefile(p, source(seen, existing, !merge));
        push(srcs, p);
    }
    var res = {};
    for (var who : [["ours", ours], ["gnu", GNU]]) {
        var cat = tmp + "/" + who[0] + ".cat", prog = who[1];
        remove(cat);
        var st = [];
        if (merge) {
            push(st, run([prog, cat, srcs[0]]).status);
            push(st, run([prog, cat] + (len(srcs) > 1 ? slice(srcs, 1) : slice(srcs, 0, 1))).status);
        } else
            push(st, run([prog, cat] + srcs).status);
        res[who[0]] = [st, readfile(cat)];
    }
    var bad = res.ours != res.gnu;
    var why = "the catalogs differ";
    if (!bad && res.ours[1]) {
        var p = run([reader, tmp + "/ours.cat"]);
        if (p.status != 0) {
            bad = true;
            why = "catgets() did not find every message:\n" + p.out;
        }
    }
    if (bad) {
        fails++;
        if (fails <= 5) {
            printf("FAIL run %d (%s): %s; statuses %s %s\n", i, merge ? "merged" : "one run", why,
                   stlist(res.ours[0]), stlist(res.gnu[0]));
            for (var k = 0; k < len(srcs); k++)
                writefile(sprintf("gencatdiff.fail%d.%d.msg", i, k), readfile(srcs[k]));
            for (var who : ["ours", "gnu"])
                if (res[who][1] != nil)
                    writefile(sprintf("gencatdiff.fail%d.%s.cat", i, who), res[who][1]);
        }
    }
}
rmtree(tmp);
printf("gencatdiff: %d compared, %d failed\n", runs, fails);
exit(fails ? 1 : 0);
