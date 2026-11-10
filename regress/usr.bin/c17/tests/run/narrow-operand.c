#include <stdint.h>
#include <stdio.h>
static int64_t safe_mul_func_int64_t_s_s(int64_t a, int64_t b) { return a * b; }
static uint64_t safe_sub_func_uint64_t_u_u(uint64_t a, uint64_t b) { return a - b; }
static uint8_t safe_mod_func_uint8_t_u_u(uint8_t a, uint8_t b) { return b ? a % b : a; }
static uint32_t safe_mul_func_uint32_t_u_u(uint32_t a, uint32_t b) { return a * b; }
static uint16_t safe_lshift_func_uint16_t_u_u(uint16_t a, unsigned b) { return (b >= 32 || a > (65535 >> b)) ? a : (uint16_t)(a << b); }
static uint16_t safe_sub_func_uint16_t_u_u(uint16_t a, uint16_t b) { return a - b; }
static int16_t g_8 = 8L;
int main(void) {
  int16_t l_23 = 0xB9A7L;
  uint64_t a = safe_mul_func_int64_t_s_s(l_23, g_8);
  uint64_t b = safe_sub_func_uint64_t_u_u(a, 0xC6F8B8AE505B0BB8LL);
  int c = ((b, 0xA5F9C710L) <= l_23);
  uint8_t d = safe_mod_func_uint8_t_u_u(c, g_8);
  uint32_t e = safe_mul_func_uint32_t_u_u(d, (-4L));
  uint64_t f = e ^ 0xAA53F0D3ECF04B06LL;
  uint16_t g = safe_lshift_func_uint16_t_u_u(f, 11);
  uint16_t h = safe_sub_func_uint16_t_u_u(g, l_23);
  printf("%llx %llx %d %u %u %llx %u %u\n", (unsigned long long)a, (unsigned long long)b, c, d, e, (unsigned long long)f, g, h);
  return 0;
}
