# sync — write the file system buffers to the disks (V86BSD)

    sync

## Build and test

    make           # builds ./sync
    make test      # the tests (sh test_sync.sh)
    make install   # into $(BINDIR); DESTDIR= for staging
    make clean

## What is implemented

* Calls `sync(2)`. No options.

## Testing

`test_sync.sh`: the status and the usage error.
