/*
 * elfinfo.tl - what the tests of c17 need to know of an ELF-64 file
 * (little-endian, as c17 writes them)
 *
 *     tl elfinfo.tl FILE           the type, sections, segments, symbols
 *     tl elfinfo.tl -x NAME FILE   the bytes of section NAME, in hexadecimal
 *     tl elfinfo.tl -r NAME FILE   the relocations of section NAME
 *
 * The first form writes lines such as
 *     type EXEC
 *     entry 0x401000
 *     section .text PROGBITS AX
 *     segment LOAD R E
 *     interp /lib64/ld-linux-x86-64.so.2
 *     needed libc.so.6
 *     runpath /opt/lib
 *     soname libx.so
 *     symbol main FUNC GLOBAL .text 0x401130
 *     dynsym counter OBJECT GLOBAL .data
 */

var SHT = {0: "NULL", 1: "PROGBITS", 2: "SYMTAB", 3: "STRTAB", 4: "RELA", 5: "HASH",
           6: "DYNAMIC", 7: "NOTE", 11: "DYNSYM", 0x6ffffffe: "VERNEED", 0x6fffffff: "VERSYM",
           8: "NOBITS", 14: "INIT_ARRAY", 15: "FINI_ARRAY", 16: "PREINIT_ARRAY", 17: "GROUP"};
var PT = {1: "LOAD", 2: "DYNAMIC", 3: "INTERP", 6: "PHDR", 7: "TLS", 0x6474e551: "GNU_STACK"};
var STT = {0: "NOTYPE", 1: "OBJECT", 2: "FUNC", 3: "SECTION", 4: "FILE", 5: "COMMON", 6: "TLS"};
var STB = {0: "LOCAL", 1: "GLOBAL", 2: "WEAK"};
var RT = {1: "R_X86_64_64", 2: "R_X86_64_PC32", 4: "R_X86_64_PLT32", 9: "R_X86_64_GOTPCREL",
          10: "R_X86_64_32", 11: "R_X86_64_32S", 19: "R_X86_64_TLSGD", 20: "R_X86_64_TLSLD",
          21: "R_X86_64_DTPOFF32", 22: "R_X86_64_GOTTPOFF", 23: "R_X86_64_TPOFF32",
          24: "R_X86_64_PC64", 41: "R_X86_64_GOTPCRELX", 42: "R_X86_64_REX_GOTPCRELX"};

func cstr(d, off) {
    var e = index(d, "\0", off);
    return substr(d, off, e - off);
}

var mode = nil, which = nil, path;
if (len(argv) == 3) {
    mode = argv[0]; which = argv[1]; path = argv[2];
} else if (len(argv) == 1) {
    path = argv[0];
} else {
    eprintf("usage: elfinfo.tl [-x|-r section] file\n");
    exit(2);
}
var d = readfile(path);
if (d == nil || substr(d, 0, 4) != "\x7fELF" || d[4] != 2) {
    eprintf("%s: not an ELF-64 file\n", path);
    exit(1);
}
var h = unpack("<HHIQQQIHHHHHH", d, 16);
var etype = h[0], entry = h[3], phoff = h[4], shoff = h[5];
var phnum = h[9], shnum = h[11], shstrndx = h[12];
var secs = [];
for (var i = 0; i < shnum; i++) {
    var s = unpack("<IIQQQQIIQQ", d, shoff + 64 * i);
    push(secs, {name: s[0], type: s[1], flags: s[2], addr: s[3], off: s[4], size: s[5],
                link: s[6], info: s[7], entsize: s[9]});
}
if (shnum) {
    var strs = secs[shstrndx];
    for (var s : secs)
        s.name = cstr(d, strs.off + s.name);
}
func section(name) {
    for (var s : secs)
        if (s.name == name)
            return s;
    eprintf("%s: no section %s\n", path, name);
    exit(1);
}
func symbols(t) {
    for (var s : secs)
        if (s.type == t) {
            var str = secs[s.link], res = [];
            for (var k = 0; k < s.size / 24; k++) {
                var y = unpack("<IBBHQQ", d, s.off + 24 * k);
                push(res, {name: cstr(d, str.off + y[0]), bind: y[1] >> 4, type: y[1] & 15,
                           shndx: y[3], value: y[4], size: y[5]});
            }
            return res;
        }
    return [];
}

if (mode == "-x") {
    var s = section(which);
    print(s.type == 8 ? "" : hex(substr(d, s.off, s.size)));
    exit(0);
}
if (mode == "-r") {
    var s = section(".rela" + which);
    var syms = symbols(2);
    for (var k = 0; k < s.size / 24; k++) {
        var r = unpack("<QQq", d, s.off + 24 * k);
        var y = syms[r[1] >> 32];
        var name = y.type == 3 ? secs[y.shndx].name : y.name;
        printf("%x %s %s %d\n", r[0], get(RT, r[1] & 0xffffffff, sprintf("%d", r[1] & 0xffffffff)),
               name, r[2]);
    }
    exit(0);
}

print("type", get({1: "REL", 2: "EXEC", 3: "DYN"}, etype, sprintf("%d", etype)));
if (etype == 2 || (etype == 3 && entry != 0))
    printf("entry 0x%x\n", entry);
for (var s : secs) {
    if (s.type == 0)
        continue;
    var f = "";
    if (s.flags & 2) f += "A";
    if (s.flags & 1) f += "W";
    if (s.flags & 4) f += "X";
    if (s.flags & 0x400) f += "T";
    print("section", s.name, get(SHT, s.type, sprintf("0x%x", s.type)), f);
}
for (var i = 0; i < phnum; i++) {
    var p = unpack("<IIQQQQQQ", d, phoff + 56 * i);
    var f = (p[1] & 4 ? "R" : "") + (p[1] & 2 ? " W" : "") + (p[1] & 1 ? " E" : "");
    print("segment", get(PT, p[0], sprintf("0x%x", p[0])), f);
}
for (var i = 0; i < phnum; i++) {
    var p = unpack("<IIQQQQQQ", d, phoff + 56 * i);
    if (p[0] == 3)
        print("interp", cstr(d, p[2]));
}
for (var s : secs)
    if (s.type == 6) {
        var str = secs[s.link];
        for (var k = 0; k < s.size / 16; k++) {
            var e = unpack("<QQ", d, s.off + 16 * k);
            if (e[0] == 1)
                print("needed", cstr(d, str.off + e[1]));
            else if (e[0] == 29)
                print("runpath", cstr(d, str.off + e[1]));
            else if (e[0] == 14)
                print("soname", cstr(d, str.off + e[1]));
            else if (e[0] == 0)
                break;
        }
    }
for (var t : [2, 11])
    for (var y : symbols(t)) {
        if (y.name == "" || y.type == 3 || y.type == 4)
            continue;
        var where = y.shndx == 0 ? "UND" : y.shndx == 0xfff1 ? "ABS" : y.shndx == 0xfff2 ? "COM" :
                    y.shndx < len(secs) ? secs[y.shndx].name : "?";
        if (t == 2)
            printf("symbol %s %s %s %s 0x%x\n", y.name, STT[y.type], STB[y.bind], where, y.value);
        else
            printf("dynsym %s %s %s %s\n", y.name, get(STT, y.type, "?"), STB[y.bind], where);
    }
