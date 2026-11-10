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
#                         $(IMAGE), an ext2 file system of $(IMGSIZE) MiB
#   make release          all of that into $(RELROOT) (obj/root) and
#                         $(IMAGE) (obj/v86bsd.img)
#   make boot             QEMU on that kernel and image (BOOTFLAGS="-m 2G -n"
#                         and so on: see distrib/boot.sh)
#   make regress          every test, whatever fails on the way: regress-utils,
#                         the utilities' (regress/run_tests.sh), and
#                         regress-sys, the kernel's k_* tests and the C
#                         library's s_* tests (regress/sys/kern/run.sh;
#                         K_NET="41000 41001" adds k_net); meant for inside
#                         V86BSD
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
#   HOSTCC=cc        the host's compiler for stage 0 (default: c17, c99 or cc)
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
BOOTFLAGS =

# (BUILDER, DESTDIR, HOSTCC, KERNCONF and BUILDFLAGS are not set here: what
# is not given is empty, and the script has its own defaults.  They are given
# to the script, not left to the environment.)
BUILD = BUILDER='$(BUILDER)' CC='$(HOSTCC)' MAKE='$(MAKE)' \
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
	sh distrib/mkdisk.sh -s $(IMGSIZE) '$(IMGROOT)' '$(IMAGE)'

release:
	$(MAKE) buildworld buildkernel
	$(MAKE) newroot DESTDIR='$(RELROOT)'
	$(MAKE) installworld distribution installkernel DESTDIR='$(RELROOT)'
	$(MAKE) image

boot:
	sh distrib/boot.sh -k '$(IMGROOT)/bsd' $(BOOTFLAGS) '$(IMAGE)'

regress:
	@st=0; \
	$(MAKE) regress-utils || st=1; \
	$(MAKE) regress-sys || st=1; \
	exit $$st

regress-utils:
	sh regress/run_tests.sh

regress-sys:
	sh regress/sys/kern/run.sh

clean:
	@for d in bin sbin usr.bin usr.sbin lib/libc sbin/init; do \
	    (cd $$d && $(MAKE) clean) || exit 1; \
	done

cleandir: clean
	rm -rf '$(OBJDIR)'

.PHONY: all buildworld buildkernel installworld distribution installkernel \
	newroot image release boot regress regress-utils regress-sys clean cleandir
