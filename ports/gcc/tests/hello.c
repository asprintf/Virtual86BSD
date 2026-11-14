#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) { (void)argv; printf("hello from gcc, argc=%d, %s\n", argc, getenv("HOME") ? getenv("HOME") : "(no HOME)"); return 0; }
