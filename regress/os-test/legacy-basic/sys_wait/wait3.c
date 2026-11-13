/*[SUSv2]*/
/* Test whether wait3 waits for any child and gives its resource usage. */

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../basic.h"

int main(void)
{
	pid_t pid = fork();
	if ( pid < 0 )
		err(1, "fork");
	if ( pid == 0 )
	{
		volatile unsigned long n = 0;
		for ( unsigned long i = 0; i < 50000000; i++ )
			n += i;
		_exit(7);
	}
	int status;
	struct rusage ru;
	if ( wait3(&status, 0, &ru) != pid )
		err(1, "wait3");
	if ( !WIFEXITED(status) || WEXITSTATUS(status) != 7 )
		errx(1, "wait3's status is not an exit with 7");
	if ( ru.ru_utime.tv_sec == 0 && ru.ru_utime.tv_usec == 0 )
		errx(1, "wait3 gave no user time for a child that used some");
	if ( wait3(&status, WNOHANG, NULL) != -1 )
		errx(1, "wait3 with no child left did not fail");
	return 0;
}
