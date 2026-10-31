# helpers for the regression cases
# T SECONDS file...: set the times to SECONDS (a fraction allowed) after the
# Epoch, in the form of POSIX touch -d (UTC)
T() {
    _d=$(awk -v s="$1" 'BEGIN {
        f = ""; if (index(s, ".")) { f = substr(s, index(s, ".")); s = substr(s, 1, index(s, ".") - 1) }
        days = int(s / 86400); r = s - days * 86400
        z = days + 719468; era = int(z / 146097); doe = z - era * 146097
        yoe = int((doe - int(doe / 1460) + int(doe / 36524) - int(doe / 146096)) / 365)
        y = yoe + era * 400; doy = doe - (365 * yoe + int(yoe / 4) - int(yoe / 100))
        mp = int((5 * doy + 2) / 153); d = doy - int((153 * mp + 2) / 5) + 1
        m = mp < 10 ? mp + 3 : mp - 9; if (m <= 2) y++
        printf "%04d-%02d-%02dT%02d:%02d:%02d%sZ", y, m, d, int(r / 3600), int(r % 3600 / 60), r % 60, f }')
    shift
    touch -d "$_d" "$@"
}
mk() { cat > "${1:-Makefile}"; }    # write a makefile from stdin
# sig SIGNAL DELAY command...: run the command with default signal
# dispositions, send it the signal after DELAY seconds, report its status
sig() {
    "$TLBIN" -e '
var names = {INT: SIGINT, TERM: SIGTERM, HUP: SIGHUP, QUIT: SIGQUIT};
for (var k : names)
    signal(names[k], "dfl");
var pid = spawn(slice(argv, 2));
sleep(num(argv[1]));
kill(pid, names[argv[0]]);
var st = waitpid(pid);
if (termsig())
    printf("status signal SIG%s\n", find(values(names), termsig()) >= 0 ? keys(names)[find(values(names), termsig())] : str(termsig()));
else
    printf("status %d\n", st);' "$@"
}
