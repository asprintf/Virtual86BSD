/*
 * mkterminfo.tl - write a compiled terminfo entry, for the tests of tput
 * and tabs (the format of term(5); POSIX has no utility that compiles one).
 *
 *     tl mkterminfo.tl DIR NAMES [-32] CAP...
 *
 * writes DIR/<first character>/<first name>.  NAMES is "name|...|long
 * name".  A CAP is NAME (a boolean), NAME#NUMBER, or NAME=STRING with the
 * escapes \E (ESC), \n, \r, \t, \\, \nnn (octal) and ^X; a NAME that is
 * not a standard capability is an extended one.  -32 writes 32-bit numbers
 * (the magic 01036).
 */

/* the standard capabilities in the order of the format (those of ncurses) */
var BOOLS = split("bw am xsb xhp xenl eo gn hc km hs in da db mir msgr os eslok xt hz ul xon nxon "
                  "mc5i chts nrrmc npc ndscr ccc bce hls xhpa crxm daisy xvpa sam cpix lpix OTbs "
                  "OTns OTnc OTMT OTNL OTpt OTxr");
var NUMS = split("cols it lines lm xmc pb vt wsl nlab lh lw ma wnum colors pairs ncv bufsz spinv "
                 "spinh maddr mjump mcs mls npins orc orl orhi orvi cps widcs btns bitwin bitype "
                 "OTug OTdC OTdN OTdB OTdT OTkn");
/* the strings up to those the tests use (the order of ncurses' strnames) */
var STRS = split("cbt bel cr csr tbc clear el ed hpa cmdch cup cud1 home civis cub1 mrcup cnorm "
                 "cuf1 ll cuu1 cvvis dch1 dl1 dsl hd smacs blink bold smcup smdc dim smir invis "
                 "prot rev smso smul ech rmacs sgr0 rmcup rmdc rmir rmso rmul flash ff fsl is1 is2 "
                 "is3 if ich1 il1 ip kbs ktbc kclr kctab kdch1 kdl1 kcud1 krmir kel ked kf0 kf1 "
                 "kf10 kf2 kf3 kf4 kf5 kf6 kf7 kf8 kf9 khome kich1 kil1 kcub1 kll knp kpp kcuf1 "
                 "kind kri khts kcuu1 rmkx smkx lf0 lf1 lf10 lf2 lf3 lf4 lf5 lf6 lf7 lf8 lf9 rmm "
                 "smm nel pad dch dl cud ich indn il cub cuf rin cuu pfkey pfloc pfx mc0 mc4 mc5 "
                 "rep rs1 rs2 rs3 rf rc vpa sc ind ri sgr hts");

func isdigit(c) { return c >= '0' && c <= '9'; }

func unescape(s) {
    var out = "", i = 0, n = len(s);
    var esc = {E: "\033", e: "\033", n: "\n", r: "\r", t: "\t", "\\": "\\", s: " ", "0": "\200"};
    while (i < n) {
        var c = s[i];
        if (c == '\\' && i + 3 < n + 1 && isdigit(s[i + 1]) && isdigit(s[i + 2]) && isdigit(s[i + 3])) {
            var v = int(substr(s, i + 1, 3), 8);
            out += chr(v ? v : 0x80);
            i += 4;
        } else if (c == '\\' && i + 1 < n) {
            var k = chr(s[i + 1]);
            out += k in esc ? esc[k] : k;
            i += 2;
        } else if (c == '^' && i + 1 < n) {
            out += s[i + 1] != '?' ? chr(ord(upper(chr(s[i + 1]))) & 0x1f) : "\177";
            i += 2;
        } else {
            out += chr(c);
            i++;
        }
    }
    return out;
}

func h(v) { return pack("<h", v & 0xffff); }

var d = argv[0], names = argv[1];
var args = slice(argv, 2), wide = "-32" in args;
args = filter(args, func (a) { return a != "-32"; });
var bools = {}, nums = {}, strs = {}, xb = {}, xn = {}, xs = {};
for (var a : args) {
    var e = index(a, "="), sharp = index(a, "#");
    if (e >= 0) {
        var k = substr(a, 0, e);
        if (k in STRS) strs[k] = unescape(substr(a, e + 1)); else xs[k] = unescape(substr(a, e + 1));
    } else if (sharp >= 0) {
        var k = substr(a, 0, sharp);
        if (k in NUMS) nums[k] = int(substr(a, sharp + 1)); else xn[k] = int(substr(a, sharp + 1));
    } else {
        if (a in BOOLS) bools[a] = 1; else xb[a] = 1;
    }
}
func count(have, all) {
    var c = 0;
    for (var k : have)
        c = max(c, find(all, k) + 1);
    return c;
}
var nb = count(bools, BOOLS), nn = count(nums, NUMS), ns = count(strs, STRS);
func numb(v) { return wide ? pack("<i", v) : h(v); }
var namesb = names + "\0";
var table = "", offs = [];
for (var k : slice(STRS, 0, ns)) {
    if (k in strs) {
        push(offs, len(table));
        table += strs[k] + "\0";
    } else
        push(offs, -1);
}
var out = h(wide ? 01036 : 0432) + h(len(namesb)) + h(nb) + h(nn) + h(ns) + h(len(table));
out += namesb;
for (var k : slice(BOOLS, 0, nb))
    out += chr(k in bools ? 1 : 0);
if (len(out) % 2) out += "\0";
for (var k : slice(NUMS, 0, nn))
    out += numb(k in nums ? nums[k] : -1);
for (var o : offs) out += h(o);
out += table;
if (len(xb) || len(xn) || len(xs)) {
    if (len(out) % 2) out += "\0";
    var xtab = "", soffs = [];
    for (var k : xs) {
        push(soffs, len(xtab));
        xtab += xs[k] + "\0";
    }
    var noffs = [], ntab = "";
    for (var k : keys(xb) + keys(xn) + keys(xs)) {
        push(noffs, len(ntab));
        ntab += k + "\0";
    }
    var x = repeat("\1", len(xb));
    if (len(x) % 2) x += "\0";
    for (var k : xn) x += numb(xn[k]);
    for (var o : soffs + noffs) x += h(o);
    out += h(len(xb)) + h(len(xn)) + h(len(xs)) + h(len(xs) + len(xb) + len(xn) + len(xs)) +
           h(len(xtab) + len(ntab)) + x + xtab + ntab;
}
var first = split(names, "|")[0];
mkdirs(d + "/" + chr(first[0]));
if (!writefile(d + "/" + chr(first[0]) + "/" + first, out)) {
    eprintf("mkterminfo: %s/%c/%s: %s\n", d, first[0], first, strerror());
    exit(1);
}
