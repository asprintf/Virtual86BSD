/*
 * mkpages.tl - the pages of man, made from the standard's own pages
 *
 *     tl mkpages.tl SPEC OUTDIR
 *
 * SPEC is the directory of the HTML edition of POSIX.1-2024 (the one with
 * utilities/ in it).  For each utility (each page of utilities/ with a NAME
 * section) OUTDIR/NAME gets the page as plain text: the sections from NAME
 * to SEE ALSO (CHANGE HISTORY left out), the paragraphs filled to 72
 * columns, examples as they are.  OUTDIR/whatis gets the summary database
 * of man -k: "name - purpose" from each NAME section, sorted.
 *
 * The pages are the standard's text, which is copyrighted (IEEE and The
 * Open Group); they are made where the standard is, and not distributed
 * with these utilities.  tl is the language of the tests (../testlib/tl.c).
 */

var WIDTH = 72;
var NBSP = "\302\240";

/* ---- HTML: a tree of nodes {tag, attrs, kids, parent}; text as strings ---- */

var VOID = ["br", "img", "hr", "meta", "link", "basefont", "input", "col"];

/* the character references of the pages; numbers 128 to 159 as Windows-1252
   (as browsers take them, per the HTML standard) */
var ENTITIES = {amp: "&", lt: "<", gt: ">", quot: "\"", apos: "'", nbsp: NBSP, copy: "©", reg: "®",
                trade: "™", mdash: "—", ndash: "–", hellip: "…", deg: "°", times: "×", divide: "÷",
                para: "¶", sect: "§", middot: "·", laquo: "«", raquo: "»", lsquo: "‘", rsquo: "’",
                ldquo: "“", rdquo: "”", bull: "•", dagger: "†", Dagger: "‡", not: "¬", cent: "¢"};
var CP1252 = [0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021, 0x2c6, 0x2030, 0x160, 0x2039,
              0x152, 0x8d, 0x17d, 0x8f, 0x90, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
              0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d, 0x17e, 0x178];

func unescape(s) {
    if (index(s, "&") < 0)
        return s;
    var out = "", i = 0;
    for (;;) {
        var j = index(s, "&", i);
        if (j < 0)
            break;
        out += substr(s, i, j - i);
        var m = match(substr(s, j, 40), "^&(#[0-9]+|#[xX][0-9a-fA-F]+|[a-zA-Z][a-zA-Z0-9]*);");
        var rep = nil;
        if (m != nil) {
            var name = m[1];
            if (startswith(name, "#")) {
                var c = startswith(lower(name), "#x") ? int(substr(name, 2), 16) : int(substr(name, 1));
                if (c >= 128 && c < 160)
                    c = CP1252[c - 128];
                rep = utf8(c);
            } else
                rep = get(ENTITIES, name, nil);
        }
        if (rep == nil) {
            out += "&";
            i = j + 1;
        } else {
            out += rep;
            i = j + len(m[0]);
        }
    }
    return out + substr(s, i);
}

func node(tag, attrs, parent) {
    return {tag: tag, attrs: attrs, kids: [], parent: parent};
}

