# df — report free disk space (POSIX.1-2024)

An implementation of the `df` utility of POSIX.1-2024 (XCU df), in C with no
dependencies beyond libc. The default output with file slots and -t belong
to the XSI option.

    df [-k] [-P|-t] [file...]

## Build and test

    make           # builds ./df
    make test      # the tests (sh test_df.sh)
    make difftest  # compares with GNU df
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The figures of statvfs(): total f_blocks, used f_blocks - f_bfree,
  available f_bavail, each times f_frsize, rounded up to 512-byte units
  (1024 with -k); the capacity rounded up.
* The file systems are those of the mount table (/proc/self/mounts, else
  /etc/mtab: POSIX has no interface for it), their names its first field
  with its escapes undone.
* -P: the format of the standard; -t, the XSI totals (the default output has
  them too); file operands, and block special files that are the names of
  mounted file systems.

## Choices where the standard leaves room

* The default output has the columns of -P, the total among them, then the
  free and the total file slots (inodes): Filesystem 512-blocks Used
  Available Capacity Ifree Inodes Mounted on; the columns of both formats
  are aligned.
* Without operands, file systems with no blocks (proc, sysfs and the like)
  are left out, and so is one mounted over by a later mount on the same
  point.
* A file is in the file system whose mount point has its device and is the
  longest leading part of its real path; a block special file not mounted is
  an error.
* With operands of which none is found, not even the header is written.
* -P with -t: the -P format; capacity 0% when used + available is 0.
* Usage errors exit with status 2, other errors with 1.

## Testing

`test_df.sh` runs the utility on prepared input and compares the output and
the exit status: the -P and -Pk headers and fields, the arithmetic of the
capacity, the default format, file operands in several file systems, block
special files, errors. The binary tested is `./df`, or `$U` if set.

`tests/dfdiff.sh` compares df -P and df -Pk with GNU df (with
POSIXLY_CORRECT) for every file system and some files in them.

## Files

| file | contents |
|------|----------|
| `df.c` | the utility |
| `tests/dfdiff.sh` | the comparison with GNU df (`make difftest`) |
| `test_df.sh` | the tests: `make test`, or `sh test_df.sh` |
| `Makefile` | build, test, install, clean |
