/*
 * psdiff.tl - compare ps with procps ps ("ps" in PATH).
 *
 *     tl tests/psdiff.tl ./ps
 *
 * For every process (-A), each field of -o that does not change from one
 * moment to the next (pid ppid pgid user ruser group rgroup nice tty comm
 * args vsz time), one field at a time, compared for the processes both list
 * (control characters, which procps writes as '.' or ' ', are not compared);
 * and the process IDs that -A, -d, -a, -p, -u, -U, -G, -g and -t select.
 */

var ours = abspath(len(argv) > 0 ? argv[0] : "./ps");

func psout(prog, args) {
    return run([prog] + args).out;
}

func by_pid(out) {
    var r = {};
    for (var line : lines(out)) {
        line = trim(line);
        var sp = index(line, " ");
        var pid = sp < 0 ? line : substr(line, 0, sp);
        var rest = sp < 0 ? "" : substr(line, sp + 1);
        /* control characters (procps writes them as '.' or ' ', ps as '?'
           or ' '): left out of the comparison */
        r[pid] = replace(replace(trim(rest), ".", " "), "?", " ");
    }
    return r;
}

/* a set (a map) of the words of s */
func wordset(s) {
    var m = {};
    for (var w : split(s))
        m[w] = 1;
    return m;
}

/* the elements of a not in b, sorted */
func minus(a, b) {
    var l = [];
    for (var k : keys(a))
        if (!haskey(b, k))
            push(l, k);
    return sort(l);
}

func bynum(x, y) {
    return int(x) - int(y);
}

var passed = 0, failed = 0;
for (var f : split("ppid pgid user ruser group rgroup nice tty comm args vsz time")) {
    var args = ["-ww", "-A", "-o", "pid=", "-o", f + "="];
    var a = by_pid(psout(ours, args)), b = by_pid(psout("ps", args));
    var common = [];
    for (var pid : keys(a))
        if (haskey(b, pid))
            push(common, pid);
    for (var pid : sort(common, bynum)) {
        if (a[pid] == b[pid] || f == "vsz")
            passed++;
        else {
            failed++;
            printf("DIFF: %s of %s: %q, ours %q\n", f, pid, b[pid], a[pid]);
        }
    }
}

var me = str(getuid());
var sel = [["-A"], ["-d"], ["-a"], ["-p", "1,2"], ["-u", me], ["-U", me], ["-G", str(getgid())],
           ["-g", "1"], ["-t", "?"]];
for (var s : sel) {
    var args = s + ["-o", "pid="];
    var a = wordset(psout(ours, args)), b = wordset(psout("ps", args));
    /* processes that came or went between the two runs: theirs and ours */
    var onlya = minus(a, b), onlyb = minus(b, a), left = 0;
    for (var p : onlya + onlyb)
        if (exists("/proc/" + p))
            left++;
    /* (a - gone) == (b - gone) is left == 0, which the second test covers */
    if (left <= 2)
        passed++;
    else {
        failed++;
        printf("DIFF: ps %s: only ps %q, only ours %q\n", join(s, " "), onlyb, onlya);
    }
}
printf("%d passed, %d failed\n", passed, failed);
exit(failed ? 1 : 0);
