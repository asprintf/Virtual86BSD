/*
 * treecmp.tl - compares two file trees: the types, modes, contents, link
 * targets, modification times (to the nanosecond; -s: to the second),
 * device numbers and hard links.  Writes the differences; exit status 1
 * if there are any.
 *
 *     tl treecmp.tl [-s] DIR1 DIR2
 */

var ns = true;
var args = argv;
if (len(args) && args[0] == "-s") {
    ns = false;
    args = slice(args, 1);
}

/* the files under root (by their names relative to it), and the groups of
   names of the regular files with more than one link */
func scan(root) {
    var out = {}, inos = {};
    var todo = [""];
    while (len(todo)) {
        var rd = pop(todo, 0);
        var dir = rd == "" ? root : root + "/" + rd;
        for (var n : listdir(dir)) {
            var rel = rd == "" ? n : rd + "/" + n;
            var p = root + "/" + rel;
            var st = lstat(p);
            var d = {type: st.type, mode: st.mode};
            if (st.type == "f") {
                d.data = readfile(p);
                if (st.nlink > 1) {
                    var k = str(st.dev) + ":" + str(st.ino);
                    if (!haskey(inos, k))
                        inos[k] = [];
                    push(inos[k], rel);
                }
            }
            if (st.type == "l")
                d.link = readlink(p);
            else
                d.mtime = ns ? st.mtime_ns : st.mtime;
            if (st.type == "c" || st.type == "b")
                d.rdev = st.rdev;
            if (st.type == "d")
                push(todo, rel);
            out[rel] = d;
        }
    }
    var groups = [];
    for (var k : inos)
        push(groups, sorted(inos[k]));
    return [out, sorted(groups)];
}

var A = scan(args[0]), B = scan(args[1]);
var a = A[0], b = B[0];
var bad = 0;
var names = keys(a);
for (var k : b)
    if (!haskey(a, k))
        push(names, k);
for (var k : sorted(names)) {
    if (!haskey(a, k) || !haskey(b, k)) {
        printf("only in %s %s\n", haskey(a, k) ? args[0] : args[1], k);
        bad = 1;
        continue;
    }
    var fields = keys(a[k]);
    for (var f : b[k])
        if (!haskey(a[k], f))
            push(fields, f);
    for (var f : fields) {
        var va = get(a[k], f, nil), vb = get(b[k], f, nil);
        if (va != vb) {
            if (f == "data") {
                va = len(va == nil ? "" : va);
                vb = len(vb == nil ? "" : vb);
            }
            printf("%s: %s differs: %s vs %s\n", k, f, str(va), str(vb));
            bad = 1;
        }
    }
}
if (A[1] != B[1]) {
    printf("hard links differ: %s %s\n", repr(A[1]), repr(B[1]));
    bad = 1;
}
exit(bad);
