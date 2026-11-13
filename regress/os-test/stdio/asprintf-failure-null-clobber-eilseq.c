/* Test whether asprintf sets the output pointer to null on failure, in the case
   that the function is supposed to fail EILSEQ. */

#include <locale.h>

#include "suite.h"

int main(void)
{
	// Install a UTF-8 locale to make sure we get an encoding failure in the
	// asprintf call below. This would probably also work in the C locale but
	// it's probably safer in a UTF-8 locale.
	const char* utf8_locale = getenv("OS_TEST_UTF8_LOCALE");
	if ( !utf8_locale )
		utf8_locale = "C.UTF-8";
	if ( !setlocale(LC_CTYPE, utf8_locale) &&
	     !setlocale(LC_CTYPE, "C.UTF-8") &&
	     !setlocale(LC_CTYPE, "POSIX.UTF-8") &&
	     !setlocale(LC_CTYPE, "en_US.UTF-8") )
	{
		// Sad but the test can continue since the wchar_t below won't fit in
		// the C locale anyway since it's an invalid codepoint.
	}
	// See if this output pointer is unchanged, clobbered to NULL, or
	// clobbered to some other value in the event that asprintf fails.
	char* old = "old";
	char* output = old;
	errno = 0;
	// Try to provoke an EILSEQ failure with an invalid unicode codepoint.
	wchar_t bad = 0xD800; // UTF-16 surrogates must never occur in UTF-8.
	int ret = asprintf(&output, "a%lcb", bad);
	int old_errno = errno;
	// asprintf should never succeed because of the encoding failure.
	if ( 0 <= ret )
		errx(2, "asprintf did not fail (returned %d): %s", ret, output);
	// Record how asprintf failed.
	puts(strerrno(old_errno));
	// asprintf could leave the old value behind in the output pointer;
	if ( output == old )
		printf("output == old\n");
	// or asprintf could set the output buffer to NULL;
	else if ( output == NULL )
		printf("output == NULL\n");
	// or asprintf could leak one of its own temporary buffers on error.
	else
		printf("output != old && output != NULL\n");
	return 0;
}
