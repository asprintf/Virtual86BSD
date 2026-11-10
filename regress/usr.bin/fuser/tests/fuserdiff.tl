/*
 * fuserdiff.tl - compare fuser with psmisc fuser ("fuser" in PATH).
 *
 *     tl tests/fuserdiff.tl ./fuser
 *
 * For files the running processes use (/, the working directories, the
 * programs, the libraries they map and their open files), with no option,
 * -u and -c: each process ID with its use characters (both run with the
 * standard error on the standard output, so that each process ID is
 * followed by its characters).  psmisc pads with blanks (not compared),
 * writes "(unknown)" for a user ID without a name where POSIX wants the
 * number, and exits 1 when no process uses a file (not compared either);
 * -c is used with directories only.
 */

var ours = abspath(len(argv) > 0 ? argv[0] : "./fuser");
var USE = "([0-9]+)([a-zA-Z]*)[[:space:]]*(\\([^)]*\\))?";

/* a map: process ID -> its use characters and (user) */
func uses(prog, args) {
    var out = run([prog] + args, {err: "out"}).out;
    var colon = index(out, ":");
    var body = colon < 0 ? "" : substr(out, colon + 1);
    var r = {};
    for (var m = match(body, USE); m != nil; m = match(body, USE)) {
        var user = m[3] == nil ? "" : m[3];
        /* a user ID without a name: psmisc writes "(unknown)", POSIX the
           number */
        if (match(user, "^\\([0-9]+\\)$"))
            user = "(unknown)";
        r[m[1]] = m[2] + user;
        body = substr(body, matchat(body, USE)[1]);
    }
    return r;
}

var fileset = {"/": 1, "/etc/passwd": 1};
func add(path) {
    if (path != nil)
        fileset[path] = 1;
}
for (var pid : listdir("/proc")) {
    if (!match(pid, "^[0-9]+$"))
        continue;
    var p = "/proc/" + pid;
    add(readlink(p + "/exe"));
    add(readlink(p + "/cwd"));
    var fds = listdir(p + "/fd");
    if (fds == nil)
        continue;
    for (var fd : fds)
        add(readlink(p + "/fd/" + fd));
    var maps = readfile(p + "/maps");
    if (maps == nil)
        continue;
    for (var line : lines(maps)) {
        var parts = split(line);
        if (len(parts) >= 6 && startswith(parts[5], "/"))
            add(parts[5]);
    }
}
var files = filter(sorted(keys(fileset)), func (f) { return startswith(f, "/") && exists(f); });
files = slice(files, 0, min(150, len(files)));

var passed = 0, failed = 0;
for (var f : files) {
    /* -c with a file that is not a directory is unspecified: not compared */
    for (var opt : isdir(f) ? [[], ["-u"], ["-c"]] : [[], ["-u"]]) {
        var a = uses(ours, opt + [f]), b = uses("fuser", opt + [f]);
        /* the processes of the two runs (each fuser, and what came and
           went) are not compared */
        var same = true, only = 0;
        for (var p : keys(a))
            if (!haskey(b, p))
                only++;
            else if (a[p] != b[p])
                same = false;
        for (var p : keys(b))
            if (!haskey(a, p))
                only++;
        if (same && only <= 2)
            passed++;
        else {
            failed++;
            var diff = {};
            for (var p : keys(a) + keys(b))
                if (a[p] != b[p])
                    diff[p] = [b[p], a[p]];
            printf("DIFF: fuser %s %s: %q\n", join(opt, " "), f, diff);
        }
    }
}
printf("%d passed, %d failed\n", passed, failed);
exit(failed ? 1 : 0);
