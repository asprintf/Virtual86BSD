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

# Settings for building with c17 (make CCNAME=c17): c17 compiles and
# also assembles the .s files, so no other tool is involved but ar.
# -G: position-independent code, for libc.a and libc.so alike (as gcc's
# -fPIC).
CC       = c17
AS       = c17 -c
CFLAGS   = -O 1 -G

# The shared library, which is also the dynamic linker (ldso/dyn):
# c17 -G binds calls inside it to it, data and function addresses go
# through the GOT (a program's copy relocations and canonical PLT
# entries); c17's run-time support becomes part of it, not exported.  The
# entry point is named by a linker script (tools/libc.so.lds).
SHARED   = $(B)/lib/libc.so $(B)/lib/Scrt1.o $(B)/lib/rcrt1.o

$(B)/lib/libc.so: $(OBJS) $(DYN_OBJS) $(B)/lib/crti.o $(B)/lib/crtn.o tools/libc.so.lds
	$(CC) -G -o $@ tools/libc.so.lds $(B)/lib/crti.o $(OBJS) $(DYN_OBJS) $(B)/lib/crtn.o
