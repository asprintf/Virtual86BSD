/* $NetBSD: termcap.h,v 1.2 2011/04/11 21:13:09 roy Exp $ */

/*
 * Copyright (c) 2009, 2011 The NetBSD Foundation, Inc.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Adapted for V86BSD from NetBSD lib/libterminfo/termcap.h (revision 1.2).
 * Changes: tgetflag(), tgetnum(), tgetstr() and tgoto() take char *
 * (X/Open Curses, Issue 4, Version 2).
 */

#ifndef _TERMCAP_H_
#define	_TERMCAP_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Output functions.
 * These are still valid for terminfo. */
int		putp(const char *);
int		tputs(const char *, int, int (*)(int));

extern short ospeed;
extern char PC;
extern char *BC;
extern char *UP;

int		tgetent(char *, const char *);
/* as X/Open Curses, Issue 4, Version 2 has them */
char *		tgetstr(char *, char **);
int		tgetflag(char *);
int		tgetnum(char *);
char *		tgoto(char *, int, int);

__END_DECLS
#endif
