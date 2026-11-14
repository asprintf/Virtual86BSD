/*[SUSv2]*/
/* Test whether a function defined with va_alist and va_dcl gets its arguments in order, the named ones before va_alist apart (c17 only). */

#include <stdio.h>
#include <string.h>
#include <varargs.h>

#include "../basic.h"

int sum();
int mkstr();

/* the arguments are all variable */
int sum(va_alist) va_dcl
{
	va_list ap;
	int n = 0, v;
	va_start(ap);
	while ( (v = va_arg(ap, int)) != 0 )
		n += v;
	va_end(ap);
	return n;
}

/* named arguments, then the variable ones of mixed types */
int mkstr(buf, fmt, va_alist) char* buf; char* fmt; va_dcl
{
	va_list ap;
	int n = 0;
	va_start(ap);
	for ( ; *fmt; fmt++ )
	{
		if ( *fmt == 'd' )
			n += sprintf(buf + n, "%d,", va_arg(ap, int));
		else if ( *fmt == 'f' )
			n += sprintf(buf + n, "%.2f,", va_arg(ap, double));
		else if ( *fmt == 's' )
			n += sprintf(buf + n, "%s,", va_arg(ap, char*));
	}
	va_end(ap);
	return n;
}

int main(void)
{
	char buf[128];
	if ( sum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0) != 55 )
		errx(1, "sum of ten arguments");
	if ( sum(0) != 0 )
		errx(1, "sum of none");
	mkstr(buf, "dfsdf", 1, 2.5, "x", 4, 5.25);
	if ( strcmp(buf, "1,2.50,x,4,5.25,") != 0 )
		errx(1, "mkstr gave %s", buf);
	return 0;
}
