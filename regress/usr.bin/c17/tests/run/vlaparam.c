/* parameters whose types use earlier parameters (C17 6.7.6.3, 6.9.1p10):
   variably modified parameters, in prototypes and definitions */
#include <stddef.h>
#include <stdio.h>

void fill(size_t n, int a[n]);
static double sum(int rows, int cols, double m[rows][cols]);

void fill(size_t n, int a[n]) {
  for (size_t i = 0; i < n; i++)
    a[i] = (int)(i * i);
}

static double sum(int rows, int cols, double m[rows][cols]) {
  double s = 0;
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < cols; j++)
      s += m[i][j];
  printf("row size %zu, element size %zu\n", sizeof *m, sizeof m[0][0]);
  return s;
}

static int last(int n, int a[static n]) { return a[n - 1]; }

int main(void) {
  int v[5];
  fill(5, v);
  double m[3][4];
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      m[i][j] = i * 10 + j;
  printf("%d %d %g %d\n", v[2], v[4], sum(3, 4, m), last(5, v));
  return 0;
}
