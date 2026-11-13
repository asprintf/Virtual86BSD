/*[SUSv2]*/
/* Test whether putw writes the bytes of an int. */

#include <stdio.h>
#include <string.h>

#include "../basic.h"

int main(void)
{
	FILE* fp = tmpfile();
	if ( !fp )
		err(1, "tmpfile");
	int w = 0x01020304, r;
	if ( putw(w, fp) != 0 )
		err(1, "putw");
	rewind(fp);
	if ( fread(&r, sizeof r, 1, fp) != 1 || memcmp(&r, &w, sizeof w) != 0 )
		errx(1, "putw did not write the int's bytes");
	fclose(fp);
	return 0;
}
