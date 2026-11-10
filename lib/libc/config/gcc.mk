# Copyright (c) 2026, Joost Orchidee Winter
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Settings for building with gcc (make CCNAME=gcc, the default).
CC       = gcc
AS       = as
CFLAGS   = -std=c17 -O2 -pipe -fPIC -ffreestanding -fno-builtin \
	   -fno-stack-protector -fno-tree-loop-distribute-patterns \
	   -fno-strict-aliasing -fcf-protection=none \
	   -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
	   -nostdinc -isystem `$(CC) -print-file-name=include`

# The shared library, which is also the dynamic linker (ldso/dyn):
# calls inside it bind to it (-Bsymbolic-functions), data goes through
# the GOT (a program's copy relocations); libgcc's helpers stay internal.
SHARED   = $(B)/lib/libc.so $(B)/lib/Scrt1.o $(B)/lib/rcrt1.o
SOFLAGS  = -shared -nostdlib -Wl,-soname,libc.so -Wl,-e,_dlstart \
	   -Wl,-Bsymbolic-functions -Wl,--hash-style=both -Wl,-z,now \
	   -Wl,-z,relro -Wl,-z,noexecstack -Wl,--exclude-libs,ALL

$(B)/lib/libc.so: $(OBJS) $(DYN_OBJS) $(B)/lib/crti.o $(B)/lib/crtn.o
	$(CC) $(SOFLAGS) -o $@ $(B)/lib/crti.o $(OBJS) $(DYN_OBJS) \
		`$(CC) -print-libgcc-file-name` $(B)/lib/crtn.o
