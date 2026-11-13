/*[nonstandard]*/
/* Test whether character ranges work with UTF-8 encoding. */

#include "suite.h"

#include <locale.h>

void run(void)
{
	match("[æ-ø]", "æ");
	match("[æ-ø]", "ø");
	reject("[æ-ø]", "Æ");
	reject("[æ-ø]", "Ø");
	reject("[æ-ø]", "a");

	reject("[^æ-ø]", "æ");
	reject("[^æ-ø]", "ø");
	match("[^æ-ø]", "Æ");
	match("[^æ-ø]", "Ø");
	match("[^æ-ø]", "a");
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
