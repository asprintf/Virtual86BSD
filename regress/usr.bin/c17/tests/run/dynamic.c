/* what dynamic linking has to get right, and static linking too: a
   variable of the C library used directly (copied into the executable,
   with its aliases), function addresses, a standard stream */
#define _POSIX_C_SOURCE 202405L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char **environ;

int main(void) {
  setenv("C17_TEST_VARIABLE", "yes", 1);  /* the library changes environ */
  int found = 0;
  for (char **e = environ; *e; e++)
    if (!strcmp(*e, "C17_TEST_VARIABLE=yes"))
      found = 1;
  printf("environ seen by the program and the library: %d %d\n", found,
         getenv("C17_TEST_VARIABLE") != NULL);
  int (*f)(const char *) = puts;
  volatile void *p = (void *)puts;
  printf("one address for puts: %d\n", (void *)f == p);
  fputs("stdout\n", stdout);
  fflush(stdout);
  return 0;
}
