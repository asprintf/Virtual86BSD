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

# Makefile - build, install, release and test Virtual/86 BSD (POSIX make: the
# system's own make runs it too).
#
#   make buildworld       the C library and every program, in stages 0-3, and
#                         init, into $(OBJDIR) (obj/); stage 1 and stage 2
#                         must come out identical
#   make buildkernel      the kernel, $(KERNCONF) (default GENERIC), into obj/
#   make installworld     /bin, /sbin, /usr ... into $(DESTDIR)
#   make distribution     /etc, the sources in /usr/src and the host's data
#                         into $(DESTDIR): a new root
#   make installkernel    the kernel as $(DESTDIR)/bsd
#   make newroot          $(DESTDIR) made afresh (removed first!)
#   make image            a disk image of $(IMGROOT) (default $(RELROOT)):
#                         $(IMAGE), an FFS2 file system of $(IMGSIZE) MiB
#                         (IMGTYPE=ext2 for an ext2 one)
#   make release          all of that into $(RELROOT) (obj/root) and
#                         $(IMAGE) (obj/v86bsd.img)
#   make boot             QEMU on that kernel and image (BOOTFLAGS="-m 2G -n"
#                         and so on: see distrib/boot.sh)
#   make regress          every test, whatever fails on the way: regress-utils,
#                         the utilities' (regress/run_tests.sh), and
#                         regress-sys, the kernel's k_* tests and the C
#                         library's s_* tests (regress/sys/kern/run.sh;
#                         K_NET="41000 41001" adds k_net), and regress-etc,
#                         /etc/rc and /etc/netstart run against prepared
#                         files (regress/etc);
#                         meant for inside V86BSD
#   make clean            what was built in the tree by make in bin, sbin ...
#   make cleandir         clean, and $(OBJDIR) removed
#
# DESTDIR names a directory: installworld, distribution and installkernel
# into the running system (no DESTDIR) are only done by V86BSD itself, and
# distribution never.  More:
#
#   OBJDIR=dir       where it is built (default obj)
#   KERNCONF=name    the kernel configuration (default GENERIC)
#   BUILDER=u@h      the name in the kernel's banner (default: that of this
#                    machine); to compare two builds, give both the same
#   ROOTPW=word      root's password in the new root (default: root, as in
#                    etc/master.passwd; needs openssl on the host; it must
#                    not contain a single quote)
#   HOSTCC=cc        the host's compiler (default: gcc, c17, c99 or cc); gcc builds
#                    the whole world, c17 the three stages of README.md
#   BUILDFLAGS=-n    all programs statically linked
#
# The work is done by distrib/build.sh; bootstrap.sh, mkdisk.sh, boot.sh and
# chroot.sh at the top of the tree call these targets and the scripts of
# distrib/.

.POSIX:

OBJDIR   = obj
RELROOT  = $(OBJDIR)/root
IMGROOT  = $(RELROOT)
IMAGE    = $(OBJDIR)/v86bsd.img
IMGSIZE  = 1024
IMGTYPE  = ffs
BOOTFLAGS =

# (BUILDER, DESTDIR, HOSTCC, KERNCONF and BUILDFLAGS are not set here: what
# is not given is empty, and the script has its own defaults.  They are given
# to the script, not left to the environment.)
BUILD = BUILDER='$(BUILDER)' ROOTPW='$(ROOTPW)' CC='$(HOSTCC)' MAKE='$(MAKE)' \
	sh distrib/build.sh -o '$(OBJDIR)' -d '$(DESTDIR)' -c '$(KERNCONF)' $(BUILDFLAGS)

all: buildworld buildkernel

buildworld:
	$(BUILD) world

buildkernel:
	$(BUILD) kernel

installworld:
	$(BUILD) installworld

distribution:
	$(BUILD) distribution

installkernel:
	$(BUILD) installkernel

newroot:
	$(BUILD) newroot

image:
	sh distrib/mkdisk.sh -s $(IMGSIZE) -t $(IMGTYPE) '$(IMGROOT)' '$(IMAGE)'

release:
	$(MAKE) buildworld buildkernel
	$(MAKE) newroot DESTDIR='$(RELROOT)'
	$(MAKE) installworld distribution installkernel DESTDIR='$(RELROOT)'
	$(MAKE) image

boot:
	sh distrib/boot.sh -k '$(IMGROOT)/bsd' $(BOOTFLAGS) '$(IMAGE)'

# regress-sys is last: its k_klog fills the kernel's log, and the boot's
# messages (dmesg, rc's dmesg.boot) are gone after it
regress:
	@st=0; \
	$(MAKE) regress-utils || st=1; \
	$(MAKE) regress-etc || st=1; \
	$(MAKE) regress-sys || st=1; \
	exit $$st

regress-utils:
	sh regress/run_tests.sh

regress-sys:
	sh regress/sys/kern/run.sh

regress-etc:
	@st=0; \
	for t in rc netstart; do \
	    sh regress/etc/test_$$t.sh || st=1; \
	done; \
	exit $$st

clean:
	@for d in bin sbin libexec usr.bin usr.sbin lib/libc sbin/init; do \
	    (cd $$d && $(MAKE) clean) || exit 1; \
	done

cleandir: clean
	rm -rf '$(OBJDIR)'

.PHONY: all buildworld buildkernel installworld distribution installkernel \
	newroot image release boot regress regress-utils regress-sys regress-etc clean cleandir
