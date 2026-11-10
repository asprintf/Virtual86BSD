#include <ctype.h>
#include "out.h"
int main(void)
{
	int c;
	for (c = -1; c < 256; c++) {
		oi(c); oc(':');
		oc('0' + !!isalnum(c)); oc('0' + !!isalpha(c)); oc('0' + !!isblank(c));
		oc('0' + !!iscntrl(c)); oc('0' + !!isdigit(c)); oc('0' + !!isgraph(c));
		oc('0' + !!islower(c)); oc('0' + !!isprint(c)); oc('0' + !!ispunct(c));
		oc('0' + !!isspace(c)); oc('0' + !!isupper(c)); oc('0' + !!isxdigit(c));
		oc(' '); oi(tolower(c)); oc(' '); oi(toupper(c));
		/* function forms too */
		oc((isalpha)(c) ? 'A' : 'a'); oc((isdigit)(c) ? 'D' : 'd'); oc((isprint)(c) ? 'P' : 'p');
		onl();
	}
	oflush();
	return 0;
}
