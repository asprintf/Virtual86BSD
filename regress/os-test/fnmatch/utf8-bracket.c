/*[nonstandard]*/
/* Test whether brackets work with UTF-8 encoding. */

#include "suite.h"

#include <locale.h>

void run(void)
{
	match("[æø]", "æ");
	reject("[æø]", "Æ");
	match("[æø]", "ø");
	reject("[æø]", "Ø");
	reject("[æø]", "å");
	reject("[æø]", "Å");

	reject("[!æø]", "æ");
	match("[!æø]", "Æ");
	reject("[!æø]", "ø");
	match("[!æø]", "Ø");
	match("[!æø]", "å");
	match("[!æø]", "Å");

	match("[é]", "é");
	reject("[é]", "É");
	reject("[é]", "Ā");

	match("[Ā]", "Ā");
	reject("[Ā]", "ā");
	reject("[Ā]", "é");

	match("[€]", "€");
	reject("[€]", "é");

	match("[λ]", "λ");
	reject("[λ]", "Λ");
	reject("[λ]", "é");

	match("[Aλ]", "A");
	match("[Aλ]", "λ");
	reject("[Aλ]", "a");
	reject("[Aλ]", "Λ");

	match("[Aλ€]", "A");
	match("[Aλ€]", "λ");
	match("[Aλ€]", "€");
	reject("[Aλ€]", "a");
	reject("[Aλ€]", "Λ");
	reject("[Aλ€]", "a");
	reject("[Aλ€]", "𝄞");
	reject("[Aλ€]", "é");

	match("[Aλ€𝄞]", "A");
	match("[Aλ€𝄞]", "λ");
	match("[Aλ€𝄞]", "€");
	match("[Aλ€𝄞]", "𝄞");
	reject("[Aλ€𝄞]", "a");
	reject("[Aλ€𝄞]", "Λ");
	reject("[Aλ€𝄞]", "é");

	reject("[!Aλ€𝄞]", "A");
	reject("[!Aλ€𝄞]", "λ");
	reject("[!Aλ€𝄞]", "€");
	reject("[!Aλ€𝄞]", "𝄞");
	match("[!Aλ€𝄞]", "a");
	match("[!Aλ€𝄞]", "Λ");
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

	flags = 0;
	run();
	return 0;
}
