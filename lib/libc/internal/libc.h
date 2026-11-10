/* Internal: process-wide library state. */
#ifndef _INTERNAL_LIBC_H
#define _INTERNAL_LIBC_H
#include <stddef.h>

struct __pthread;

struct __tls_module {
	struct __tls_module *next;
	void *image;        /* initialisation image (.tdata) */
	size_t len;         /* bytes of image */
	size_t size;        /* total size (.tdata + .tbss) */
	size_t align;
	size_t offset;      /* distance from the thread pointer down to the block */
	size_t id;          /* its index in the dynamic thread vector (dtv) */
};

struct __libc {
	unsigned long *auxv;
	size_t page_size;
	int threaded;               /* set once a second thread was created */
	volatile int threads_minus_1;  /* the number of threads less one */
	int secure;                 /* AT_SECURE */
	struct __tls_module *tls_head;  /* the modules of the static TLS area */
	size_t tls_cnt;             /* how many (the dtv has one more entry) */
	size_t tls_area_size;       /* TLS + TCB, rounded, for new threads */
	size_t tls_align;
	/* set by the dynamic linker (libc.so): it ran the start-up, and runs
	 * the constructors and destructors of all modules */
	void (*dl_init)(int, char **, char **);
	void (*dl_fini)(void);
	void *(*tls_get_new)(unsigned long *);   /* a dlopen'ed module's TLS */
	void (*tls_free)(struct __pthread *);     /* at thread exit */
};

extern struct __libc __libc;
extern char **__environ;


void __init_tls(unsigned long *auxv);
struct __pthread *__copy_tls(unsigned char *mem);
void __init_tp(struct __pthread *);
void __tls_layout(void);
void __init_main_tls(void);
void __libc_exit_fini(void);
void __funcs_on_exit(void);
int __set_thread_area(void *);

#endif
