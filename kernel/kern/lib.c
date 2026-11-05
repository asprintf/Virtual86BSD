/* lib.c - string functions and vsnprintf for the kernel. */
#include "kernel.h"

void *memset(void *d, int c, size_t n) {
  unsigned char *p = d;
  if (((uintptr_t)p & 7) == 0 && (n & 7) == 0) {
    uint64_t v = (unsigned char)c;
    v |= v << 8;
    v |= v << 16;
    v |= v << 32;
    uint64_t *q = d;
    for (size_t i = 0; i < n / 8; i++)
      q[i] = v;
    return d;
  }
  while (n--)
    *p++ = (unsigned char)c;
  return d;
}

void *memcpy(void *d, const void *s, size_t n) {
  if ((((uintptr_t)d | (uintptr_t)s | n) & 7) == 0) {
    uint64_t *q = d;
    const uint64_t *r = s;
    for (size_t i = 0; i < n / 8; i++)
      q[i] = r[i];
    return d;
  }
  unsigned char *p = d;
  const unsigned char *q = s;
  while (n--)
    *p++ = *q++;
  return d;
}

void *memmove(void *d, const void *s, size_t n) {
  unsigned char *p = d;
  const unsigned char *q = s;
  if (p == q || n == 0)
    return d;
  if (p < q || p >= q + n)
    return memcpy(d, s, n);
  while (n--)
    p[n] = q[n];
  return d;
}

int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *p = a, *q = b;
  for (size_t i = 0; i < n; i++)
    if (p[i] != q[i])
      return p[i] - q[i];
  return 0;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n])
    n++;
  return n;
}

size_t strnlen(const char *s, size_t max) {
  size_t n = 0;
  while (n < max && s[n])
    n++;
  return n;
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b)
    a++, b++;
  return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
  for (; n; n--, a++, b++) {
    if (*a != *b)
      return (unsigned char)*a - (unsigned char)*b;
    if (!*a)
      return 0;
  }
  return 0;
}

char *strcpy(char *d, const char *s) {
  char *r = d;
  while ((*d++ = *s++))
    ;
  return r;
}

char *strchr(const char *s, int c) {
  for (;; s++) {
    if (*s == (char)c)
      return (char *)s;
    if (!*s)
      return NULL;
  }
}

char *strrchr(const char *s, int c) {
  const char *r = NULL;
  for (;; s++) {
    if (*s == (char)c)
      r = s;
    if (!*s)
      return (char *)r;
  }
}

/* vsnprintf: %d %i %u %x %X %o %p %s %c %%, flags - 0 space +, width,
   precision for %s, length modifiers l ll z h hh. */
struct out {
  char *buf;
  size_t n, len;
};

static void put(struct out *o, char c) {
  if (o->len + 1 < o->n)
    o->buf[o->len] = c;
  o->len++;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
  struct out o = {buf, n, 0};
  for (; *fmt; fmt++) {
    if (*fmt != '%') {
      put(&o, *fmt);
      continue;
    }
    fmt++;
    int left = 0, zero = 0, plus = 0, space = 0, width = 0, prec = -1, lng = 0;
    for (;; fmt++) {
      if (*fmt == '-')
        left = 1;
      else if (*fmt == '0')
        zero = 1;
      else if (*fmt == '+')
        plus = 1;
      else if (*fmt == ' ')
        space = 1;
      else
        break;
    }
    if (*fmt == '*') {
      width = va_arg(ap, int);
      fmt++;
    } else
      while (*fmt >= '0' && *fmt <= '9')
        width = width * 10 + (*fmt++ - '0');
    if (*fmt == '.') {
      fmt++;
      prec = 0;
      if (*fmt == '*') {
        prec = va_arg(ap, int);
        fmt++;
      } else
        while (*fmt >= '0' && *fmt <= '9')
          prec = prec * 10 + (*fmt++ - '0');
    }
    while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
      if (*fmt != 'h')
        lng = 1;
      fmt++;
    }
    char tmp[24];
    const char *s = tmp;
    int slen = 0, neg = 0;
    switch (*fmt) {
    case 'c':
      tmp[0] = (char)va_arg(ap, int);
      slen = 1;
      break;
    case 's':
      s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      slen = prec >= 0 ? (int)strnlen(s, prec) : (int)strlen(s);
      zero = 0;
      break;
    case 'd':
    case 'i':
    case 'u':
    case 'x':
    case 'X':
    case 'o':
    case 'p': {
      uint64_t v;
      int base = 10;
      if (*fmt == 'p') {
        v = (uint64_t)va_arg(ap, void *);
        base = 16;
        lng = 1;
      } else if (*fmt == 'd' || *fmt == 'i') {
        int64_t sv = lng ? va_arg(ap, long) : va_arg(ap, int);
        if (sv < 0) {
          neg = 1;
          v = -(uint64_t)sv;
        } else
          v = sv;
      } else {
        v = lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
        base = *fmt == 'o' ? 8 : *fmt == 'u' ? 10 : 16;
      }
      const char *dig = *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
      char *e = tmp + sizeof(tmp);
      char *p = e;
      do {
        *--p = dig[v % base];
        v /= base;
      } while (v);
      if (*fmt == 'p') {
        *--p = 'x';
        *--p = '0';
      }
      s = p;
      slen = (int)(e - p);
      break;
    }
    case '%':
      put(&o, '%');
      continue;
    default:
      put(&o, '%');
      put(&o, *fmt);
      if (!*fmt)
        fmt--;
      continue;
    }
    char sign = neg ? '-' : plus ? '+' : space ? ' ' : 0;
    int total = slen + (sign != 0);
    if (!left && !zero)
      for (int i = total; i < width; i++)
        put(&o, ' ');
    if (sign)
      put(&o, sign);
    if (!left && zero)
      for (int i = total; i < width; i++)
        put(&o, '0');
    for (int i = 0; i < slen; i++)
      put(&o, s[i]);
    if (left)
      for (int i = total; i < width; i++)
        put(&o, ' ');
  }
  if (o.n)
    o.buf[o.len < o.n ? o.len : o.n - 1] = 0;
  return (int)o.len;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf, n, fmt, ap);
  va_end(ap);
  return r;
}