func parse_html(s) {
    var root = node("root", {}, nil), cur = root;
    var i = 0, n = len(s), text = "";
    while (i < n) {
        var j = index(s, "<", i);
        if (j < 0)
            j = n;
        text += substr(s, i, j - i);
        i = j;
        if (i >= n)
            break;
        var rest = substr(s, i, 512);
        var m;
        if (startswith(rest, "<!--")) {
            var e = index(s, "-->", i + 4);
            i = e < 0 ? n : e + 3;
            continue;
        }
        if ((m = match(rest, "^<[!?][^>]*>")) != nil) {
            i += len(m[0]);
            continue;
        }
        if ((m = match(rest, "^</([a-zA-Z][^\t\n\r\f />]*)[^>]*>")) != nil) {
            if (text != "") {
                push(cur.kids, unescape(text));
                text = "";
            }
            var tag = lower(m[1]);
            /* the open element of that name (the root has no parent) */
            var k = cur;
            while (k.parent != nil && k.tag != tag)
                k = k.parent;
            if (k.parent != nil)
                cur = k.parent;
            i += len(m[0]);
            continue;
        }
        if ((m = match(rest, "^<([a-zA-Z][^\t\n\r\f />]*)")) == nil) {
            text += "<";
            i++;
            continue;
        }
        /* a start tag: its attributes */
        var tag = lower(m[1]);
        var p = i + len(m[0]), attrs = {}, selfclose = false;
        for (;;) {
            var a = match(substr(s, p, 1024), "^[\t\n\r\f /]*([^\t\n\r\f />=][^\t\n\r\f />=]*)([\t\n\r\f ]*=[\t\n\r\f ]*(\"[^\"]*\"|'[^']*'|[^\t\n\r\f >]*))?");
            if (a == nil || a[1] == nil)
                break;
            var v = a[3];
            if (v != nil && (startswith(v, "\"") || startswith(v, "'")))
                v = substr(v, 1, len(v) - 2);
            attrs[lower(a[1])] = v == nil ? nil : unescape(v);
            p += len(a[0]);
        }
        var e = match(substr(s, p, 64), "^[\t\n\r\f ]*(/?)>");
        if (e == nil) {
            text += "<";        /* not a tag after all */
            i++;
            continue;
        }
        selfclose = e[1] == "/";
        i = p + len(e[0]);
        if (text != "") {
            push(cur.kids, unescape(text));
            text = "";
        }
        if (selfclose) {
            push(cur.kids, node(tag, attrs, cur));
            continue;
        }
        if ((tag == "dt" || tag == "dd" || tag == "li" || tag == "p") && cur.tag == tag)
            cur = cur.parent;           /* an implied end tag */
        var nd = node(tag, attrs, cur);
        push(cur.kids, nd);
        if (!(tag in VOID))
            cur = nd;
        if (tag == "script" || tag == "style") {
            /* their content as it is, up to the end tag */
            var low = lower(s);
            var end = index(low, "</" + tag, i);
            if (end < 0)
                end = n;
            if (end > i)
                push(nd.kids, substr(s, i, end - i));
            i = end;
        }
    }
    if (text != "")
        push(cur.kids, unescape(text));
    return root;
}

func istext(k) {
    return typeof(k) == "string";
}

func attr(n, name) {
    var v = get(n.attrs, name, nil);
    return v == nil ? "" : v;
}

/* ---- text ---- */

/* the words of s: blanks (and no-break spaces) between them */
func words(s) {
    return filter(resplit(replace(s, NBSP, " "), "[ \t\n\r\f\v]+"), func (w) { return w != ""; });
}

func squeeze(s) {
    return join(words(s), " ");
}

/* the length in characters (UTF-8) */
func clen(s) {
    var c = 0;
    for (var i = 0; i < len(s); i++)
        if ((s[i] & 0xc0) != 0x80)
            c++;
    return c;
}

/* a line whose letters are all capitals (as a section title; the letters
   of Latin-1 and Greek as such, in UTF-8) */
var UPPER = "[A-Z]|\303[\200-\226\230-\236]|\316[\221-\241\243-\251]";
var LOWER = "[a-z]|\303[\237-\266\270-\277]|\316[\254-\277]|\317[\200-\216]";

func isupper(s) {
    return match(s, UPPER) != nil && match(s, LOWER) == nil;
}

/* the lines of a paragraph of single-spaced words, filled to width
   (no word broken) */
func wrap(text, width, first, indent) {
    var lines = [], ws = split(text, " "), i = 0;
    while (i < len(ws)) {
        var ind = len(lines) ? indent : first;
        var w = width - len(ind), cur = ws[i], cl = clen(ws[i]);
        i++;
        while (i < len(ws) && cl + 1 + clen(ws[i]) <= w) {
            cur += " " + ws[i];
            cl += 1 + clen(ws[i]);
            i++;
        }
        push(lines, ind + cur);
    }
    return lines;
}

func Out(width) {
    return {lines: [], width: width};
}

