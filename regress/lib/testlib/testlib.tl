/*
 * testlib.tl - what the tl test programs share (include "../../testlib/testlib.tl").
 *
 *   utf8_locale()        a UTF-8 locale: $UTF8_LOCALE, or C.UTF-8, or any one
 *                        that locale -a lists; "" if there is none (POSIX
 *                        names no such locale)
 *   compiler()           the C compiler: $TESTCC or $CC, or the first of c17,
 *                        c99, cc
 *   cc_env()             a map: CC, and the optional flags the compiler
 *                        accepts: CWARN (-std=c99 -Wall -Werror), CPEDANTIC
 *                        (the same with -pedantic), CQUIET (-w), and CSTDS:
 *                        those of c99 c11 c17 that it takes with -std= and the
 *                        strict options; "" where not
 *   build(name, outdir)  compiles testlib/NAME.c into outdir: the path of the
 *                        program
 *   non_ascii(s)         does the string hold a byte outside ASCII?
 *   which(name)          the path of a program in PATH, or nil
 *   shquote(s)           s quoted for the shell
 *   check(name, cond)    counts a test: passed, or failed (FAIL name)
 *   tests_done(name)     "name: N passed, M failed"; the exit status
 */

var TESTLIB = dirname(__FILE__);

func which(name) {
    if (index(name, "/") >= 0)
        return access(name, "x") ? name : nil;
    var path = getenv("PATH");
    for (var d : split(path == nil ? "/usr/bin:/bin" : path, ":")) {
        var p = (d == "" ? "." : d) + "/" + name;
        if (isfile(p) && access(p, "x"))
            return p;
    }
    return nil;
}

func shquote(s) {
    return "'" + replace(s, "'", "'\\''") + "'";
}

func utf8_locale() {
    var loc = getenv("UTF8_LOCALE");
    if (loc != nil && loc != "")
        return loc;
    var r = run(["locale", "-a"], {err: "null"});
    var names = split(r.out);
    for (var n : names)
        if (lower(n) == "c.utf-8" || lower(n) == "c.utf8")
            return n;
    for (var n : names)
        if (endswith(lower(n), ".utf-8") || endswith(lower(n), ".utf8"))
            return n;
    return "";
}

func compiler() {
    var cc = getenv("TESTCC");
    if (cc == nil || cc == "")
        cc = getenv("CC");
    if (cc != nil && cc != "")
        return cc;
    for (var c : ["c17", "c99", "cc"])
        if (which(c) != nil)
            return c;
    return "cc";
}

func cc_env() {
    var cc = compiler();
    var d = mkdtemp("ccenv.");
    writefile(d + "/t.c", "int main(void) { return 0; }\n");
    var accepts = func (cc, d, flags) {
        return run(cc + " " + flags + " -o " + shquote(d + "/t") + " " + shquote(d + "/t.c"),
                   {out: "null", err: "null"}).status == 0;
    };
    var warn = "-std=c99 -Wall -Werror";
    var stds = [];
    for (var s : ["c99", "c11", "c17"])
        if (accepts(cc, d, "-std=" + s + " -pedantic -Wall -Wextra -Werror"))
            push(stds, s);
    var env = {CC: cc, CWARN: accepts(cc, d, warn) ? warn : "",
               CPEDANTIC: accepts(cc, d, warn + " -pedantic") ? warn + " -pedantic" : "",
               CQUIET: accepts(cc, d, "-w") ? "-w" : "", CSTDS: join(stds, " ")};
    rmtree(d);
    return env;
}

func build(name, outdir) {
    var out = outdir + "/" + name;
    var r = run(compiler() + " -o " + shquote(out) + " " + shquote(TESTLIB + "/" + name + ".c"), {out: "inherit", err: "inherit"});
    if (r.status != 0)
        error("cannot compile %s/%s.c", TESTLIB, name);
    return out;
}

func non_ascii(s) {
    for (var c : s)
        if (c > 127)
            return true;
    return false;
}

var _tests_passed = 0, _tests_failed = 0;

func check(name, cond) {
    if (cond)
        _tests_passed++;
    else {
        _tests_failed++;
        printf("FAIL %s\n", name);
    }
    return cond;
}

func tests_done(name) {
    printf("%s: %d passed, %d failed\n", name, _tests_passed, _tests_failed);
    return _tests_failed ? 1 : 0;
}
