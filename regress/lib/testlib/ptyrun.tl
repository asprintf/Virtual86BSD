/*
 * ptyrun.tl - run a command with some of its descriptors on a terminal.
 *
 *     tl ptyrun.tl FDS COMMAND...
 *
 * FDS names the descriptors (of 0, 1 and 2) that are the slave side of a
 * new pseudo-terminal, which is the controlling terminal of the command
 * (it runs in a new session); the others are inherited.  The terminal's
 * size is left 0 by 0.  What reaches the terminal is written to the
 * standard output (CR LF as LF); the exit status is the command's, or
 * 128 plus the number of the signal that ended it.  If PTYRUN_INPUT is
 * set, it is typed on the terminal first, after PTYRUN_WAIT seconds if
 * that is set.
 */

if (len(argv) < 2) {
    eprintf("usage: tl ptyrun.tl FDS COMMAND...\n");
    exit(2);
}
var t = term(slice(argv, 1), {fds: argv[0], rows: 0, cols: 0});
var input = getenv("PTYRUN_INPUT");
if (input != nil && input != "") {
    var wait = getenv("PTYRUN_WAIT");
    if (wait != nil && wait != "")
        sleep(num(wait));
    tsend(t, input);
}
/* all of the output: until no one has the terminal open */
while (!teof(t))
    tpump(t, 1);
var status = nil;
while (status == nil)
    status = twait(t, 60);
puts(replace(traw(t), "\r\n", "\n"));
exit(status);
