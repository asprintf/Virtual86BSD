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

# Makefile - build, boot and test Virtual/86 BSD on the FreeBSD kernel
# (POSIX make).  Everything is built by clang and lld on an x86-64 Linux host;
# README.md says what that needs.
#
#   make kernel      FreeBSD's kernel ($(KERNCONF), sys/amd64/conf/V86), the
#                    boot loader (stand/) and the image tools, into
#                    obj/freebsd (distrib/build-freebsd.sh)
#   make image       the C library, every program, FreeBSD's UFS tools and
#                    the bootable disk image $(IMAGE) (distrib/boot-freebsd.sh -n)
#   make release     kernel and image
#   make boot        boots $(IMAGE) in QEMU (distrib/qemu-freebsd.sh):
#                    BOOTFLAGS="-g -m 2048 -s 4" and so on
#   make regress     regress-libc, regress-etc and regress-utils, whatever
#                    fails on the way
#   make regress-libc    the C library's tests in the VM (regress/lib/libc/vmrun.sh)
#   make regress-utils   the utilities' tests in the VM (distrib/utilsuite-freebsd.sh;
#                        UTILS="sh grep" picks some)
#   make regress-etc     /etc/rc and /etc/netstart against prepared files (regress/etc)
#   make gate-ost    os-test in the VM, into obj/gate-ost (lib/libc/tools/gate.sh; long)
#   make gate-pts    the Open POSIX Test Suite, into obj/gate-pts (long)
#   make clean       what was built in the tree by make in bin, sbin ...
#   make cleandir    clean, and obj/ removed
#
#   JOBS=n           parallel jobs of the kernel build (default 12)
#   KERNCONF=name    the kernel configuration (default V86)
#   IMAGE=file       the disk image (default obj/boot/v86bsd.img)

.POSIX:

JOBS     = 12
KERNCONF = V86
IMAGE    = obj/boot/v86bsd.img
BOOTFLAGS =
UTILS    =
FBSDW    = WITHOUT_CDDL=yes WITHOUT_ZFS=yes WITHOUT_TESTS=yes
BF       = sh distrib/build-freebsd.sh --

all: kernel image

kernel:
	$(BF) -j$(JOBS) $(FBSDW) WITH_DISK_IMAGE_TOOLS_BOOTSTRAP=yes KERNCONF=$(KERNCONF) kernel-toolchain
	$(BF) -j$(JOBS) $(FBSDW) KERNCONF=$(KERNCONF) buildkernel
	$(BF) loader $(FBSDW)

image:
	sh distrib/boot-freebsd.sh -n

release: kernel image

boot:
	sh distrib/qemu-freebsd.sh $(BOOTFLAGS) '$(IMAGE)'

regress:
	@st=0; \
	$(MAKE) regress-libc || st=1; \
	$(MAKE) regress-etc || st=1; \
	$(MAKE) regress-utils || st=1; \
	exit $$st

regress-libc:
	sh regress/lib/libc/vmrun.sh

regress-utils:
	sh distrib/utilsuite-freebsd.sh $(UTILS)

regress-etc:
	@st=0; \
	for t in rc netstart; do \
	    sh regress/etc/test_$$t.sh || st=1; \
	done; \
	exit $$st

gate-ost:
	sh lib/libc/tools/gate.sh ost obj/gate-ost

gate-pts:
	sh lib/libc/tools/gate.sh pts obj/gate-pts

clean:
	@for d in bin sbin libexec usr.bin usr.sbin lib/libc sbin/init; do \
	    (cd $$d && $(MAKE) clean) || exit 1; \
	done

cleandir: clean
	rm -rf obj

.PHONY: all kernel image release boot regress regress-libc regress-utils \
	regress-etc gate-ost gate-pts clean cleandir
