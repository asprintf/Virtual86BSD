# hostname — show or set the name of the host (V86BSD)

    hostname [-s] [name]

## Build and test

    make           # builds ./hostname
    make test      # the tests (sh test_hostname.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* No name: print the host name (`-s`: up to the first dot). With a name: set it (root; at most 64 bytes).
* Works on both ABIs (it uses `gethostname` and `sethostname`).

## Testing

`test_hostname.sh`; the setting tests run only as root on V86BSD and put the name back.
