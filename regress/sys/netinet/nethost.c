/* nethost - the host side of tests/k_net (POSIX C; built with the host's
   compiler: cc -o nethost tools/nethost.c).

   usage: nethost TPORT UPORT FWDPORT RESULT
   A TCP echo server on 127.0.0.1:TPORT and a UDP echo server on
   127.0.0.1:UPORT (QEMU's user networking shows the host as 10.0.2.2),
   and a client that connects to 127.0.0.1:FWDPORT (forwarded to the
   guest's port 7777 with hostfwd=tcp:127.0.0.1:FWDPORT-:7777), sends a
   line, and writes the answer to the file RESULT.  It exits after that,
   or after ten minutes. */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static struct sockaddr_in local(int port) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons((unsigned short)port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return a;
}

static void tcp_echo(int port) {
  int l = socket(AF_INET, SOCK_STREAM, 0), v = 1;
  struct sockaddr_in a = local(port);
  setsockopt(l, SOL_SOCKET, SO_REUSEADDR, &v, sizeof v);
  if (bind(l, (struct sockaddr *)&a, sizeof a) || listen(l, 8)) {
    perror("nethost: tcp");
    exit(1);
  }
  for (;;) {
    int c = accept(l, 0, 0);
    if (c < 0)
      continue;
    if (fork() == 0) {
      char b[65536];
      ssize_t n;
      while ((n = read(c, b, sizeof b)) > 0)
        for (ssize_t o = 0; o < n;) {
          ssize_t w = write(c, b + o, (size_t)(n - o));
          if (w <= 0)
            _exit(0);
          o += w;
        }
      _exit(0);
    }
    close(c);
  }
}

static void udp_echo(int port) {
  int u = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in a = local(port), from;
  char b[65536];
  if (bind(u, (struct sockaddr *)&a, sizeof a)) {
    perror("nethost: udp");
    exit(1);
  }
  for (;;) {
    socklen_t fl = sizeof from;
    ssize_t n = recvfrom(u, b, sizeof b, 0, (struct sockaddr *)&from, &fl);
    if (n >= 0)
      sendto(u, b, (size_t)n, 0, (struct sockaddr *)&from, fl);
  }
}

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: nethost TPORT UPORT FWDPORT RESULT\n");
    return 2;
  }
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  pid_t t = fork();
  if (t == 0)
    tcp_echo(atoi(argv[1]));
  pid_t u = fork();
  if (u == 0)
    udp_echo(atoi(argv[2]));
  int ok = 0;
  for (time_t end = time(0) + 600; !ok && time(0) < end; sleep(1)) {
    int c = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = local(atoi(argv[3]));
    struct timeval tv = {60, 0};
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    if (connect(c, (struct sockaddr *)&a, sizeof a) == 0 && write(c, "hello from the host\n", 20) == 20) {
      char got[256];
      size_t k = 0;
      ssize_t n;
      while (k < sizeof got - 1 && (n = read(c, got + k, 1)) == 1 && got[k] != '\n')
        k++;
      if (k && got[k] == '\n') {
        FILE *f = fopen(argv[4], "w");
        if (f) {
          fwrite(got, 1, k + 1, f);
          fclose(f);
        }
        ok = 1;
      }
    }
    close(c);
  }
  kill(t, SIGTERM);
  kill(u, SIGTERM);
  return ok ? 0 : 1;
}
