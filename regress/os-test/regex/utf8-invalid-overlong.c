/*[nonstandard]*/
/* Test if regular expressions work with a too long UTF-8 sqeuence. */

#include "suite.h"

#include <locale.h>

void run(void)
{
	reject(".", "\xC0\xAF"); // Overlong encoding

	reject("[^x]", "\xC0\xAF"); // Overlong encoding

	reject("..", "\xC0\xAF"); // Overlong encoding

	reject("[!x][!x]", "\xC0\xAF"); // Overlong encoding
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
