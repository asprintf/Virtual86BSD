/*[nonstandard]*/
/* Test if regular expressions recover after an invalid UTF-8 sequence. */

#include "suite.h"

#include <locale.h>

void run(void)
{
	partial(".", "\xC3\x20", m(1, 2)); // Incomplete sequence then space

	partial("[^x]", "\xC3\x20", m(1, 2)); // Incomplete sequence then space
}

int main(void)
{
	const char* utf8_locale = getenv("OS_TEST_UTF8_LOCALE");
	if ( !utf8_locale )
		utf8_locale = "C.UTF-8";
	if ( !setlocale(LC_CTYPE, utf8_locale) &&
	     !setlocale(LC_CTYPE, "C.UTF-8") &&
	     !setlocale(LC_CTYPE, "POSIX.UTF-8") &&
	     !setlocale(LC_CTYPE, "en_US.UTF-8") )
		errx(1, "setlocale LC_CTYPE failed: %s", utf8_locale);
	if ( !setlocale(LC_CTYPE, utf8_locale) &&
	     !setlocale(LC_COLLATE, "C.UTF-8") &&
	     !setlocale(LC_COLLATE, "POSIX.UTF-8") &&
	     !setlocale(LC_CTYPE, "en_US.UTF-8") )
		errx(1, "setlocale LC_COLLATE failed: %s", utf8_locale);

	cflags = 0;
	run();
	cflags = REG_EXTENDED;
	run();
	return 0;
}
