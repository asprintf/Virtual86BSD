# getty — set up a terminal line and ask for a login name (V86BSD)

The `getty` of the BSDs, much reduced. init(8) runs one on every line of
`/etc/ttys` that is on; it asks for a login name and runs `login`.

    getty [-f gettytab] [type [line]]

## Build and test

    make           # builds ./getty
    make test      # the tests (sh test_getty.sh)
    make install   # into $(BINDIR) (/usr/libexec); DESTDIR= for staging
    make clean

## What it does

* With a *line* (`tty00`) it takes `/dev/line` from whoever has it: `revoke(2)` (their open files go
  dead, their session is hung up), a new session with the terminal as its controlling terminal, the
  owner set to root and the mode to 0600 (login gives the user theirs). With no line, or `-`, the
  terminal is the standard input.
* The terminal is set (speed, 8 bits, canonical input with echo and erase/kill editing), the banner
  and the prompt are written, a line is read, and `login -- name` is run with getty's environment
  (init gives it `PATH` and `TERM`).
* An empty line asks again; so does a name that starts with `-`, has a control character or is longer
  than 32 bytes. End of file ends getty with status 0, a timeout with status 1; init starts a new
  one (and sleeps if they end too fast).
* `/etc/gettytab` (termcap format): a *type* names an entry, `tc=` chains entries, `default` fills
  in what is missing. Used: `sp` (speed), `im` (banner: `%h` host, `%t` line, `%s` system, `%m`
  machine, `%r` release, `%d` date, `%%`), `lm` (prompt), `lo` (login program), `to` (seconds to
  answer). With no table the built-in banner is `%s/%m (%h) (%t)` and the prompt `login: `.
* SIGINT and SIGQUIT are ignored while it waits; login gets them back.

Not there: autobaud, port selectors, parity, upper-case-only terminals, `nx` chains; getty does not
log (login does: failed logins and root logins, see syslogd).

## Testing

`test_getty.sh` runs getty on a pseudo-terminal that `ptydrive` (`regress/lib/testlib/ptydrive.c`)
types on, with a table of its own and a login that says what it was given: banner escapes, `tc=`
and aliases, `lm@`, editing, every refusal, `^C`, `^D`, the timeout, the usage errors and the
errors of a bad line or login program. The line mode (revoke, chown) needs a real device node and
is checked on a boot, with init on `tty00`.
