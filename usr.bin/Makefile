# Copyright (c) 2026, Joost Orchidee Winter
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

# usr.bin - the other user commands (POSIX.1-2024 XCU), each in its own directory
#
#   make              build all of them
#   make test         build, then run every test suite (../regress/run_tests.sh)
#   make difftest     compare with other implementations (see each README.md)
#   make install      install into $(DESTDIR)$(BINDIR)
#   make clean        remove everything that was built
#
# Macros given on the command line (CC=, CFLAGS=, PREFIX=, BINDIR=, DESTDIR=)
# reach every directory.  One utility: cd sed && make.

.POSIX:

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

SUBDIR = ar asa at awk basename bc c17 cal cflow cksum cmp comm \
	compress crontab csplit ctags cut cxref diff dirname du env \
	expand false file find fold fuser gencat getconf gettext grep \
	head iconv id ipcrm ipcs join lex link locale localedef logger \
	login logname lp m4 mailx make man mesg mkfifo more msgfmt newgrp nice \
	nl nm nohup od passwd paste patch pathchk pr printf readlink realpath \
	renice sccs sed sort split strings strip tabs tail talk tee tic time \
	timeout tl touch tput tr true tsort tty uname unexpand uniq \
	unlink uucp uudecode uuencode uustat uux vi wc who write xargs \
	xgettext yacc

all:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE)) || exit 1; \
	done

test: all
	sh ../regress/run_tests.sh $(SUBDIR)

difftest: all
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) difftest) || exit 1; \
	done

install: all
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) BINDIR=$(BINDIR) install) || exit 1; \
	done

clean:
	@for u in $(SUBDIR); do \
	    (cd $$u && $(MAKE) clean) || exit 1; \
	done

.PHONY: all test difftest install clean