func last(l) {
    return l[len(l) - 1];
}

func blank(o) {
    if (len(o.lines) && last(o.lines) != "" && !isupper(last(o.lines)))
        push(o.lines, "");
}

func para(o, text, indent, first) {
    text = squeeze(text);
    if (text == "")
        return;
    if (first == nil)
        first = indent;
    for (var l : wrap(text, o.width, repeat(" ", first), repeat(" ", indent)))
        push(o.lines, l);
}

func expandtabs(s) {
    if (index(s, "\t") < 0)
        return s;
    var out = "", col = 0;
    for (var i = 0; i < len(s); i++) {
        if (s[i] == '\t') {
            var k = 8 - col % 8;
            out += repeat(" ", k);
            col += k;
        } else {
            out += chr(s[i]);
            if ((s[i] & 0xc0) != 0x80)
                col++;
        }
    }
    return out;
}

func verbatim(o, text, indent) {
    for (var l : split(text, "\n"))
        push(o.lines, rtrim(repeat(" ", indent) + expandtabs(l)));
}

var INLINE = ["a", "i", "b", "em", "tt", "code", "sup", "sub", "span", "font", "strong", "img", "br", "var", "cite",
              "small", "big", "u", "samp", "kbd"];

/* the text of an inline node; br as a newline when keep_breaks */
func inline_text(n, keep_breaks) {
    if (istext(n))
        return n;
    if (n.tag == "img") {
        var alt = attr(n, "alt");
        return startswith(alt, "[Option") ? "" : alt;
    }
    if (n.tag == "br")
        return keep_breaks ? "\n" : " ";
    if (n.tag == "script" || n.tag == "style")
        return "";
    var s = "";
    for (var k : n.kids)
        s += inline_text(k, keep_breaks);
    return s;
}

/* the nodes of a tag under n (not under those) */
func iter_tag(n, tag) {
    var r = [];
    for (var k : n.kids) {
        if (istext(k))
            continue;
        if (k.tag == tag)
            push(r, k);
        else
            r += iter_tag(k, tag);
    }
    return r;
}

func strip_nl(s) {
    while (startswith(s, "\n"))
        s = substr(s, 1);
    while (endswith(s, "\n"))
        s = substr(s, 0, len(s) - 1);
    return s;
}

/* the block-level rendering of n's children */
func render(n, o, indent) {
    var buf = "";
    for (var k : n.kids) {
        if (istext(k) || k.tag in INLINE) {
            buf += inline_text(k);
            continue;
        }
        if (len(words(buf)))
            para(o, buf, indent);
        buf = "";
        var t = k.tag;
        if (t == "p") {
            blank(o);
            render(k, o, indent);
        } else if (t == "pre") {
            blank(o);
            verbatim(o, strip_nl(inline_text(k, true)), indent + 4);
            blank(o);
        } else if (t == "blockquote") {
            if ("synopsis" in attr(k, "class")) {
                for (var l : split(inline_text(k, true), "\n")) {
                    l = squeeze(l);
                    if (l != "")
                        para(o, l, indent + 8, indent + 4);
                }
            } else
                render(k, o, indent + 4);
        } else if (t == "dl")
            render(k, o, indent);
        else if (t == "dt") {
            blank(o);
            para(o, replace(inline_text(k), NBSP, " "), indent);
        } else if (t == "dd")
            render(k, o, indent + 4);
        else if (t == "ul" || t == "ol") {
            var num = 0;
            for (var li : k.kids) {
                if (istext(li) || li.tag != "li")
                    continue;
                num++;
                var mark = t == "ul" ? "*" : sprintf("%d.", num);
                var sb = Out(max(20, o.width - indent - len(mark) - 1));
                render(li, sb, 0);
                var body = sb.lines;
                while (len(body) && body[0] == "")
                    body = slice(body, 1);
                blank(o);
                for (var i = 0; i < len(body); i++) {
                    var l = body[i];
                    if (i == 0)
                        push(o.lines, l != "" ? repeat(" ", indent) + mark + " " + l : "");
                    else
                        push(o.lines, l != "" ? repeat(" ", indent + len(mark) + 1) + l : "");
                }
            }
        } else if (t == "table") {
            blank(o);
            for (var tr : iter_tag(k, "tr")) {
                var cells = [];
                for (var c : tr.kids)
                    if (!istext(c) && (c.tag == "td" || c.tag == "th")) {
                        var ct = squeeze(inline_text(c));
                        if (ct != "")
                            push(cells, ct);
                    }
                para(o, join(cells, "  "), indent + 8, indent + 4);
            }
            blank(o);
        } else if (t in ["div", "center", "li", "tbody", "thead", "tr", "td", "th", "h5", "h6", "caption"]) {
            if (t in ["h5", "h6", "caption"])
                blank(o);
            render(k, o, indent);
        } else if (t in ["script", "style", "hr"])
            ;
        else
            render(k, o, indent);
    }
    if (len(words(buf)))
        para(o, buf, indent);
}

