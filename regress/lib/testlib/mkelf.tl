/*
 * mkelf.tl - write a relocatable ELF object, for the tests of nm, ar and
 * strip (in any class and byte order, which the compiler may not make)
 *
 *     tl mkelf.tl OUT [-32] [-be] < spec
 *
 * Each line of spec is a symbol: NAME BIND TYPE SECTION VALUE SIZE, with
 * BIND local, global or weak, TYPE notype, object, func, section, file or
 * ifunc, SECTION text, data, bss, rodata, debug, undef, abs or common.  The
 * sections .text (executable), .data (writable), .bss (no bits), .rodata,
 * and .debug_info (not loaded) are always there; a relocation section
 * .rela.text has one relocation for each symbol whose name starts with
 * "r_".
 */

var SECS = ["", ".text", ".data", ".bss", ".rodata", ".debug_info", ".rela.text", ".symtab", ".strtab", ".shstrtab"];
var IDX = {text: 1, data: 2, bss: 3, rodata: 4, debug: 5, undef: 0, abs: 0xfff1, common: 0xfff2};
var BIND = {local: 0, global: 1, weak: 2};
var TYPE = {notype: 0, object: 1, func: 2, section: 3, file: 4, ifunc: 10};

var out = filter(argv, func (a) { return !startswith(a, "-"); })[0];
var is64 = !("-32" in argv);
var e = "-be" in argv ? ">" : "<";
var syms = [];
for (var line : lines(readfile("-"))) {
    if (trim(line) == "")
        continue;
    var f = split(line);
    push(syms, {name: f[0], bind: BIND[f[1]], type: TYPE[f[2]], shndx: IDX[f[3]],
                value: int(f[4], 0), size: int(f[5], 0)});
}
/* locals first, as the format wants */
sort(syms, func (a, b) { return (a.bind != 0) - (b.bind != 0); });
var nlocal = 1 + len(filter(syms, func (y) { return y.bind == 0; }));
var strtab = "\0", names = [];
for (var y : syms) {
    push(names, y.name != "-" ? len(strtab) : 0);
    if (y.name != "-")
        strtab += y.name + "\0";
}
var symtab = repeat("\0", is64 ? 24 : 16);
for (var i = 0; i < len(syms); i++) {
    var y = syms[i];
    if (is64)
        symtab += pack(e + "IBBHQQ", names[i], (y.bind << 4) | y.type, 0, y.shndx, y.value, y.size);
    else
        symtab += pack(e + "IIIBBH", names[i], y.value, y.size, (y.bind << 4) | y.type, 0, y.shndx);
}
var rela = "";
for (var i = 0; i < len(syms); i++)
    if (startswith(syms[i].name, "r_")) {
        if (is64)
            rela += pack(e + "QQq", 0, ((i + 1) << 32) | 1, 0);
        else
            rela += pack(e + "IIi", 0, ((i + 1) << 8) | 1, 0);
    }
var shstr = "\0", shname = [];
for (var s : SECS) {
    push(shname, s != "" ? len(shstr) : 0);
    if (s != "")
        shstr += s + "\0";
}
var data = [nil, repeat("\x90", 64), repeat("\1", 16), "", "ro\0", "debug info", rela, symtab, strtab, shstr];
var ehsize = is64 ? 64 : 52;
var body = "", offs = list(len(SECS), 0);
for (var i = 1; i < len(SECS); i++) {
    while ((ehsize + len(body)) % 8)
        body += "\0";
    offs[i] = ehsize + len(body);
    body += data[i];
}
while ((ehsize + len(body)) % 8)
    body += "\0";
var shoff = ehsize + len(body);
/* type, flags, link, info, entsize */
var entsym = is64 ? 24 : 16, entrela = is64 ? 24 : 12;
var sh = [nil, [1, 6, 0, 0, 0], [1, 3, 0, 0, 0], [8, 3, 0, 0, 0], [1, 2, 0, 0, 0], [1, 0, 0, 0, 0],
          [4, 0x40, 7, 1, entrela], [2, 0, 8, nlocal, entsym], [3, 0, 0, 0, 0], [3, 0, 0, 0, 0]];
var shdrs = repeat("\0", is64 ? 64 : 40);
for (var i = 1; i < len(SECS); i++) {
    var h = sh[i];
    var size = i == 3 ? 32 : len(data[i]);
    shdrs += pack(e + (is64 ? "IIQQQQIIQQ" : "IIIIIIIIII"), shname[i], h[0], h[1], 0, offs[i], size, h[2], h[3], 8, h[4]);
}
var ident = "\177ELF" + chr(is64 ? 2 : 1) + chr(e == ">" ? 2 : 1) + "\1\0" + repeat("\0", 8);
var hdr;
if (is64)
    hdr = ident + pack(e + "HHIQQQIHHHHHH", 1, 62, 1, 0, 0, shoff, 0, 64, 0, 0, 64, len(SECS), 9);
else
    hdr = ident + pack(e + "HHIIIIIHHHHHH", 1, 3, 1, 0, 0, shoff, 0, 52, 0, 0, 40, len(SECS), 9);
if (!writefile(out, hdr + body + shdrs)) {
    eprintf("mkelf: %s: %s\n", out, strerror());
    exit(1);
}
