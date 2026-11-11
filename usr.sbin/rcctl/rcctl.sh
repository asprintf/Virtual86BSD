#!/bin/sh
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

# rcctl - control the daemons of /etc/rc.d.
#
#   rcctl ls all|on|off|started|stopped|failed
#   rcctl get service [flags|status]
#   rcctl set service flags [flags]
#   rcctl set service status on|off
#   rcctl enable|disable service ...
#   rcctl start|stop|restart|reload|check service ...
#
# A service is a script in /etc/rc.d (rc.subr is not one).  Whether it
# starts at boot, and with what flags, is service_flags in /etc/rc.conf,
# which this program does not change: it writes /etc/rc.conf.local, the
# file that is read after it.  `enable' and `disable' only add the line that
# is needed (none, when rc.conf already says so); `set service flags' always
# writes one, and so also turns the service on (NO turns it off).  The
# commands that run a service call its script, and print what it prints:
# `service(ok)' or `service(failed)'; the status is 0 when that is ok for
# every service.  `ls started' and `stopped' ask the scripts (check),
# `failed' lists the services that are on but not running.
#
# RC_CONF names another rc.conf and RCTL_DIR another rc.d (for tests).

RC_CONF=${RC_CONF:-/etc/rc.conf}
LOCAL=$RC_CONF.local
RCD=${RCTL_DIR:-/etc/rc.d}
export RC_CONF

usage() {
	echo "usage: rcctl ls all|on|off|started|stopped|failed" >&2
	echo "       rcctl get service [flags|status]" >&2
	echo "       rcctl set service flags [flags]" >&2
	echo "       rcctl set service status on|off" >&2
	echo "       rcctl enable|disable service ..." >&2
	echo "       rcctl start|stop|restart|reload|check service ..." >&2
	exit 1
}

# a service's name: letters, digits and _ . -, an executable script of RCD
valid() {
	case $1 in
	rc.subr | '' | -* | *[!A-Za-z0-9_.-]*)
		echo "rcctl: service $1 does not exist" >&2
		exit 2
		;;
	esac
	if [ ! -f "$RCD/$1" ]; then
		echo "rcctl: service $1 does not exist" >&2
		exit 2
	fi
}

# the services: the names of the files of RCD but rc.subr
services() {
	for f in "$RCD"/*; do
		[ -f "$f" ] || continue
		n=${f##*/}
		[ "$n" = rc.subr ] && continue
		echo "$n"
	done
}

# the value of service_flags once rc.conf and rc.conf.local have been read; NO when there is none
flags_of() {
	(
		[ -f "$RC_CONF" ] && . "$RC_CONF"
		[ -f "$LOCAL" ] && . "$LOCAL"
		eval "echo \"\${${1}_flags-NO}\""
	) 2> /dev/null
}

is_on() { [ "$(flags_of "$1")" != NO ]; }

# a flags value that can be put in a file that root runs: no quote, no
# control character
safe() {
	case $1 in
	*"'"*)
		echo "rcctl: flags must not contain a single quote" >&2
		exit 1
		;;
	esac
	if [ "$(printf '%s' "$1" | tr -d '[:cntrl:]')" != "$1" ]; then
		echo "rcctl: flags must not contain control characters" >&2
		exit 1
	fi
}

# take every line of service_flags= out of rc.conf.local, then add VALUE
# (the whole line: flags, or nothing) when it is given
rewrite() {
	svc=$1
	line=$2
	tmp=$LOCAL.$$
	if [ -f "$LOCAL" ]; then
		awk -v p="${svc}_flags=" 'index($0, p) != 1' "$LOCAL" > "$tmp" ||
			{ rm -f "$tmp"; echo "rcctl: cannot write $LOCAL" >&2; exit 1; }
	else
		: > "$tmp" || { echo "rcctl: cannot write $LOCAL" >&2; exit 1; }
	fi
	[ -n "$line" ] && echo "$line" >> "$tmp"
	if [ ! -s "$tmp" ] && [ ! -f "$LOCAL" ]; then
		rm -f "$tmp"
		return 0
	fi
	chmod 644 "$tmp" && mv "$tmp" "$LOCAL" || { rm -f "$tmp"; echo "rcctl: cannot write $LOCAL" >&2; exit 1; }
}

do_enable() {
	valid "$1"
	rewrite "$1" ""
	is_on "$1" || rewrite "$1" "$1_flags="
}

do_disable() {
	valid "$1"
	rewrite "$1" ""
	is_on "$1" && rewrite "$1" "$1_flags=NO"
	return 0
}

do_set_flags() {
	valid "$1"
	safe "$2"
	case $2 in
	'' | NO) rewrite "$1" "$1_flags=$2" ;;
	*) rewrite "$1" "$1_flags='$2'" ;;
	esac
}

running() { sh "$RCD/$1" check > /dev/null 2>&1; }

[ $# -ge 1 ] || usage
cmd=$1
shift
case $cmd in
ls)
	[ $# -eq 1 ] || usage
	case $1 in
	all | on | off | started | stopped | failed) ;;
	*) usage ;;
	esac
	for s in $(services); do
		case $1 in
		all) echo "$s" ;;
		on) is_on "$s" && echo "$s" ;;
		off) is_on "$s" || echo "$s" ;;
		started) running "$s" && echo "$s" ;;
		stopped) running "$s" || echo "$s" ;;
		failed) is_on "$s" && ! running "$s" && echo "$s" ;;
		esac
	done
	exit 0
	;;
get)
	[ $# -ge 1 ] && [ $# -le 2 ] || usage
	valid "$1"
	case $2 in
	'') echo "$1_flags=$(flags_of "$1")" ;;
	flags) flags_of "$1" ;;
	status) if is_on "$1"; then echo on; else echo off; fi ;;
	*) usage ;;
	esac
	;;
set)
	[ $# -ge 2 ] || usage
	svc=$1
	what=$2
	shift 2
	case $what in
	flags)
		do_set_flags "$svc" "$*"
		;;
	status)
		[ $# -eq 1 ] || usage
		case $1 in
		on) do_enable "$svc" ;;
		off) do_disable "$svc" ;;
		*) usage ;;
		esac
		;;
	*) usage ;;
	esac
	;;
enable | disable)
	[ $# -ge 1 ] || usage
	for s in "$@"; do
		valid "$s"
	done
	for s in "$@"; do
		"do_$cmd" "$s"
	done
	;;
start | stop | restart | reload | check)
	[ $# -ge 1 ] || usage
	for s in "$@"; do
		valid "$s"
	done
	rc=0
	for s in "$@"; do
		sh "$RCD/$s" "$cmd" || rc=1
	done
	exit $rc
	;;
*) usage ;;
esac
