# Settings for building with c17 (make CCNAME=c17): c17 compiles and
# also assembles the .s files, so no other tool is involved but ar.
CC       = c17
AS       = c17 -c
CFLAGS   = -O 1
SHARED   =
