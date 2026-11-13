/*[nonstandard]*/
/* Test whether equivalence classes work with UTF-8 encoding (insensitive). */

#include "suite.h"

#include <locale.h>

void run(void)
{
	match("[[:alpha:]]", "a");
	match("[[:alpha:]]", "ø");
	match("[[:alpha:]]", "λ");
	match("[[:alpha:]]", "Λ");
	reject("[[:alpha:]]", "1");

	match("[[:lower:]]", "a");
	match("[[:lower:]]", "ø");
	match("[[:lower:]]", "λ");
	match("[[:lower:]]", "Λ");
	reject("[[:lower:]]", "1");

	match("[[:upper:]]", "a");
	match("[[:upper:]]", "ø");
	match("[[:upper:]]", "λ");
	match("[[:upper:]]", "Λ");
	reject("[[:upper:]]", "1");
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

	cflags = 0 | REG_ICASE;
	run();
	cflags = REG_EXTENDED | REG_ICASE;
	run();
	return 0;
}
