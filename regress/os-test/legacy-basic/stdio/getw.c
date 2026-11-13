/*[SUSv2]*/
/* Test whether getw reads back the words putw wrote, then EOF. */

#include <stdio.h>

#include "../basic.h"

int main(void)
{
	FILE* fp = tmpfile();
	if ( !fp )
		err(1, "tmpfile");
	if ( putw(0x12345678, fp) != 0 || putw(-1, fp) != 0 )
		err(1, "putw");
	rewind(fp);
	if ( getw(fp) != 0x12345678 )
		errx(1, "getw did not read the first word");
	/* -1 is EOF too: feof tells them apart */
	if ( getw(fp) != -1 || feof(fp) )
		errx(1, "getw did not read the word -1");
	if ( getw(fp) != EOF || !feof(fp) )
		errx(1, "getw at the end of the file was not EOF with end-of-file set");
	fclose(fp);
	return 0;
}
