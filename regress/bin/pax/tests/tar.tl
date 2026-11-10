/*
 * tar.tl - tar and pax archives made and read for the regression cases of
 * pax, independently of it
 *
 *   tl tar.tl hdrs FILE
 *       the headers of an archive: for each member its type flag, name,
 *       size and mode, and the records of extended headers; then the size
 *       of the archive (names and records as text, a byte that is not
 *       UTF-8 as U+FFFD)
 *   tl tar.tl mkpax OUT RECORDS NAME [RECORDS NAME...]
 *       an archive of members NAME (2 bytes each; "-": none), each after an
 *       extended header of RECORDS, "key=value;key=value" (\xNN, \\, \n and
 *       \t in values; "g:" first makes a global header instead)
 *   tl tar.tl symlink FILE
 *       the member of an archive of mkpax made a symbolic link
 *   tl tar.tl flip FILE OFFSET
 *       one bit of the byte at OFFSET changed
 *   tl tar.tl huge OUT | records OUT | global OUT
 *       the archives of the case read-records, made as a common tar
 *       library makes them (pax format): a header of a file of 9 GB
 *       with records for its size, uid and mtime; a member whose records
 *       change its path and delete its uname; members after a global
 *       header
 */

var BLOCK = 512;
var NUL = "\0";

func zeros(n) {
    return repeat(NUL, n);
}

/* ---- the header block, as tarfile's _create_header() makes it ---- */

/* a string field: cut or padded with NULs */
func stn(s, n) {
    return len(s) >= n ? substr(s, 0, n) : s + zeros(n - len(s));
}

/* a number field: octal, digits-1 digits and a NUL */
func itn(v, digits) {
    return sprintf("%0*o", digits - 1, v) + NUL;
}

func checksum(h) {
    var c = 0;
    for (var i = 0; i < len(h); i++)
        c += h[i];
    return c;
}

/* info: name, mode, uid, gid, size, mtime, type, linkname, uname, gname */
func header(info) {
    var h = stn(get(info, "name", ""), 100) + itn(get(info, "mode", 0) & 07777, 8) + itn(get(info, "uid", 0), 8)
          + itn(get(info, "gid", 0), 8) + itn(get(info, "size", 0), 12) + itn(get(info, "mtime", 0), 12)
          + "        " + get(info, "type", "0") + stn(get(info, "linkname", ""), 100) + "ustar\00000"
          + stn(get(info, "uname", ""), 32) + stn(get(info, "gname", ""), 32) + zeros(8) + zeros(8) + zeros(155);
    h += zeros(BLOCK - len(h));
    return substr(h, 0, 148) + sprintf("%06o", checksum(h)) + NUL + substr(h, 155);
}

func payload(s) {
    var r = len(s) % BLOCK;
    return r ? s + zeros(BLOCK - r) : s;
}

/* a record: its length, which counts its own digits, first */
func record(k, v) {
    var body = k + "=" + v + "\n";
    var n = len(body) + 2;
    while (len(str(n)) + 1 + len(body) != n)
        n = len(str(n)) + 1 + len(body);
    return str(n) + " " + body;
}

/* an extended header (type "x") or a global one ("g") of the records, a
   list of [key, value], as tarfile's _create_pax_generic_header() */
func paxheader(records, type) {
    var r = "";
    for (var kv : records)
        r += record(kv[0], kv[1]);
    return header({name: "././@PaxHeader", type: type, size: len(r)}) + payload(r);
}

/* a member's headers as TarInfo.tobuf(format=PAX_FORMAT) makes them: the
   numbers that do not fit (or have a fraction, given as strings) go in
   records after those given; the ustar header has 0, or the rounded value */
func tarinfo(info, records) {
    var recs = copy(records);
    for (var nd : [["uid", 8], ["gid", 8], ["size", 12], ["mtime", 12]]) {
        var name = nd[0], v = get(info, name, 0), max = 1 << (3 * (nd[1] - 1));
        var text = nil;
        if (typeof(v) == "string") {    /* a fraction */
            text = v;
            info[name] = int(split(v, ".")[0]);
        } else if (v >= max) {
            text = str(v);
            info[name] = 0;
        }
        if (text != nil)
            push(recs, [name, text]);
    }
    return (len(recs) ? paxheader(recs, "x") : "") + header(info);
}

/* the end of an archive of tarfile: two zero blocks, then zeros to a
   multiple of 20 blocks */
func tarend(d) {
    d += zeros(2 * BLOCK);
    var r = len(d) % (20 * BLOCK);
    return r ? d + zeros(20 * BLOCK - r) : d;
}

/* ---- the headers of an archive ---- */

/* bytes as UTF-8 text: what is not UTF-8 replaced by U+FFFD, each maximal
   part of a sequence by one (the Unicode "maximal subpart" rule) */
func utf8_replace(s) {
    var out = "", i = 0, n = len(s);
    while (i < n) {
        var c = s[i];
        if (c < 0x80) {
            out += chr(c);
            i++;
            continue;
        }
        var need = 0, lo = 0x80, hi = 0xbf;
        if (c >= 0xc2 && c <= 0xdf)
            need = 1;
        else if (c >= 0xe0 && c <= 0xef) {
            need = 2;
            if (c == 0xe0)
                lo = 0xa0;
            if (c == 0xed)
                hi = 0x9f;
        } else if (c >= 0xf0 && c <= 0xf4) {
            need = 3;
            if (c == 0xf0)
                lo = 0x90;
            if (c == 0xf4)
                hi = 0x8f;
        }
        if (!need) {
            out += "\357\277\275";
            i++;
            continue;
        }
        var j = i + 1, k = 0;
        while (k < need && j < n && s[j] >= (k == 0 ? lo : 0x80) && s[j] <= (k == 0 ? hi : 0xbf)) {
            j++;
            k++;
        }
        if (k == need)
            out += substr(s, i, j - i);
        else
            out += "\357\277\275";
        i = j;
    }
    return out;
}

