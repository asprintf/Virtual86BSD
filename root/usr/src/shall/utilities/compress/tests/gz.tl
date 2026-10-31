/*
 * gz.tl - the gzip format (RFC 1951 and 1952) in tl, for compressdiff.tl:
 * a second implementation besides gzip itself, written apart from both.
 *
 *   crc32(s)             the CRC-32 of the string
 *   gunzip(s)            the data of the gzip members in s, or nil if it is
 *                        not well-formed (also: a wrong CRC or length)
 *   gzip_write(s, level) s in the gzip format: level 0 in stored blocks of
 *                        random sizes, the others in blocks with the fixed
 *                        codes, with matches searched harder as the level
 *                        rises
 */

var _CRC = [];
for (var n = 0; n < 256; n++) {
    var c = n;
    for (var k = 0; k < 8; k++)
        c = c & 1 ? 0xedb88320 ^ (c >> 1) : c >> 1;
    push(_CRC, c);
}

func crc32(s) {
    var c = 0xffffffff;
    var t = _CRC;
    for (var b : s)
        c = t[(c ^ b) & 255] ^ (c >> 8);
    return c ^ 0xffffffff;
}

/* the bases and the extra bits of the lengths (codes 257...) and distances */
var LBASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115,
             131, 163, 195, 227, 258];
var LEXT = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
var DBASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
             2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
var DEXT = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
var CLORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

/* ---------- inflate (after Mark Adler's puff.c) ---------- */

var _in, _inpos, _bitbuf, _bitcnt, _bad;

func getbits(n) {
    var v = _bitbuf;
    while (_bitcnt < n) {
        if (_inpos >= len(_in)) {
            _bad = 1;
            return 0;
        }
        v |= _in[_inpos++] << _bitcnt;
        _bitcnt += 8;
    }
    _bitbuf = v >> n;
    _bitcnt -= n;
    return v & ((1 << n) - 1);
}

/* the decoding table of the code lengths: {count, sym}, or nil if a
   length is over-subscribed */
func huffman(lengths) {
    var count = list(16, 0), offs = list(16, 0), sym = list(len(lengths), 0);
    for (var l : lengths)
        count[l]++;
    var left = 1;
    for (var i = 1; i < 16; i++) {
        left = (left << 1) - count[i];
        if (left < 0)
            return nil;
    }
    for (var i = 1; i < 15; i++)
        offs[i + 1] = offs[i] + count[i];
    for (var s = 0; s < len(lengths); s++)
        if (lengths[s])
            sym[offs[lengths[s]]++] = s;
    return {count: count, sym: sym};
}

/* a symbol, or -1 */
func decode(h) {
    var code = 0, first = 0, idx = 0;
    var count = h.count;
    for (var l = 1; l < 16; l++) {
        code |= getbits(1);
        if (_bad)
            return -1;
        var c = count[l];
        if (code - c < first)
            return h.sym[idx + (code - first)];
        idx += c;
        first = (first + c) << 1;
        code <<= 1;
    }
    return -1;
}

/* the symbols of a block with codes into out (a list of bytes); false if bad */
func codes(out, lc, dc) {
    for (;;) {
        var sym = decode(lc);
        if (sym < 0)
            return false;
        if (sym < 256)
            push(out, sym);
        else if (sym == 256)
            return true;
        else {
            sym -= 257;
            if (sym >= 29)
                return false;
            var n = LBASE[sym] + getbits(LEXT[sym]);
            var ds = decode(dc);
            if (ds < 0 || ds >= 30)
                return false;
            var dist = DBASE[ds] + getbits(DEXT[ds]);
            if (_bad || dist > len(out))
                return false;
            var from = len(out) - dist;
            for (var i = 0; i < n; i++)
                push(out, out[from + i]);
        }
    }
}

var _FIXED = nil;

func fixed_tables() {
    if (_FIXED == nil) {
        var l = list(144, 8) + list(112, 9) + list(24, 7) + list(8, 8);
        _FIXED = [huffman(l), huffman(list(30, 5))];
    }
    return _FIXED;
}

func dynamic_tables() {
    var nlen = getbits(5) + 257, ndist = getbits(5) + 1, ncode = getbits(4) + 4;
    if (_bad || nlen > 286 || ndist > 30)
        return nil;
    var cl = list(19, 0);
    for (var i = 0; i < ncode; i++)
        cl[CLORDER[i]] = getbits(3);
    var ch = huffman(cl);
    if (ch == nil)
        return nil;
    var lengths = [];
    while (len(lengths) < nlen + ndist) {
        var sym = decode(ch);
        if (sym < 0)
            return nil;
        if (sym < 16)
            push(lengths, sym);
        else {
            var v = 0, rep;
            if (sym == 16) {
                if (!len(lengths))
                    return nil;
                v = lengths[len(lengths) - 1];
                rep = 3 + getbits(2);
            } else if (sym == 17)
                rep = 3 + getbits(3);
            else
                rep = 11 + getbits(7);
            if (len(lengths) + rep > nlen + ndist)
                return nil;
            for (var i = 0; i < rep; i++)
                push(lengths, v);
        }
    }
    if (_bad || lengths[256] == 0)
        return nil;
    var lc = huffman(slice(lengths, 0, nlen)), dc = huffman(slice(lengths, nlen));
    return lc == nil || dc == nil ? nil : [lc, dc];
}

