/*[nonstandard]*/
/* Test whether brackets work with UTF-8 encoding (insensitive). */

#include "suite.h"

#include <locale.h>

void run(void)
{
	match("[æø]", "æ");
	match("[æø]", "Æ");
	match("[æø]", "ø");
	match("[æø]", "Ø");
	reject("[æø]", "å");
	reject("[æø]", "Å");

	reject("[!æø]", "æ");
	reject("[!æø]", "Æ");
	reject("[!æø]", "ø");
	reject("[!æø]", "Ø");
	match("[!æø]", "å");
	match("[!æø]", "Å");

	match("[é]", "é");
	match("[é]", "É");
	reject("[é]", "Ā");

	match("[Ā]", "Ā");
	match("[Ā]", "ā");
	reject("[Ā]", "é");

	match("[€]", "€");
	reject("[€]", "é");

	match("[λ]", "λ");
	match("[λ]", "Λ");
	reject("[λ]", "é");

	match("[Aλ]", "A");
	match("[Aλ]", "λ");
	match("[Aλ]", "a");
	match("[Aλ]", "Λ");

	match("[Aλ€]", "A");
	match("[Aλ€]", "λ");
	match("[Aλ€]", "€");
	match("[Aλ€]", "a");
	match("[Aλ€]", "Λ");
	match("[Aλ€]", "a");
	reject("[Aλ€]", "𝄞");
	reject("[Aλ€]", "é");

	match("[Aλ€𝄞]", "A");
	match("[Aλ€𝄞]", "λ");
	match("[Aλ€𝄞]", "€");
	match("[Aλ€𝄞]", "𝄞");
	match("[Aλ€𝄞]", "a");
	match("[Aλ€𝄞]", "Λ");
	reject("[Aλ€𝄞]", "é");

	reject("[!Aλ€𝄞]", "A");
	reject("[!Aλ€𝄞]", "λ");
	reject("[!Aλ€𝄞]", "€");
	reject("[!Aλ€𝄞]", "𝄞");
	reject("[!Aλ€𝄞]", "a");
	reject("[!Aλ€𝄞]", "Λ");
	match("[!Aλ€𝄞]", "é");

	match("[!a]", "æ");
	match("[!a]", "Æ");
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

	flags = FNM_CASEFOLD;
	run();
	return 0;
}
