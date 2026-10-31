# Settings for building with gcc (make CCNAME=gcc, the default).
CC       = gcc
AS       = as
CFLAGS   = -std=c17 -O2 -pipe -fPIC -ffreestanding -fno-builtin \
	   -fno-stack-protector -fno-tree-loop-distribute-patterns \
	   -fno-strict-aliasing -fcf-protection=none \
	   -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
	   -nostdinc -isystem `$(CC) -print-file-name=include`

# The shared library, which is also the dynamic linker (src/ldso/dyn):
# calls inside it bind to it (-Bsymbolic-functions), data goes through
# the GOT (a program's copy relocations); libgcc's helpers stay internal.
SHARED   = $(B)/lib/libc.so $(B)/lib/Scrt1.o $(B)/lib/rcrt1.o
SOFLAGS  = -shared -nostdlib -Wl,-soname,libc.so -Wl,-e,_dlstart \
	   -Wl,-Bsymbolic-functions -Wl,--hash-style=both -Wl,-z,now \
	   -Wl,-z,relro -Wl,-z,noexecstack -Wl,--exclude-libs,ALL

$(B)/lib/libc.so: $(OBJS) $(DYN_OBJS) $(B)/lib/crti.o $(B)/lib/crtn.o
	$(CC) $(SOFLAGS) -o $@ $(B)/lib/crti.o $(OBJS) $(DYN_OBJS) \
		`$(CC) -print-libgcc-file-name` $(B)/lib/crtn.o
