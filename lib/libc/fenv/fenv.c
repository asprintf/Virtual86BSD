/*
 * Copyright (c) 2026, Joost Orchidee Winter
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

/* <fenv.h>: the functions made of the primitives in arch/x86_64/fenv_prim.s. */
#include <fenv.h>

int fegetexceptflag(fexcept_t *f, int e)
{
	*f = (fexcept_t)fetestexcept(e);
	return 0;
}

/* sets the flags without raising: feraiseexcept() only sets them */
int fesetexceptflag(const fexcept_t *f, int e)
{
	feclearexcept(e & FE_ALL_EXCEPT);
	feraiseexcept(*f & e & FE_ALL_EXCEPT);
	return 0;
}

int feholdexcept(fenv_t *env)
{
	fenv_t t;
	fegetenv(env);
	t = *env;
	t.__control_word |= 0x3f;          /* mask all x87 exceptions */
	t.__status_word &= ~0x3f;
	t.__mxcsr = (t.__mxcsr | 0x1f80) & ~0x3fu;
	fesetenv(&t);
	return 0;
}

int feupdateenv(const fenv_t *env)
{
	int e = fetestexcept(FE_ALL_EXCEPT);
	fesetenv(env);
	feraiseexcept(e);
	return 0;
}
