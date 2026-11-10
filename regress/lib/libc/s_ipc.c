/* s_ipc.c - XSI message queues, semaphore sets and shared memory,
 * ftok; POSIX message queues (with notification by signal and by
 * thread); syslog, read back from a /dev/log socket of the test's own
 * (when there is none, and it may make one). */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL: line %d: %s\n", __LINE__, #c); fflush(stdout); fails++; } } while (0)

static void msleep(int ms)
{
	struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
	nanosleep(&ts, 0);
}

static void xsi(void)
{
	struct { long type; char text[32]; } m;
	struct msqid_ds mq;
	struct semid_ds sd;
	struct shmid_ds shd;
	struct sembuf op;
	unsigned short vals[3] = {1, 2, 3}, got[3];
	union { int val; struct semid_ds *buf; unsigned short *array; } arg;
	key_t k1, k2;
	int q, s, h, st;
	char *p;
	pid_t pid;

	k1 = ftok("/", 1);
	k2 = ftok("/", 2);
	CHECK(k1 != -1 && k1 != k2 && ftok("/no/such/file", 1) == -1);

	q = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
	CHECK(q >= 0);
	m.type = 2;
	strcpy(m.text, "second");
	CHECK(msgsnd(q, &m, sizeof m.text, 0) == 0);
	m.type = 1;
	strcpy(m.text, "first");
	CHECK(msgsnd(q, &m, sizeof m.text, 0) == 0);
	CHECK(msgctl(q, IPC_STAT, &mq) == 0 && mq.msg_qnum == 2 && mq.msg_qbytes > 0 &&
	      mq.msg_perm.uid == geteuid() && (mq.msg_perm.mode & 0777) == 0600 && mq.msg_lspid == getpid());
	CHECK(msgrcv(q, &m, sizeof m.text, 1, 0) == sizeof m.text && !strcmp(m.text, "first"));
	CHECK(msgrcv(q, &m, 3, 0, 0) == -1 && errno == E2BIG);
	CHECK(msgrcv(q, &m, 3, 0, MSG_NOERROR) == 3 && m.type == 2);
	CHECK(msgrcv(q, &m, sizeof m.text, 0, IPC_NOWAIT) == -1 && errno == ENOMSG);
	mq.msg_perm.mode = 0640;
	CHECK(msgctl(q, IPC_SET, &mq) == 0 && msgctl(q, IPC_STAT, &mq) == 0 && (mq.msg_perm.mode & 0777) == 0640);
	if (!(pid = fork())) {
		m.type = 5;
		strcpy(m.text, "child");
		_exit(msgsnd(q, &m, sizeof m.text, 0) != 0);
	}
	CHECK(msgrcv(q, &m, sizeof m.text, 5, 0) == sizeof m.text && !strcmp(m.text, "child"));
	waitpid(pid, &st, 0);
	CHECK(msgctl(q, IPC_RMID, 0) == 0 && msgsnd(q, &m, 1, 0) == -1);

	s = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600);
	CHECK(s >= 0);
	arg.array = vals;
	CHECK(semctl(s, 0, SETALL, arg) == 0);
	arg.array = got;
	CHECK(semctl(s, 0, GETALL, arg) == 0 && got[0] == 1 && got[1] == 2 && got[2] == 3);
	arg.val = 7;
	CHECK(semctl(s, 2, SETVAL, arg) == 0 && semctl(s, 2, GETVAL) == 7);
	memset(&sd, 0xff, sizeof sd);
	arg.buf = &sd;
	CHECK(semctl(s, 0, IPC_STAT, arg) == 0 && sd.sem_nsems == 3 && sd.sem_perm.cuid == geteuid());
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	CHECK(semop(s, &op, 1) == 0 && semctl(s, 0, GETVAL) == 0);
	op.sem_flg = IPC_NOWAIT;
	CHECK(semop(s, &op, 1) == -1 && errno == EAGAIN);
	if (!(pid = fork())) {
		struct sembuf up = {0, 1, 0};
		msleep(30);
		_exit(semop(s, &up, 1) != 0);
	}
	op.sem_flg = 0;
	CHECK(semop(s, &op, 1) == 0);       /* waits for the child */
	waitpid(pid, &st, 0);
	CHECK(semctl(s, 0, GETPID) == getpid() && semctl(s, 0, GETNCNT) == 0 && semctl(s, 0, GETZCNT) == 0);
	{       /* SEM_UNDO: undone when the process ends */
		if (!(pid = fork())) {
			struct sembuf u = {1, -2, SEM_UNDO};
			_exit(semop(s, &u, 1) != 0);
		}
		waitpid(pid, &st, 0);
		CHECK(semctl(s, 1, GETVAL) == 2);
	}
	CHECK(semctl(s, 0, IPC_RMID) == 0);
	CHECK(semget(IPC_PRIVATE, 70000, IPC_CREAT | 0600) == -1 && errno == EINVAL);

	h = shmget(IPC_PRIVATE, 10000, IPC_CREAT | 0600);
	CHECK(h >= 0);
	p = shmat(h, 0, 0);
	CHECK(p != (char *)-1);
	strcpy(p, "parent");
	if (!(pid = fork())) {
		char *c = shmat(h, 0, 0);
		int ok = c != (char *)-1 && !strcmp(c, "parent");
		strcpy(c, "child");
		shmdt(c);
		_exit(!ok);
	}
	waitpid(pid, &st, 0);
	CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0 && !strcmp(p, "child"));
	CHECK(shmctl(h, IPC_STAT, &shd) == 0 && shd.shm_segsz == 10000 && shd.shm_nattch == 1 &&
	      shd.shm_cpid == getpid());
	{
		char *ro = shmat(h, 0, SHM_RDONLY);
		CHECK(ro != (char *)-1 && !strcmp(ro, "child"));
		shmdt(ro);
	}
	CHECK(shmdt(p) == 0 && shmdt(p) == -1 && errno == EINVAL);
	CHECK(shmctl(h, IPC_RMID, 0) == 0);
}

