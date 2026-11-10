/* rt_bits.c - c17: run-time support: bit counting, as the C libraries'
   static archives refer to it (__popcountdi2, from glibc's libc.a). */
int __popcountdi2(long long x) {
  unsigned long long v = (unsigned long long)x;
  int n = 0;
  while (v) {
    v &= v - 1;
    n++;
  }
  return n;
}
