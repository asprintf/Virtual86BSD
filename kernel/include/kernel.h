/* kernel.h - basic types and helpers used everywhere in the kernel. */
#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
typedef int64_t ssize_t;
typedef int64_t off_t;
typedef int32_t pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t mode_t;
typedef uint64_t dev_t;
typedef uint64_t ino_t;

#define KERNEL_BASE 0xffffffff80000000UL  /* the image: phys + KERNEL_BASE */
#define DIRECT_MAP  0xffff888000000000UL  /* all of RAM: phys + DIRECT_MAP */
#define USER_TOP    0x0000800000000000UL  /* user addresses are below this */

#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_DOWN(x) ((x) & PAGE_MASK)

#define P2V(p) ((void *)((uint64_t)(p) + DIRECT_MAP))
#define V2P(v) ((uint64_t)(v) - DIRECT_MAP)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define container_of(p, type, member) ((type *)((char *)(p) - offsetof(type, member)))

#define NORETURN _Noreturn

/* kern/lib.c */
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...);

/* kern/printk.c */
void printk(const char *fmt, ...);
NORETURN void panic(const char *fmt, ...);
#define assert(c) ((c) ? (void)0 : panic("assertion failed: %s (%s:%d)", #c, __FILE__, __LINE__))

/* intrusive doubly linked list */
struct list {
  struct list *next, *prev;
};
#define LIST_INIT(l) { &(l), &(l) }
static inline void list_init(struct list *l) { l->next = l->prev = l; }
static inline bool list_empty(const struct list *l) { return l->next == l; }
static inline void list_add_tail(struct list *l, struct list *e) {
  e->prev = l->prev;
  e->next = l;
  l->prev->next = e;
  l->prev = e;
}
static inline void list_add(struct list *l, struct list *e) {
  e->next = l->next;
  e->prev = l;
  l->next->prev = e;
  l->next = e;
}
static inline void list_del(struct list *e) {
  e->prev->next = e->next;
  e->next->prev = e->prev;
  e->next = e->prev = e;
}
#define list_entry(p, type, member) container_of(p, type, member)
#define list_for_each(p, l) for (struct list *p = (l)->next; p != (l); p = p->next)
#define list_for_each_safe(p, n, l) \
  for (struct list *p = (l)->next, *n = p->next; p != (l); p = n, n = p->next)

/* errno values (Linux) */
#include "errno.h"

/* error-carrying pointers, as Linux */
#define ERR_PTR(e) ((void *)(intptr_t)(e))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)

/* timekeeping: kern/time.c */
#define HZ 1000
extern volatile uint64_t jiffies;
uint64_t monotonic_ns(void);
static inline uint64_t sat_add(uint64_t a, uint64_t b) { return a + b < a ? ~0UL : a + b; }
static inline uint64_t ts_to_ns(int64_t sec, int64_t nsec) {
  if ((uint64_t)sec >= 18000000000UL)
    return ~0UL;
  return (uint64_t)sec * 1000000000UL + (uint64_t)nsec;
}
uint64_t realtime_ns(void);
void time_init(void);
void set_realtime_ns(uint64_t ns);

#endif