static volatile sig_atomic_t got_sig;
static void on_sig(int s, siginfo_t *si, void *c) { (void)c; got_sig = s == SIGUSR1 && si->si_value.sival_int == 42; }

static volatile int notified;
static void notify(union sigval v) { notified = v.sival_int; }

static void posix_mq(void)
{
	struct mq_attr a, old;
	struct sigevent sev;
	struct sigaction sa;
	struct timespec at;
	char buf[64];
	unsigned prio;
	mqd_t q;

	memset(&a, 0, sizeof a);
	a.mq_maxmsg = 4;
	a.mq_msgsize = 64;
	mq_unlink("/s_ipc_test");
	q = mq_open("/s_ipc_test", O_RDWR | O_CREAT | O_EXCL, 0600, &a);
	if (q == (mqd_t)-1 && (errno == ENOSYS || errno == ENOENT)) {
		printf("message queues: not supported here (%s)\n", strerror(errno));
		return;
	}
	CHECK(q != (mqd_t)-1);
	CHECK(mq_open("/s_ipc_test", O_RDWR | O_CREAT | O_EXCL, 0600, &a) == (mqd_t)-1 && errno == EEXIST);
	CHECK(mq_send(q, "low", 3, 1) == 0 && mq_send(q, "high", 4, 9) == 0);
	CHECK(mq_getattr(q, &a) == 0 && a.mq_curmsgs == 2 && a.mq_maxmsg == 4 && a.mq_msgsize == 64);
	CHECK(mq_receive(q, buf, sizeof buf, &prio) == 4 && prio == 9 && !memcmp(buf, "high", 4));
	CHECK(mq_receive(q, buf, 10, &prio) == -1 && errno == EMSGSIZE);
	CHECK(mq_receive(q, buf, sizeof buf, &prio) == 3 && prio == 1);
	clock_gettime(CLOCK_REALTIME, &at);
	at.tv_nsec += 20000000;
	if (at.tv_nsec >= 1000000000) { at.tv_sec++; at.tv_nsec -= 1000000000; }
	CHECK(mq_timedreceive(q, buf, sizeof buf, 0, &at) == -1 && errno == ETIMEDOUT);
	a.mq_flags = O_NONBLOCK;
	CHECK(mq_setattr(q, &a, &old) == 0 && !(old.mq_flags & O_NONBLOCK));
	CHECK(mq_receive(q, buf, sizeof buf, 0) == -1 && errno == EAGAIN);

	/* notification: a signal, then a thread */
	memset(&sa, 0, sizeof sa);
	sa.sa_sigaction = on_sig;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, 0);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;
	sev.sigev_value.sival_int = 42;
	CHECK(mq_notify(q, &sev) == 0);
	CHECK(mq_notify(q, &sev) == -1 && errno == EBUSY);
	if (!fork()) {
		mqd_t c = mq_open("/s_ipc_test", O_WRONLY);
		_exit(mq_send(c, "x", 1, 0) != 0);
	}
	wait(0);
	msleep(20);
	CHECK(got_sig);
	CHECK(mq_receive(q, buf, sizeof buf, 0) == 1);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = notify;
	sev.sigev_value.sival_int = 77;
	CHECK(mq_notify(q, &sev) == 0);
	CHECK(mq_send(q, "y", 1, 0) == 0);
	msleep(50);
	CHECK(notified == 77);
	CHECK(mq_close(q) == 0);
	CHECK(mq_unlink("/s_ipc_test") == 0 && mq_unlink("/s_ipc_test") == -1 && errno == ENOENT);
	signal(SIGUSR1, SIG_DFL);
}

