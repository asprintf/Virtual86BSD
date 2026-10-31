/*
 * elfinfo.tl - the sections and symbols of an ELF file, for the tests of
 * strip
 *
 *     tl elfinfo.tl FILE
 *
 * writes "sections: NAME..." (the section names, in order) and "symbols:
 * NAME..." (the named symbols of the symbol table, in order; "none" when
 * there is no symbol table), and "ok" when every relocation section's
 * symbols and target are in range.
 */

var d = readfile(argv[0]);
if (d == nil) {
    eprintf("elfinfo: %s: %s\n", argv[0], strerror());
    exit(1);
}
var is64 = d[4] == 2;
var e = d[5] == 2 ? ">" : "<";
var shoff, shentsize, shnum, shstrndx, v;
if (is64) {
    shoff = unpack(e + "Q", d, 40)[0];
    v = unpack(e + "HHH", d, 58);
} else {
    shoff = unpack(e + "I", d, 32)[0];
    v = unpack(e + "HHH", d, 46);
}
shentsize = v[0];
shnum = v[1];
shstrndx = v[2];
var secs = [];
for (var i = 0; i < shnum; i++) {
    var f = unpack(e + (is64 ? "IIQQQQIIQQ" : "IIIIIIIIII"), d, shoff + i * shentsize);
    push(secs, {name: f[0], type: f[1], off: f[4], size: f[5], link: f[6], info: f[7]});
}
var so = secs[shstrndx].off;

func cstr(d, base, o) {
    return substr(d, base + o, index(d, "\0", base + o) - (base + o));
}

for (var s : secs)
    s.n = cstr(d, so, s.name);
var names = [];
for (var s : slice(secs, 1))
    push(names, s.n);
printf("sections: %s\n", join(names, " "));
var st = filter(secs, func (s) { return s.type == 2; });
if (!len(st))
    print("symbols: none");
else {
    var s = st[0];
    var es = is64 ? 24 : 16;
    names = [];
    for (var k = 1; k < s.size / es; k++) {
        var no = unpack(e + "I", d, s.off + k * es)[0];
        if (no)
            push(names, cstr(d, secs[s.link].off, no));
    }
    printf("symbols: %s\n", join(names, " "));
}
var ok = true;
for (var s : secs) {
    if (s.type == 4 || s.type == 9) {
        var es = s.type == 4 ? (is64 ? 24 : 12) : (is64 ? 16 : 8);
        var nsym = s.link ? secs[s.link].size / (is64 ? 24 : 16) : 0;
        ok = ok && 0 < s.info && s.info < len(secs);
        for (var k = 0; k < s.size / es; k++) {
            var info = unpack(e + (is64 ? "Q" : "I"), d, s.off + k * es + (is64 ? 8 : 4))[0];
            var sym = is64 ? info >> 32 : info >> 8;
            ok = ok && sym < nsym;
        }
    }
}
print(ok ? "ok" : "bad");
