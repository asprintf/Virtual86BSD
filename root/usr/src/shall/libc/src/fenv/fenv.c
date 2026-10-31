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