/* the raw DEFLATE data at _in[_inpos]: the bytes, or nil */
func inflate() {
    var out = [];
    _bitbuf = 0;
    _bitcnt = 0;
    _bad = 0;
    for (;;) {
        var last = getbits(1), type = getbits(2);
        if (_bad)
            return nil;
        if (type == 0) {
            _bitbuf = 0;
            _bitcnt = 0;
            if (_inpos + 4 > len(_in))
                return nil;
            var u = unpack("<HH", _in, _inpos);
            _inpos += 4;
            if (u[0] != (~u[1] & 0xffff) || _inpos + u[0] > len(_in))
                return nil;
            for (var i = 0; i < u[0]; i++)
                push(out, _in[_inpos + i]);
            _inpos += u[0];
        } else if (type == 3)
            return nil;
        else {
            var t = type == 1 ? fixed_tables() : dynamic_tables();
            if (t == nil || !codes(out, t[0], t[1]))
                return nil;
        }
        if (last)
            break;
    }
    _bitbuf = 0;            /* the rest of the byte is padding */
    _bitcnt = 0;
    return out;
}

func bytes_str(l) {
    return join(apply(l, chr), "");
}

func gunzip(s) {
    _in = s;
    _inpos = 0;
    var data = "";
    do {
        if (_inpos + 10 > len(s) || s[_inpos] != 0x1f || s[_inpos + 1] != 0x8b || s[_inpos + 2] != 8)
            return nil;
        var flg = s[_inpos + 3];
        _inpos += 10;
        if (flg & 4) {          /* FEXTRA */
            if (_inpos + 2 > len(s))
                return nil;
            _inpos += 2 + unpack("<H", s, _inpos)[0];
        }
        for (var f : [8, 16])   /* FNAME, FCOMMENT: ended by a NUL */
            if (flg & f) {
                var z = index(s, "\0", _inpos);
                if (z < 0)
                    return nil;
                _inpos = z + 1;
            }
        if (flg & 2)            /* FHCRC */
            _inpos += 2;
        if (_inpos > len(s))
            return nil;
        var out = inflate();
        if (out == nil || _inpos + 8 > len(s))
            return nil;
        var m = bytes_str(out);
        var t = unpack("<II", s, _inpos);
        _inpos += 8;
        if (t[0] != crc32(m) || t[1] != (len(m) & 0xffffffff))
            return nil;
        data += m;
    } while (_inpos < len(s));
    return data;
}

/* ---------- writing ---------- */

var _out, _obuf, _onbits;

func putbits(v, n) {
    _obuf |= v << _onbits;
    _onbits += n;
    while (_onbits >= 8) {
        push(_out, _obuf & 255);
        _obuf >>= 8;
        _onbits -= 8;
    }
}

func flushbits() {
    if (_onbits > 0)
        push(_out, _obuf & 255);
    _obuf = 0;
    _onbits = 0;
}

/* a Huffman code, sent from its first bit */
func puthuff(code, n) {
    var r = 0;
    for (var i = 0; i < n; i++) {
        r = (r << 1) | (code & 1);
        code >>= 1;
    }
    putbits(r, n);
}

func putlit(v) {
    if (v < 144)
        puthuff(0x30 + v, 8);
    else if (v < 256)
        puthuff(0x190 + v - 144, 9);
    else if (v < 280)
        puthuff(v - 256, 7);
    else
        puthuff(0xc0 + v - 280, 8);
}

func putmatch(n, dist) {
    var c = 28;
    while (LBASE[c] > n)
        c--;
    putlit(257 + c);
    putbits(n - LBASE[c], LEXT[c]);
    c = 29;
    while (DBASE[c] > dist)
        c--;
    puthuff(c, 5);
    putbits(dist - DBASE[c], DEXT[c]);
}

/* s[from:to] as a block with the fixed codes; head and prev are the hash chains */
func fixed_block(s, from, to, last, chain, head, prev) {
    putbits(last, 1);
    putbits(1, 2);
    var n = len(s);
    var i = from;
    while (i < to) {
        var best = 0, bdist = 0;
        if (i + 2 < n) {
            var h = ((s[i] << 10) ^ (s[i + 1] << 5) ^ s[i + 2]) & 0x7fff;
            var max = min(258, to - i);
            var p = head[h];
            for (var k = 0; k < chain && p >= 0 && i - p <= 32768; k++) {
                var l = 0;
                while (l < max && s[p + l] == s[i + l])
                    l++;
                if (l > best) {
                    best = l;
                    bdist = i - p;
                }
                p = prev[p & 0x7fff];
            }
        }
        var step = best >= 3 ? best : 1;
        if (best >= 3)
            putmatch(best, bdist);
        else
            putlit(s[i]);
        for (var j = i; j < i + step; j++)
            if (j + 2 < n) {
                var h = ((s[j] << 10) ^ (s[j + 1] << 5) ^ s[j + 2]) & 0x7fff;
                prev[j & 0x7fff] = head[h];
                head[h] = j;
            }
        i += step;
    }
    putlit(256);
}

func gzip_write(s, level) {
    _out = [];
    _obuf = 0;
    _onbits = 0;
    var n = len(s);
    if (level == 0) {
        var i = 0;
        do {
            var k = min(n - i, rand(1, 65535));
            var last = i + k == n;
            putbits(last, 1);
            putbits(0, 2);
            flushbits();
            for (var b : pack("<HH", k, ~k & 0xffff))
                push(_out, b);
            for (var j = 0; j < k; j++)
                push(_out, s[i + j]);
            i += k;
        } while (i < n);
    } else {
        var head = list(0x8000, -1), prev = list(0x8000, -1);
        var i = 0;
        do {
            var k = min(n - i, rand(1, 50000));
            fixed_block(s, i, i + k, i + k == n, level * 4, head, prev);
            i += k;
        } while (i < n);
        flushbits();
    }
    var hdr = pack("<BBBBIBB", 0x1f, 0x8b, 8, 0, 0, level == 9 ? 2 : level == 1 ? 4 : 0, 3);
    return hdr + bytes_str(_out) + pack("<II", crc32(s), n & 0xffffffff);
}
