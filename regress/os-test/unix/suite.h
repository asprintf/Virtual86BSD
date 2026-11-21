#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../misc/errors.h"

__attribute__((unused))
static char* create_tmpdir(void)
{
	const char* tmpdir = getenv("TMPDIR");
	if ( !tmpdir )
		tmpdir = "/tmp";
	size_t template_len = strlen(tmpdir) + strlen("/os-test.XXXXXX");
	char* template = malloc(template_len + 1);
	if ( !template )
		err(1, "malloc");
	// mkdtemp is unfortunately less portable than link, so emulate it.
	while ( 1 )
	{
		strcpy(template, tmpdir);
		strcat(template, "/os-test.XXXXXX");
		int fd = mkstemp(template);
		if ( fd < 0 )
			err(1, "mkstemp");
		close(fd);
		if ( unlink(template) < 0 )
			err(1, "unlink");
		if ( mkdir(template, 0700) < 0 )
		{
			if ( errno == EEXIST )
				continue;
			err(1, "mkdir");
		}
		break;
	}
	return template;
}

__attribute__((unused))
static char* join_paths(const char* a, const char* b)
{
	size_t a_len = strlen(a);
	size_t b_len = strlen(b);
	bool has_slash = (a_len && a[a_len-1] == '/') || b[0] == '/';
	size_t size = a_len + !has_slash + b_len + 1;
	char* result = malloc(size);
	memcpy(result, a, a_len);
	if ( !has_slash )
		result[a_len] = '/';
	memcpy(result + a_len + !has_slash, b, b_len);
	result[size - 1] = '\0';
	return result;
}
