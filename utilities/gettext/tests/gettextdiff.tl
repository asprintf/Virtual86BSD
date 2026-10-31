/*
 * gettextdiff.tl - gettext and ngettext compared with GNU gettext's
 *
 *     tl gettextdiff.tl GETTEXT [SEED [RUNS]]
 *
 * GETTEXT is this gettext (ngettext is the link next to it).  A locale
 * de_DE.UTF-8 is made with localedef in a directory of its own (LOCPATH),
 * as GNU gettext ignores LANGUAGE in the C locales.  Random messages
 * objects, made by GNU msgfmt (plural forms of several languages, some
 * in ISO-8859-1), are put under one of the forms of the locale's name and
 * under other languages; then random calls of both programs (-e, -E, -s, -n, the text
 * domain as an operand, by -d or TEXTDOMAIN, LANGUAGE lists, the C locale,
 * msgids found and not, values of n) must write the same and exit alike.
 * NLSPATH is not used: GNU gettext does not take it.  Where GNU gettext
 * departs from the standard, the cases are left out: LANGUAGE always names
 * fr, which has messages objects for the domains (GNU gettext does not go
 * on to the locale's name when LANGUAGE leads to no messages object), and
 * the msgids have no \" (GNU's -e does not take all the escapes of C),
 * and -s is given one msgid (GNU's translates only the first).
 */
include "../../testlib/testlib.tl";

/* a program in /usr/bin, /usr/local/bin or /bin, or nil */
func sysprog(name) {
    for (var d : ["/usr/bin", "/usr/local/bin", "/bin"])
        if (access(d + "/" + name, "x"))
            return d + "/" + name;
    return nil;
}

var GNU = sysprog("gettext"), GNUN = sysprog("ngettext"), MSGFMT = sysprog("msgfmt");

var IDS = ["hello", "world", "file", "open %s", "a\\tb", "two\\nlines", "Call", "caf\303\251", "none"];
var PLURALS = [[2, "n != 1"], [1, "0"],
               [3, "n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2"],
               [4, "n==1?0: (n>1&&n<=10)?1: (n==0)?2:3"], [2, "n > 1"]];
var GERMAN = ["de_DE.UTF-8", "de_DE.utf8", "de_DE", "de"];
var DIRS = GERMAN + ["fr", "it_IT", "pt_BR"];

/* UTF-8 text (of characters below U+0800) in ISO-8859-1 */
func latin1(s) {
    var out = "";
    for (var i = 0; i < len(s); i++) {
        if (s[i] >= 0xc0 && i + 1 < len(s)) {
            out += chr(((s[i] & 0x1f) << 6) | (s[i + 1] & 0x3f));
            i++;
        } else
            out += chr(s[i]);
    }
    return out;
}

/* k distinct elements of l, in random order */
func sample(l, k) {
    var c = copy(l);
    shuffle(c);
    return slice(c, 0, k);
}

func make_po(tag, latin) {
    var pl = choice(PLURALS);
    var nplurals = pl[0];
    var out = ["msgid \"\"", "msgstr \"\"",
               sprintf("\"Content-Type: text/plain; charset=%s\\n\"", latin ? "ISO-8859-1" : "UTF-8"),
               sprintf("\"Plural-Forms: nplurals=%d; plural=%s;\\n\"", nplurals, pl[1]), ""];
    for (var mid : IDS) {
        if (frand() < 0.3)
            continue;
        if (mid == "none")
            continue;
        var tr = sprintf("%s[%s]", mid, tag);
        if (latin)
            tr += " gr\303\274\303\237";
        if (frand() < 0.4) {
            push(out, sprintf("msgid \"%s\"", mid));
            push(out, sprintf("msgid_plural \"%ss\"", mid));
            for (var k = 0; k < nplurals; k++)
                push(out, sprintf("msgstr[%d] \"%s#%d\"", k, tr, k));
        } else {
            push(out, sprintf("msgid \"%s\"", mid));
            push(out, sprintf("msgstr \"%s\"", tr));
        }
        push(out, "");
    }
    var text = join(out, "\n") + "\n";
    return latin ? latin1(text) : text;
}