static void logging(const char *prog)
{
	struct sockaddr_un sa = {AF_UNIX, "/dev/log"};
	char buf[512];
	int fd, n, pri;
	struct stat st;
	if (stat("/dev/log", &st) == 0) {
		printf("syslog: /dev/log exists: not tested\n");
		return;
	}
	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0) {
		printf("syslog: cannot make /dev/log: not tested\n");
		close(fd);
		return;
	}
	openlog("s_ipc", LOG_PID | LOG_NDELAY, LOG_LOCAL3);
	syslog(LOG_WARNING, "value %d, %m, 100%%", 5);
	n = (int)recv(fd, buf, sizeof buf - 1, 0);
	buf[n > 0 ? n : 0] = 0;
	CHECK(sscanf(buf, "<%d>", &pri) == 1 && pri == (LOG_LOCAL3 | LOG_WARNING));
	CHECK(strstr(buf, "s_ipc[") && strstr(buf, "]: value 5, ") && strstr(buf, ", 100%"));
	{       /* the timestamp: "Mmm dd hh:mm:ss " */
		char *p = strchr(buf, '>') + 1;
		CHECK(strlen(p) > 16 && p[3] == ' ' && p[6] == ' ' && p[9] == ':' && p[12] == ':' && p[15] == ' ');
	}
	CHECK(setlogmask(LOG_UPTO(LOG_ERR)) == 0xff);
	syslog(LOG_INFO, "masked");
	syslog(LOG_ERR | LOG_MAIL, "unmasked");
	n = (int)recv(fd, buf, sizeof buf - 1, MSG_DONTWAIT);
	buf[n > 0 ? n : 0] = 0;
	CHECK(n > 0 && strstr(buf, "unmasked") && sscanf(buf, "<%d>", &pri) == 1 && pri == (LOG_MAIL | LOG_ERR));
	CHECK(recv(fd, buf, sizeof buf, MSG_DONTWAIT) == -1);
	closelog();
	openlog(0, 0, 0);
	setlogmask(0xff);
	syslog(LOG_DEBUG, "x");
	n = (int)recv(fd, buf, sizeof buf - 1, 0);
	buf[n > 0 ? n : 0] = 0;
	{       /* the tag: the program's name */
		char want[256];
		snprintf(want, sizeof want, " %s: x", prog);
		CHECK(strstr(buf, want) != 0);
	}
	closelog();
	close(fd);
	unlink("/dev/log");
}

int main(int argc, char **argv)
{
	const char *prog = argc ? strrchr(argv[0], '/') : 0;
	xsi();
	posix_mq();
	logging(prog ? prog + 1 : argv[0]);
	printf("%d failures\n", fails);
	return fails != 0;
}
