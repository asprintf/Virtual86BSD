/* param.h - system parameters and helper macros. */
#ifndef _SYS_PARAM_H_
#define _SYS_PARAM_H_

#include <sys/types.h>
#include <machine/vmparam.h>
#ifdef _KERNEL
#include <options.h> /* the options of the configuration (config(8)) */
#endif

#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_UP(x) (((x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_DOWN(x) ((x) & PAGE_MASK)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define container_of(p, type, member) ((type *)((char *)(p) - offsetof(type, member)))

#define NORETURN _Noreturn

#endif