if (len(argv) < 1) {
    print("usage: gettextdiff.tl GETTEXT [SEED [RUNS]]");
    exit(2);
}
var ours = abspath(argv[0]);
var oursn = dirname(ours) + "/ngettext";
var seed = len(argv) > 1 ? int(argv[1]) : 1;
var runs = len(argv) > 2 ? int(argv[2]) : 500;
if (GNU == nil || GNUN == nil || MSGFMT == nil || which("localedef") == nil) {
    print("gettextdiff: needs GNU gettext, ngettext, msgfmt and localedef");
    exit(2);
}
srand(seed);
var fails = 0;
var tmp = mkdtemp("gettextdiff.");
var loc = tmp + "/loc";
mkdir(loc);
run(["localedef", "-i", "de_DE", "-f", "UTF-8", loc + "/de_DE.UTF-8"], {out: "null", err: "null"});
if (!isdir(loc + "/de_DE.UTF-8")) {
    print("gettextdiff: localedef could not make de_DE.UTF-8");
    rmtree(tmp);
    exit(2);
}
var tdir = tmp + "/msgs";
for (var rnd = 0; rnd < runs / 50 + 1; rnd++) {
    /* a new set of messages objects every 50 calls */
    rmtree(tdir);
    for (var dom : ["dom", "other"]) {
        /* one form of de_DE.UTF-8 for each domain (GNU goes on to the
           next form when one lacks the message, the standard stops at the
           first messages object), and other languages */
        var dirs = ["fr"];
        for (var d : DIRS)
            if (!(d in GERMAN) && d != "fr" && frand() < 0.6)
                push(dirs, d);
        if (frand() < 0.9)
            push(dirs, choice(GERMAN));
        for (var d : dirs) {
            var lm = tdir + "/" + d + "/LC_MESSAGES";
            mkdirs(lm);
            var po = tmp + "/x.po";
            writefile(po, make_po(d + "/" + dom, frand() < 0.2));
            if (run([MSGFMT, "-o", lm + "/" + dom + ".mo", po], {out: "inherit", err: "inherit"}).status != 0)
                error("msgfmt failed");
        }
    }
    for (var i = 0; i < min(50, runs - rnd * 50); i++) {
        var env = {LANGUAGE: nil, TEXTDOMAIN: nil, NLSPATH: nil, LANG: nil, LC_MESSAGES: nil,
                   LOCPATH: loc, TEXTDOMAINDIR: tdir};
        env.LC_ALL = frand() < 0.85 ? "de_DE.UTF-8" : "C";
        if (frand() < 0.5) {
            var langs = sample(["it_IT", "pt_BR", "xx", "de", "de_AT"], rand(0, 2));
            insert(langs, rand(0, len(langs)), "fr");
            env.LANGUAGE = join(langs, ":");
        }
        var dom = choice(["dom", "other", "nodom"]);
        var plural = frand() < 0.4;
        var opts = [];
        var esc = choice([nil, "-e", "-E"]);
        if (esc != nil)
            push(opts, esc);
        var how = choice(["op", "d", "env"]);
        if (how == "d")
            opts += ["-d", dom];
        else if (how == "env")
            env.TEXTDOMAIN = dom;
        var mid = choice(IDS);
        var args, a, b;
        if (plural) {
            var n = str(choice([0, 1, 2, 3, 5, 11, 21, 22, 25, 101, 111]));
            args = opts + (how == "op" ? [dom] : []) + [mid, mid + "s", n];
            a = [oursn] + args;
            b = [GNUN] + args;
        } else {
            if (how != "op" && frand() < 0.3) {
                push(opts, "-s");
                if (frand() < 0.5)
                    push(opts, "-n");
                args = opts + [choice(IDS)];
            } else
                args = opts + (how == "op" ? [dom] : []) + [mid];
            a = [ours] + args;
            b = [GNU] + args;
        }
        var pa = run(a, {env: env}), pb = run(b, {env: env});
        if (pa.out != pb.out || pa.status != pb.status) {
            fails++;
            if (fails <= 10) {
                printf("FAIL: LC_ALL=%s LANGUAGE=%s TEXTDOMAIN=%s %s\n", env.LC_ALL,
                       env.LANGUAGE == nil ? "" : env.LANGUAGE, env.TEXTDOMAIN == nil ? "" : env.TEXTDOMAIN,
                       join(args, " "));
                printf("  ours: %q status %d\n", pa.out, pa.status);
                printf("  gnu:  %q status %d\n", pb.out, pb.status);
            }
        }
    }
}
rmtree(tmp);
printf("gettextdiff: %d compared, %d failed\n", runs, fails);
exit(fails ? 1 : 0);
