/* k_shm.c - shmat/shmdt sequences */
#define _XOPEN_SOURCE 800
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  int h = shmget(IPC_PRIVATE, 10000, IPC_CREAT | 0600);
  char *p = shmat(h, 0, 0);
  printf("p=%p\n", (void *)p);
  strcpy(p, "parent");
  if (!fork()) {
    char *c = shmat(h, 0, 0);
    printf("child c=%p %s\n", (void *)c, c);
    strcpy(c, "child");
    shmdt(c);
    _exit(0);
  }
  wait(0);
  char *ro = shmat(h, 0, SHM_RDONLY);
  printf("ro=%p %s\n", (void *)ro, ro);
  printf("shmdt ro %d\n", shmdt(ro));
  int a = shmdt(p);
  int ea = errno;
  int b = shmdt(p);
  printf("shmdt p %d %d, again %d errno %d\n", a, ea, b, errno);
  FILE *m = fopen("/proc/self/maps", "r");
  char l[256];
  while (fgets(l, sizeof l, m))
    fputs(l, stdout);
  return 0;
}
