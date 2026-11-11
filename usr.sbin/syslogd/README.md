# syslogd — the system log daemon (V86BSD)

The `syslogd` of the BSDs, reduced to what a single machine needs. Programs log with `syslog(3)`
(or `logger(1)`): RFC 3164 datagrams to `/dev/log`. The kernel's messages are read from
`/dev/klog`. Each message goes where `/etc/syslog.conf` says.

    syslogd [-d] [-f config_file] [-k klog_device] [-m mark_interval]
            [-P pid_file] [-p log_socket]

## Build and test

    make           # builds ./syslogd
    make test      # the tests (sh test_syslogd.sh; they use a socket, kernel log, pid file and
                   # configuration of their own, so they can run beside a real syslogd)
    make install   # into $(BINDIR) (/usr/sbin); DESTDIR= for staging
    make clean

## What it does

* `/etc/syslog.conf`: `selector<tab>action`. A selector is `facility[,facility...].level`
  (`*` for all facilities; `.level` the level and every one more severe, `.=level` only that one,
  `.!level` and `.!=level` take levels away from what the selectors before chose, `.none` takes the
  facility away; several selectors with `;`). An action is a file, a list of users (the terminals
  they are logged in on, from utmpx) or `*` (every user). Facilities as in `<syslog.h>` (plus
  `security` for `auth` and `mark`).
* A file is created (mode 0640) and appended to; SIGHUP rereads the configuration and opens the
  files again (so a rotated log is a rename and a SIGHUP). Files are synced a second after the
  messages stop (5 seconds at most while they go on).
* The time stamp and the host name of a line are the daemon's: a program cannot back-date a
  message. Control characters of a message are shown as `^X`, so a message cannot move the cursor,
  clear the screen or fake a second line; a message is cut at 1024 bytes.
* A line that repeats the one before it in a file is not written again; "last message repeated N
  times" comes when something else does, or after 30 seconds.
* Kernel lines are logged as `/bsd: text`, with `kern.crit` unless they start with `<N>`. The
  first syslogd to run gets everything the kernel printed since boot (the boot messages), once;
  `dmesg` shows the whole buffer, whoever has read it.
* `mark` in a selector gets `-- MARK --` every `-m` minutes (default 20; 0 never).
* Its own messages (`syslogd: start`, `restart`, `restarted`, `exiting on signal N`) are
  `syslog.info`. The process id is in `/var/run/syslog.pid`; a second daemon is refused.
  `-d` stays in the foreground and shows the lines on the standard error.
* Without a configuration file: `*.err` to `/dev/console`, `*.emerg` to every user.

Not there: forwarding (`@host`) and receiving from the network, pipes (`|command`), program and
host blocks (`!prog`, `+host`) — a line that asks for one is reported and ignored — and log
rotation (newsyslog).
