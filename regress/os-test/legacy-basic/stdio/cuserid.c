/*[SUSv2]*/
/* Test whether cuserid gives the name of the effective user. */

#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	struct passwd* pw = getpwuid(geteuid());
	char buf[L_cuserid];
	char* r = cuserid(buf);
	if ( r != buf )
		errx(1, "cuserid(s) did not return s");
	if ( pw && strcmp(buf, pw->pw_name) != 0 )
		errx(1, "cuserid gave %s, not %s", buf, pw->pw_name);
	r = cuserid(NULL);
	if ( pw && (!r || strcmp(r, pw->pw_name) != 0) )
		errx(1, "cuserid(NULL) did not give %s", pw->pw_name);
	return 0;
}
