/*
 * Copyright (c) 2026, asprintf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* vidattr, vid_attr, vidputs, vid_puts: X/Open Curses Issue 4 Version 2 */

#include "curses.h"
#include "curses_private.h"

static int (*vidputc)(int);

static void
put(const char *s)
{
	if (s != NULL)
		tputs(s, 1, vidputc);
}

/* the rendition attr, colour pair pair, from the terminal's descriptions;
 * the model curses keeps of the terminal is neither read nor changed */
static int
setrendition(attr_t attr, short pair, int (*putc)(int))
{
	short fg, bg;

	if (cur_term == NULL)
		return ERR;
	vidputc = putc;
	if (set_attributes != NULL) {
		put(tiparm(set_attributes,
		    (attr & WA_STANDOUT) != 0, (attr & WA_UNDERLINE) != 0,
		    (attr & WA_REVERSE) != 0, (attr & WA_BLINK) != 0,
		    (attr & WA_DIM) != 0, (attr & WA_BOLD) != 0,
		    (attr & WA_INVIS) != 0, (attr & WA_PROTECT) != 0,
		    (attr & WA_ALTCHARSET) != 0));
	} else {
		put(exit_attribute_mode);
		if (exit_attribute_mode == NULL) {
			put(exit_standout_mode);
			put(exit_underline_mode);
			put(exit_alt_charset_mode);
		}
		if (attr & WA_STANDOUT)
			put(enter_standout_mode);
		if (attr & WA_UNDERLINE)
			put(enter_underline_mode);
		if (attr & WA_REVERSE)
			put(enter_reverse_mode);
		if (attr & WA_BLINK)
			put(enter_blink_mode);
		if (attr & WA_DIM)
			put(enter_dim_mode);
		if (attr & WA_BOLD)
			put(enter_bold_mode);
		if (attr & WA_INVIS)
			put(enter_secure_mode);
		if (attr & WA_PROTECT)
			put(enter_protected_mode);
		if (attr & WA_ALTCHARSET)
			put(enter_alt_charset_mode);
	}
	/* the highlights sgr has no parameters for */
	if (attr & WA_HORIZONTAL)
		put(enter_horizontal_hl_mode);
	if (attr & WA_LEFT)
		put(enter_left_hl_mode);
	if (attr & WA_LOW)
		put(enter_low_hl_mode);
	if (attr & WA_RIGHT)
		put(enter_right_hl_mode);
	if (attr & WA_TOP)
		put(enter_top_hl_mode);
	if (attr & WA_VERTICAL)
		put(enter_vertical_hl_mode);
	/* colours (after start_color()): the pair's, as init_pair() made it */
	if (_cursesi_screen == NULL || !__using_color)
		return OK;
	if (pair == 0)
		put(orig_pair);
	else if (pair_content(pair, &fg, &bg) == OK) {
		if (set_a_foreground != NULL) {
			put(tiparm(set_a_foreground, fg));
			put(tiparm(set_a_background, bg));
		} else if (set_foreground != NULL) {
			put(tiparm(set_foreground, fg));
			put(tiparm(set_background, bg));
		}
	}
	return OK;
}

int
vidputs(chtype attr, int (*putc)(int))
{

	return setrendition(attr & (__ATTRIBUTES & ~__COLOR),
	    (short)PAIR_NUMBER(attr), putc);
}

int
vidattr(chtype attr)
{

	return vidputs(attr, putchar);
}

int
vid_puts(attr_t attr, short pair, void *opts, int (*putc)(int))
{

	if (opts != NULL)
		return ERR;
	return setrendition(attr & WA_ATTRIBUTES & ~__COLOR, pair, putc);
}

int
vid_attr(attr_t attr, short pair, void *opts)
{

	return vid_puts(attr, pair, opts, putchar);
}