/* [names, purpose, text] of a utility's page, or nil */
func page(path) {
    var root = parse_html(readfile(path));
    var bodies = iter_tag(root, "body");
    var body = len(bodies) ? bodies[0] : root;
    /* the sections: each h4.mansect and what follows up to the next */
    var sections = [], cur = nil;
    for (var k : body.kids) {
        if (!istext(k) && k.tag == "h4" && "mansect" in attr(k, "class")) {
            cur = [squeeze(inline_text(k)), []];
            push(sections, cur);
        } else if (cur != nil)
            push(cur[1], k);
    }
    if (!len(sections) || sections[0][0] != "NAME")
        return nil;
    var nm = node("x", {}, nil);
    nm.kids = sections[0][1];
    var m = match(squeeze(inline_text(nm)), "^([^ ]+(, [^ ]+)*) (—|-) (.*)$");
    if (m == nil)
        return nil;
    var o = Out(WIDTH);
    for (var sec : sections) {
        var title = sec[0], kids = sec[1];
        if (title == "CHANGE HISTORY")
            break;
        blank(o);
        push(o.lines, title);
        var holder = node("x", {}, nil), informative = false;
        for (var k : kids) {
            if (!istext(k) && k.tag == "div" && "informative" in inline_text(k)) {
                informative = true;
                if ("box" in attr(k, "class"))
                    continue;
            }
            push(holder.kids, k);
        }
        render(holder, o, 0);
        if (informative) {
            blank(o);
            push(o.lines, "The following sections are informative.");
        }
    }
    var ls = o.lines;
    while (len(ls) && ls[0] == "")
        ls = slice(ls, 1);
    while (len(ls) && last(ls) == "")
        pop(ls);
    var text = gsub(join(ls, "\n") + "\n", "\n\n\n+", "\n\n");
    return [m[1], m[4], text];
}

if (len(argv) != 2) {
    eprintf("usage: tl mkpages.tl SPEC OUTDIR\n");
    exit(1);
}
var spec = argv[0], outdir = argv[1];
var udir = spec + "/utilities";
if (!isdir(udir)) {
    eprintf("mkpages.tl: %s: no utilities directory\n", spec);
    exit(1);
}
mkdirs(outdir);
var whatis = [], done = {}, n = 0;
for (var f : listdir(udir)) {
    if (!endswith(f, ".html"))
        continue;
    var r = page(udir + "/" + f);
    if (r == nil)
        continue;
    var names = split(r[0], ", "), purpose = r[1], text = r[2];
    var stem = substr(f, 0, len(f) - 5);
    if (!(stem in names) && !match(stem, "^V[0-9]_"))
        push(names, stem);
    for (var name : names) {
        if (haskey(done, name))
            continue;
        done[name] = true;
        if (name == "whatis" || "/" in name || startswith(name, "."))
            continue;
        writefile(outdir + "/" + name, text);
        push(whatis, name + " - " + purpose);
        n++;
    }
}
writefile(outdir + "/whatis", join(apply(sorted(whatis), func (l) { return l + "\n"; }), ""));
printf("mkpages.tl: %d pages in %s\n", n, outdir);
