# rcctl — control the daemons of /etc/rc.d (V86BSD)

A daemon is a script in `/etc/rc.d` that names the program and ends in `rc_cmd $1`
(`/etc/rc.d/rc.subr` does the rest). Whether it starts at boot, and with what flags, is
`name_flags` in `/etc/rc.conf` (`NO`: off; empty: the default flags). `rcctl` never edits
`rc.conf`; it writes `/etc/rc.conf.local`, which is read after it.

    rcctl ls all|on|off|started|stopped|failed
    rcctl get service [flags|status]
    rcctl set service flags [flags]
    rcctl set service status on|off
    rcctl enable|disable service ...
    rcctl start|stop|restart|reload|check service ...

    rcctl enable atd              # atd_flags= in rc.conf.local
    rcctl set syslogd flags -m 5  # syslogd_flags='-m 5'
    rcctl restart syslogd         # prints syslogd(ok)

## rc.subr

`start`, `stop`, `restart`, `reload` and `check` print `name(ok)` or `name(failed)` (nothing with
`RC_QUIET` set; /etc/rc uses that) and return 0 or 1. A running daemon is found by its command line
(`ps`), which is `daemon flags` unless the script sets `pexp`. A script may set `daemon_flags` (the
default flags), `rc_bg=YES` (the daemon does not go to the background itself) and `rc_reload=NO`
(it cannot reread its configuration). `/etc/rc` starts `syslogd`, then `cron` and `atd` when their
flags are not `NO`, then the scripts of `pkg_scripts`; `/etc/rc.shutdown` stops those in reverse.

`set flags` refuses a value with a single quote or a control character: the line goes into a file
that root runs.

## Build and test

    make test      # sh test_rcctl.sh: services of its own with a daemon of its own, in a
                   # directory of the test (RC_CONF and RCTL_DIR); nothing of the system is touched
    make install   # into $(BINDIR) (/usr/sbin)
