/* A SIGKILL sent to one thread ends the whole process (the kernel used to
 * end that thread only, so a process killed out of memory exited with 0). */
#define _POSIX_C_SOURCE 202405L
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void *killself(void *arg)
{
	(void)arg;
	pthread_kill(pthread_self(), SIGKILL);
	for (;;)
		pause();
}

static void *sleeper(void *arg)
{
	(void)arg;
	for (;;)
		pause();
}

/* the child: a second thread is sent SIGKILL by itself or by main */
static void child(int self)
{
	pthread_t t;

	if (self) {
		pthread_create(&t, NULL, killself, NULL);
		pthread_join(t, NULL);
	} else {
		pthread_create(&t, NULL, sleeper, NULL);
		pthread_kill(t, SIGKILL);
		sleep(5);
	}
	_exit(0);
}

int main(void)
{
	for (int self = 0; self < 2; self++) {
		int st;
		pid_t pid = fork();

		if (pid == 0)
			child(self);
		CHECK(waitpid(pid, &st, 0) == pid);
		printf("%s: status %#x\n", self ? "thread kills itself" : "main kills a thread", st);
		CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGKILL);
	}
	if (!failures)
		printf("k_thrkill ok\n");
	return failures != 0;
}
