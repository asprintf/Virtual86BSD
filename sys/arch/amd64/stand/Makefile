# Copyright (c) 2026, asprintf
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

# Makefile - the boot blocks: mbr (stage 1, the boot code of sector 0 of the
# disk) and boot (stage 2, from LBA 34 on), 32-bit code with a 16-bit stub, built
# by the host's compiler and GNU binutils (the loader is not V86BSD code: it
# runs before the system does).  mkimage, the other tool of the boot, is built
# by the kernel's Makefile.
#
#   make [HOSTCC=cc]    makes mbr/mbr (440 bytes) and boot/boot
HOSTCC = cc
AS = as
LD = ld
OBJCOPY = objcopy
CFLAGS = -m32 -Os -Wall -Wextra -ffreestanding -fno-builtin -fno-pic -fno-pie \
	-fno-stack-protector -fno-strict-aliasing -fno-asynchronous-unwind-tables \
	-fcf-protection=none -mno-sse -mno-mmx -mno-red-zone -march=i586 \
	-Iboot/shim -I../../..
OBJS = boot/start.o boot/main.o boot/cmd.o boot/load.o boot/ffs.o boot/disk.o \
	boot/mem.o boot/cons.o boot/lib.o

all: mbr/mbr boot/boot

boot/boot: $(OBJS) boot/boot.ld
	$(LD) -m elf_i386 --no-warn-rwx-segments -T boot/boot.ld -o boot/boot.elf $(OBJS)
	$(OBJCOPY) -O binary boot/boot.elf boot/boot

mbr/mbr: mbr/mbr.S boot/boot
	n=`wc -c < boot/boot`; n=`expr \( $$n + 511 \) / 512`; \
	$(AS) --32 --defsym NSECT=$$n -o mbr/mbr.o mbr/mbr.S && \
	$(LD) -m elf_i386 --no-warn-rwx-segments -Ttext 0x7c00 -o mbr/mbr.elf mbr/mbr.o && \
	$(OBJCOPY) -O binary -j .text mbr/mbr.elf mbr/mbr

boot/start.o: boot/start.S
	$(AS) --32 -o $@ boot/start.S

.SUFFIXES: .c .o
.c.o:
	$(HOSTCC) $(CFLAGS) -c -o $@ $<

$(OBJS): boot/boot.h

clean:
	rm -f boot/*.o boot/boot boot/boot.elf mbr/mbr mbr/mbr.o mbr/mbr.elf
