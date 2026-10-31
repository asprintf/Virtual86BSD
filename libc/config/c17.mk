# Settings for building with c17 (make CCNAME=c17): c17 compiles and
# also assembles the .s files, so no other tool is involved but ar.
# -G: position-independent code, for libc.a and libc.so alike (as gcc's
# -fPIC).
CC       = c17
AS       = c17 -c
CFLAGS   = -O 1 -G

# The shared library, which is also the dynamic linker (src/ldso/dyn):
# c17 -G binds calls inside it to it, data and function addresses go
# through the GOT (a program's copy relocations and canonical PLT
# entries); c17's run-time support becomes part of it, not exported.  The
# entry point is named by a linker script (tools/libc.so.lds).
SHARED   = $(B)/lib/libc.so $(B)/lib/Scrt1.o $(B)/lib/rcrt1.o

$(B)/lib/libc.so: $(OBJS) $(DYN_OBJS) $(B)/lib/crti.o $(B)/lib/crtn.o tools/libc.so.lds
	$(CC) -G -o $@ tools/libc.so.lds $(B)/lib/crti.o $(OBJS) $(DYN_OBJS) $(B)/lib/crtn.o