/* a field up to its first NUL (strip: and blanks at the end) */
func field(h, at, n) {
    var f = substr(h, at, n);
    var z = index(f, NUL);
    return z < 0 ? f : substr(f, 0, z);
}

func hdrs(path) {
    var d = readfile(path), i = 0;
    while (i + BLOCK <= len(d)) {
        var h = substr(d, i, BLOCK);
        if (h == zeros(BLOCK)) {
            printf("zero block at %d\n", i);
            break;
        }
        var name = rtrim(substr(h, 0, 100), NUL), pre = rtrim(substr(h, 345, 155), NUL);
        var sz = trim(substr(h, 124, 12), NUL + " ");
        sz = sz == "" ? 0 : int(sz, 8);
        var full = pre != "" ? pre + "/" + name : name;
        var flag = substr(h, 156, 1);
        printf("%s %s %d %s\n", flag, utf8_replace(full), sz, rtrim(substr(h, 100, 8), NUL + " "));
        if (flag == "x" || flag == "g")
            for (var rec : lines(utf8_replace(substr(d, i + BLOCK, sz))))
                printf("    %s\n", rec);
        i += flag >= "2" && flag <= "6" ? BLOCK : BLOCK + (sz + BLOCK - 1) / BLOCK * BLOCK;
    }
    printf("archive size %d\n", len(d));
}

/* ---- the archives of mkpax ---- */

/* a header of mkpax: mode 644, owner 0, time 0 */
func simple(name, size, flag) {
    var h = stn(substr(name, 0, 100), 100) + "0000644\0" + "0000000\0" + "0000000\0" + sprintf("%011o", size) + NUL
          + "00000000000\0" + "        " + flag;
    h += zeros(257 - len(h)) + "ustar\0" + "00";
    h += zeros(BLOCK - len(h));
    return substr(h, 0, 148) + sprintf("%06o", checksum(h)) + NUL + " " + substr(h, 156);
}

func unescape(v) {
    var out = "";
    for (var i = 0; i < len(v); i++) {
        if (v[i] == '\\' && i + 1 < len(v)) {
            var k = v[i + 1];
            if (k == 'x' && i + 3 < len(v)) {
                out += chr(int(substr(v, i + 2, 2), 16));
                i += 3;
                continue;
            }
            out += k == 'n' ? "\n" : k == 't' ? "\t" : chr(k);
            i++;
            continue;
        }
        out += chr(v[i]);
    }
    return out;
}

func mkpax(out, args) {
    var d = "";
    for (var i = 0; i + 1 < len(args); i += 2) {
        var recs = args[i], name = args[i + 1], flag = "x";
        if (startswith(recs, "g:")) {
            flag = "g";
            recs = substr(recs, 2);
        }
        var r = "";
        for (var kv : split(recs, ";")) {
            if (kv == "")
                continue;
            var e = index(kv, "=");
            r += record(substr(kv, 0, e), unescape(substr(kv, e + 1)));
        }
        if (r != "")
            d += simple("PaxHeaders/x", len(r), flag) + payload(r);
        if (name != "-")
            d += simple(name, 2, "0") + "x\n" + zeros(510);
    }
    writefile(out, d + zeros(1024));
}

/* the member (after an extended header, if there is one) made a symbolic
   link: type 2, size 0, the data and the rest dropped */
func symlink_member(path) {
    var d = readfile(path);
    var i = substr(d, 156, 1) == "x" ? 1024 : 0;
    var h = substr(d, i, BLOCK);
    h = substr(h, 0, 124) + sprintf("%011o", 0) + NUL + substr(h, 136, 12) + "        " + "2" + substr(h, 157);
    h = substr(h, 0, 148) + sprintf("%06o", checksum(h)) + NUL + " " + substr(h, 156);
    writefile(path, substr(d, 0, i) + h + zeros(1024));
}

var cmd = argv[0];
if (cmd == "hdrs")
    hdrs(argv[1]);
else if (cmd == "mkpax")
    mkpax(argv[1], slice(argv, 2));
else if (cmd == "symlink")
    symlink_member(argv[1]);
else if (cmd == "flip") {
    var d = readfile(argv[1]), at = int(argv[2]);
    writefile(argv[1], substr(d, 0, at) + chr(d[at] ^ 1) + substr(d, at + 1));
} else if (cmd == "huge")
    writefile(argv[1], tarinfo({name: "huge", mode: 0644, size: 9000000000, uid: 4000000000, mtime: "1234567890.5",
                                uname: "some user"}, [["comment", "x"]]));
else if (cmd == "records")
    writefile(argv[1], tarinfo({name: "name-in-header", mode: 0644, size: 3, uname: "hdruser"},
                               [["path", "from-record"], ["uname", ""]]) + "abc" + zeros(509) + zeros(1024));
else if (cmd == "global")
    writefile(argv[1], tarend(paxheader([["uname", "globaluser"]], "g") + header({name: "a", mode: 0644})
                              + header({name: "b", mode: 0644})));
else {
    eprintf("usage: tl tar.tl hdrs|mkpax|symlink|flip|huge|records|global ...\n");
    exit(2);
}
