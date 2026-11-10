# helpers for the regression cases
T0=200001010000     # a fixed time for touch -t
mktree() {           # a small tree with every portable file type
    mkdir -p src/dir/sub src/empty
    echo one > src/one
    printf 'two\nlines\n' > src/dir/two
    awk 'BEGIN { while (i++ < 3000) printf "z" }' > src/dir/sub/big
    ln src/one src/dir/hard
    ln -s ../one src/dir/sym
    mkfifo src/fifo
    chmod 640 src/dir/two
    find src -exec "$LTOUCH" -t $T0 {} +
}
# exact file status, and file times without following symbolic links, which
# POSIX stat and touch lack (../../../lib/testlib/fstat.c and ltouch.c, from the runner)
fstat() { "$FSTAT" "$@"; }
treecmp() { "$TLBIN" "$TREECMP" "$@" && echo "trees equal"; }
# tar.tl: archives made and read independently of pax
TARTL=$(dirname "$LIB")/tar.tl
hdrs() { "$TLBIN" "$TARTL" hdrs "$1"; }   # the tar headers of an archive: typeflag, name, size, mode
