# kill — send a signal to processes, or list the signals (POSIX.1-2024)

The `kill` utility of POSIX.1-2024 (the shell has its own builtin).

    kill -s signal_name pid ...
    kill -l [exit_status]
    kill [-signal_name] pid ...
    kill [-signal_number] pid ...

## Build and test

    make           # builds ./kill
    make test      # the tests (sh test_kill.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* Signals by name (without `SIG`) or number; `0` only checks that the process is there. A negative pid is a
  process group. The default is `TERM`.
* `-l` lists the names; with an exit status, the name of the signal that ended a process (`kill -l 143` is `TERM`).
* Job specifications (`%1`) are the shell's. Status 1 if a signal could not be sent, 2 for a usage error or an
  unknown signal.

## Testing

`test_kill.sh` sends every form of the signal to `sleep` processes and a shell that traps one, and checks the lists, the
errors and the status.
