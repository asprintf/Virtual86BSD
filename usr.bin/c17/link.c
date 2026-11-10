/* link.c - c17: the link editor: ELF-64 relocatable objects, ar archives
   and shared objects to an x86-64 executable, static or dynamic.
 *
 * Choices:
 *  - Executables are not position-independent (ET_EXEC at 0x400000).  A
 *    dynamic executable names the C library's dynamic linker (PT_INTERP)
 *    and the shared objects it needs (DT_NEEDED, in the order given; those
 *    of an AS_NEEDED list of a linker script only if they provide a
 *    symbol), with DT_RUNPATH for -R.
 *  - A function of a shared object is called through a PLT entry; if its
 *    address is taken other than through the GOT, the PLT entry is its
 *    address (canonical).  A variable of a shared object used directly is
 *    copied into the executable (R_X86_64_COPY), with its aliases.  GOT
 *    entries of imported symbols are filled by the dynamic linker.  All
 *    bindings are made at load time (DF_BIND_NOW); version requirements
 *    (DT_VERNEED) record the versions bound.
 *  - Symbols of the executable that shared objects refer to or also
 *    define are exported (in .dynsym).
 *  - Linker scripts are understood as far as the C libraries use them:
 *    GROUP, INPUT, AS_NEEDED, OUTPUT_FORMAT.
 *  - An archive is searched where it appears among the inputs, repeatedly
 *    until it defines no more undefined symbols.  The trailing inputs from
 *    LinkOptions.group_start on (the C library and c17's runtime support)
 *    are in addition searched as a group, since they refer to each other.
 *  - The executable is loaded at 0x400000 in three segments: read-only
 *    (headers and constant data), executable, and writable (with the TLS
 *    initialisation image at its start).
 *  - Every symbol referenced through the GOT gets a GOT entry holding its
 *    final address (or thread-pointer offset); general- and local-dynamic
 *    TLS sequences are rewritten to the local-exec form.
 *  - Sections that do not occupy memory are kept (concatenated by name)
 *    unless -s: debugging information then still works.  .eh_frame,
 *    .comment and .note sections are discarded, except the one that
 *    brands a program for V86BSD (.note.v86bsd.ident, from the C
 *    library's start file): it is loaded, and PT_NOTE names it, so the
 *    kernel runs the program by its own system call numbers.
 *  - A shared library (c17 -G) is position-independent (ET_DYN at 0).  Its
 *    functions bind to itself (as with ld's -Bsymbolic-functions), and so
 *    do symbols of hidden or internal visibility and those of c17's
 *    run-time support (LinkOptions.hide_from), which are not exported;
 *    its other variables are reached through the GOT and exported, so a
 *    program may copy them.  Absolute addresses in its data become
 *    R_X86_64_RELATIVE relocations, or R_X86_64_64 against a symbol bound
 *    elsewhere; GOT entries likewise (R_X86_64_GLOB_DAT); calls of
 *    functions it does not define go through PLT entries.  Undefined
 *    symbols are left to the dynamic linker.  Thread-local variables keep
 *    the general-dynamic model (a GOT pair: R_X86_64_DTPMOD64 and
 *    R_X86_64_DTPOFF64), so the library can be loaded by dlopen; the
 *    initial-exec model (R_X86_64_TPOFF64) is kept too.  An
 *    ENTRY(symbol) line in a linker script gives it an entry point (a C
 *    library that is also the dynamic linker).
 */
#include "c17.h"
#include "elf.h"

#define BASE_ADDR 0x400000
#define PAGE 0x1000

typedef struct ObjFile ObjFile;
typedef struct OutSec OutSec;

typedef struct {
  ObjFile *obj;
  char *name;
  uint32_t type;
  uint64_t flags;
  uint64_t size;
  uint64_t align;
  unsigned char *data;    /* NULL for SHT_NOBITS */
  unsigned char *rela;    /* its SHT_RELA section, if any */
  uint64_t nrela;
  OutSec *out;
  uint64_t offset;        /* in out */
  bool discard;
  int prio;               /* .init_array.N ordering */
} InSec;

typedef struct GSym GSym;
struct GSym {
  char *name;
  ObjFile *def;
  InSec *sec;             /* NULL: absolute or common or undefined */
  uint64_t value;
  int bind;
  int type;
  uint64_t size;
  bool defined;
  bool is_abs;
  bool common;
  uint64_t common_align;
  bool referenced;        /* by a non-weak undefined reference */
  bool weak_ref;
  ObjFile *first_ref;
  int got;                /* index of its GOT entry, or -1 */
  bool got_tp;            /* the GOT entry holds the TP offset */
  bool linker_defined;
  /* dynamic linking */
  struct DynLib *dso;     /* defined in this shared object (not in an object) */
  uint64_t dso_value;
  int dso_ver;            /* its version index there */
  bool dso_ref;           /* a shared object refers to it, or defines it too */
  bool imported;          /* bound at load time to a shared object's definition */
  int plt;                /* index of its PLT entry, or -1 */
  bool canonical;         /* the PLT entry is its address */
  bool copied;            /* copied into .dynbss */
  GSym *copy_of;          /* an alias of a copied symbol */
  uint64_t copy_off;      /* in .dynbss */
  int dynidx;             /* in .dynsym, or 0 */
  int vna;                /* the version needed (vernaux index), or 0 */
  int iplt;               /* an indirect function's PLT entry, or -1 */
  int vis;                /* the most constraining visibility seen (STV_*) */
  int gd;                 /* -G: its general-dynamic GOT pair, or -1 */
};

typedef struct DynLib {
  char *path;
  char *soname;
  unsigned char *buf;
  size_t len;
  unsigned char *dynsym;
  int ndynsym;
  char *dynstr;
  unsigned char *versym;
  char **vernames;        /* by version index */
  int nvernames;
  bool as_needed;
  bool needed;
  uint32_t soname_off;    /* in .dynstr */
} DynLib;

struct ObjFile {
  char *name;
  unsigned char *buf;
  size_t len;
  InSec *secs;
  int nsecs;
  unsigned char *symtab;
  int nsyms;
  int first_global;
  char *strtab;
  GSym **gsyms;           /* per symbol index; NULL for locals */
  bool included;
  bool parsed;
  bool hide;              /* -G: its definitions are not exported */
};

struct OutSec {
  char *name;
  uint32_t type;
  uint64_t flags;
  uint64_t align;
  uint64_t addr;
  uint64_t size;
  uint64_t fileoff;
  InSec **in;
  int nin;
  int index;
  int seg;                /* 0 r, 1 rx, 2 rw, -1 not allocated */
  unsigned char *data;    /* for synthesized sections (.got) */
  char *link_name;        /* the sections sh_link and sh_info refer to */
  char *info_name;
  uint64_t entsize;
};

static HashMap gsyms;
static GSym **gsym_list;
static int ngsyms;
static ObjFile **objs;     /* included objects, in order */
static int nobjs;
static OutSec **outs;
static int nouts;
static bool failed;
static GSym **got_syms;
static int ngot;
static GSym **gd_syms;       /* -G: GOT pairs after the GOT entries */
static int ngd;
static OutSec *got_sec;
static OutSec *bss_sec;
static uint64_t tls_start, tls_end, tls_align = 1, tls_filesz;
static bool has_tls;
static bool dynamic;
static DynLib **dlibs;
static int ndlibs;
static GSym **plt_syms, **copy_syms, **dyn_syms;
static int nplt, ncopy, ndyn;
static OutSec *plt_sec, *gotplt_sec, *dynbss_sec, *dynamic_sec, *dsohandle_sec;
static GSym **iplt_syms;
static int niplt;
static OutSec *iplt_sec, *igot_sec, *rela_iplt_sec;
static bool shared;          /* -G: a shared library */
static bool reading_hidden;  /* the input being read is c17's run-time support */
static uint64_t base_addr;
static char **script_end_syms;  /* NAME = . at the end of a linker script's SECTIONS */
static int nscript_end_syms;

static void lerror(char *fmt, ...) PRINTF(1, 2);
static void lerror(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "%s: ", progname);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  failed = true;
}

static unsigned char *slurp(char *path, size_t *len) {
  FILE *fp = fopen(path, "r");
  if (!fp) {
    lerror("cannot open %s: %s", path, strerror(errno));
    return NULL;
  }
  Buf b = {0};
  char tmp[65536];
  size_t n;
  while ((n = fread(tmp, 1, sizeof(tmp), fp)) > 0)
    buf_write(&b, tmp, n);
  if (ferror(fp)) {
    lerror("cannot read %s: %s", path, strerror(errno));
    fclose(fp);
    return NULL;
  }
  fclose(fp);
  *len = b.len;
  if (!b.buf)
    b.buf = xcalloc(1, 1);
  return (unsigned char *)b.buf;
}

static GSym *get_gsym(char *name) {
  GSym *s = hashmap_get(&gsyms, name);
  if (!s) {
    s = xcalloc(1, sizeof(GSym));
    s->name = name;
    s->got = -1;
    s->plt = -1;
    s->iplt = -1;
    s->gd = -1;
    hashmap_put(&gsyms, name, s);
    gsym_list = xrealloc(gsym_list, sizeof(GSym *) * (ngsyms + 1));
    gsym_list[ngsyms++] = s;
  }
  return s;
}

/* ------------------------------------------------- reading objects */

static uint64_t shdr_field(ObjFile *o, int i, int off, int size) {
  uint64_t shoff = rd64(o->buf + 40);
  unsigned char *sh = o->buf + shoff + (uint64_t)i * SHDR_SIZE + off;
  return size == 4 ? rd32(sh) : rd64(sh);
}

/* Checks the header and reads sections and the symbol table. */
static bool parse_object(ObjFile *o) {
  if (o->parsed)
    return true;
  o->parsed = true;
  unsigned char *h = o->buf;
  if (o->len < EHDR_SIZE || memcmp(h, "\177ELF", 4)) {
    lerror("%s: not an object file", o->name);
    return false;
  }
  if (h[4] != 2 || h[5] != 1 || rd16(h + 18) != EM_X86_64) {
    lerror("%s: not an x86-64 ELF-64 little-endian file", o->name);
    return false;
  }
  int type = rd16(h + 16);
  if (type == ET_DYN) {
    lerror("%s: a shared object in an archive", o->name);
    return false;
  }
  if (type != ET_REL) {
    lerror("%s: not a relocatable object file", o->name);
    return false;
  }
  uint64_t shoff = rd64(h + 40);
  int shnum = rd16(h + 60);
  if (shoff + (uint64_t)shnum * SHDR_SIZE > o->len) {
    lerror("%s: truncated object file", o->name);
    return false;
  }
  int shstrndx = rd16(h + 62);
  char *shstr = (char *)o->buf + shdr_field(o, shstrndx, 24, 8);
  o->nsecs = shnum;
  o->secs = xcalloc(shnum, sizeof(InSec));
  for (int i = 0; i < shnum; i++) {
    InSec *s = &o->secs[i];
    s->obj = o;
    s->name = shstr + shdr_field(o, i, 0, 4);
    s->type = shdr_field(o, i, 4, 4);
    s->flags = shdr_field(o, i, 8, 8);
    uint64_t off = shdr_field(o, i, 24, 8);
    s->size = shdr_field(o, i, 32, 8);
    s->align = shdr_field(o, i, 48, 8);
    if (!s->align)
      s->align = 1;
    if (s->type != SHT_NOBITS && s->type != SHT_NULL) {
      if (off + s->size > o->len) {
        lerror("%s: section %s extends past the end of the file", o->name, s->name);
        return false;
      }
      s->data = o->buf + off;
    }
    if (s->type == SHT_SYMTAB) {
      o->symtab = o->buf + off;
      o->nsyms = s->size / SYM_SIZE;
      o->first_global = shdr_field(o, i, 44, 4);
      int link = shdr_field(o, i, 40, 4);
      o->strtab = (char *)o->buf + shdr_field(o, link, 24, 8);
    }
  }
  for (int i = 0; i < shnum; i++) {
    InSec *s = &o->secs[i];
    if (s->type == SHT_RELA) {
      int target = shdr_field(o, i, 44, 4);
      if (target > 0 && target < shnum) {
        o->secs[target].rela = s->data;
        o->secs[target].nrela = s->size / RELA_SIZE;
      }
    }
    if (s->type == SHT_REL) {
      lerror("%s: REL relocations are not supported", o->name);
      return false;
    }
  }
  o->gsyms = xcalloc(o->nsyms ? o->nsyms : 1, sizeof(GSym *));
  return true;
}

static unsigned char *sym_ent(ObjFile *o, int i) { return o->symtab + (uint64_t)i * SYM_SIZE; }
static char *sym_name(ObjFile *o, int i) { return o->strtab + rd32(sym_ent(o, i)); }
static int sym_bind(ObjFile *o, int i) { return sym_ent(o, i)[4] >> 4; }
static int sym_type(ObjFile *o, int i) { return sym_ent(o, i)[4] & 15; }
static int sym_shndx(ObjFile *o, int i) { return rd16(sym_ent(o, i) + 6); }
static uint64_t sym_value(ObjFile *o, int i) { return rd64(sym_ent(o, i) + 8); }
static uint64_t sym_size(ObjFile *o, int i) { return rd64(sym_ent(o, i) + 16); }

/* Would including o define a symbol that is currently undefined? */
static bool defines_needed(ObjFile *o) {
  for (int i = o->first_global; i < o->nsyms; i++) {
    int shndx = sym_shndx(o, i);
    if (shndx == SHN_UNDEF || shndx == SHN_COMMON)
      continue;
    GSym *g = hashmap_get(&gsyms, sym_name(o, i));
    if (g && !g->defined && !g->common && !g->dso && g->referenced)
      return true;
  }
  return false;
}

/* Adds an object's global symbols to the global table. */
static void include_object(ObjFile *o) {
  o->included = true;
  objs = xrealloc(objs, sizeof(ObjFile *) * (nobjs + 1));
  objs[nobjs++] = o;

  /* COMDAT groups: keep the first group with a given signature */
  static HashMap groups;
  for (int i = 0; i < o->nsecs; i++) {
    InSec *s = &o->secs[i];
    if (s->type != SHT_GROUP)
      continue;
    int symi = shdr_field(o, i, 44, 4);
    char *sig = sym_name(o, symi);
    uint32_t *w = (uint32_t *)s->data;
    (void)w;
    bool dup = hashmap_get(&groups, sig) != NULL;
    if (!dup)
      hashmap_put(&groups, sig, o);
    if (dup)
      for (uint64_t k = 1; k < s->size / 4; k++) {
        uint32_t idx = rd32(s->data + 4 * k);
        if (idx < (uint32_t)o->nsecs)
          o->secs[idx].discard = true;
      }
    s->discard = true;
  }

  for (int i = o->first_global; i < o->nsyms; i++) {
    int bind = sym_bind(o, i);
    if (bind == STB_LOCAL)
      continue;
    char *name = sym_name(o, i);
    GSym *g = get_gsym(name);
    o->gsyms[i] = g;
    int shndx = sym_shndx(o, i);
    int type = sym_type(o, i);
    /* visibility: the most constraining (internal, hidden, protected) */
    static const int rank[4] = {0, 3, 2, 1};
    int vis = sym_ent(o, i)[5] & 3;
    if (o->hide && shndx != SHN_UNDEF)
      vis = STV_HIDDEN;
    if (rank[vis] > rank[g->vis])
      g->vis = vis;
    if (shndx == SHN_UNDEF) {
      if (bind == STB_WEAK)
        g->weak_ref = true;
      else if (!g->referenced) {
        g->referenced = true;
        g->first_ref = o;
      }
      continue;
    }
    if (shndx == SHN_COMMON) {
      if (g->defined)
        continue;
      uint64_t size = sym_size(o, i), al = sym_value(o, i);
      if (!g->common || size > g->size)
        g->size = size;
      if (!g->common || al > g->common_align)
        g->common_align = al;
      g->common = true;
      g->def = o;
      g->type = STT_OBJECT;
      if (g->dso)
        g->dso_ref = true;
      g->dso = NULL;
      continue;
    }
    InSec *sec = shndx == SHN_ABS ? NULL : shndx < o->nsecs ? &o->secs[shndx] : NULL;
    if (sec && sec->discard)
      continue;  /* in a duplicate group */
    if (g->defined) {
      if (bind == STB_WEAK || g->bind == STB_WEAK) {
        if (g->bind == STB_WEAK && bind != STB_WEAK)
          goto define;
        continue;
      }
      lerror("multiple definition of '%s' (in %s and %s)", name, g->def->name, o->name);
      continue;
    }
  define:
    if (g->dso)
      g->dso_ref = true;  /* it overrides the shared object's definition */
    g->dso = NULL;
    g->defined = true;
    g->common = false;
    g->def = o;
    g->sec = sec;
    g->is_abs = shndx == SHN_ABS;
    g->value = sym_value(o, i);
    g->bind = bind;
    g->type = type;
    g->size = sym_size(o, i);
  }
}

/* ------------------------------------------------------- archives */

typedef struct {
  char *name;
  ObjFile **members;
  int nmembers;
} Archive;

static Archive *read_archive(char *path, unsigned char *buf, size_t len) {
  Archive *ar = xcalloc(1, sizeof(Archive));
  ar->name = path;
  size_t pos = 8;
  char *longnames = NULL;
  size_t longlen = 0;
  while (pos + 60 <= len) {
    unsigned char *hdr = buf + pos;
    char name[17], sizestr[11];
    memcpy(name, hdr, 16);
    name[16] = '\0';
    memcpy(sizestr, hdr + 48, 10);
    sizestr[10] = '\0';
    size_t size = strtoul(sizestr, NULL, 10);
    size_t data = pos + 60;
    if (data + size > len) {
      lerror("%s: truncated archive", path);
      break;
    }
    char *mname;
    unsigned char *mdata = buf + data;
    size_t msize = size;
    if (!strncmp(name, "/ ", 2) || !strncmp(name, "/SYM64/", 7) ||
        !strncmp(name, "__.SYMDEF", 9)) {
      mname = NULL;  /* symbol index: not used */
    } else if (!strncmp(name, "// ", 3)) {
      longnames = (char *)mdata;
      longlen = size;
      mname = NULL;
    } else if (name[0] == '/' && isdigit((unsigned char)name[1])) {
      size_t off = strtoul(name + 1, NULL, 10);
      if (!longnames || off >= longlen) {
        lerror("%s: bad long member name", path);
        mname = "?";
      } else {
        char *e = longnames + off;
        size_t n = 0;
        while (off + n < longlen && e[n] != '\n' && !(e[n] == '/' && (off + n + 1 >= longlen || e[n + 1] == '\n')))
          n++;
        mname = xstrndup(e, n);
      }
    } else if (!strncmp(name, "#1/", 3)) {
      size_t n = strtoul(name + 3, NULL, 10);
      mname = xstrndup((char *)mdata, n);
      mdata += n;
      msize -= n;
    } else {
      int n = 16;
      while (n > 0 && (name[n - 1] == ' ' || name[n - 1] == '/'))
        n--;
      mname = xstrndup(name, n);
    }
    if (mname) {
      ObjFile *o = xcalloc(1, sizeof(ObjFile));
      o->hide = reading_hidden;
      o->name = format("%s(%s)", path, mname);
      o->buf = mdata;
      o->len = msize;
      ar->members = xrealloc(ar->members, sizeof(ObjFile *) * (ar->nmembers + 1));
      ar->members[ar->nmembers++] = o;
    }
    pos = data + size;
    if (pos & 1)
      pos++;
  }
  return ar;
}

/* Includes members that define needed symbols; returns whether any was. */
static bool search_archive(Archive *ar) {
  bool any = false;
  for (bool changed = true; changed;) {
    changed = false;
    for (int i = 0; i < ar->nmembers; i++) {
      ObjFile *o = ar->members[i];
      if (o->included)
        continue;
      if (!parse_object(o))
        continue;
      if (o->symtab && defines_needed(o)) {
        include_object(o);
        changed = any = true;
      }
    }
  }
  return any;
}

/* ------------------------------------------------- shared objects */

static uint64_t dso_shdr(DynLib *d, int i, int off, int size) {
  unsigned char *sh = d->buf + rd64(d->buf + 40) + (uint64_t)i * SHDR_SIZE + off;
  return size == 4 ? rd32(sh) : rd64(sh);
}

static char *base_name(char *path) {
  char *s = strrchr(path, '/');
  return s ? s + 1 : path;
}

/* Reads the dynamic symbols of a shared object; its definitions resolve
   symbols not defined by objects. */
static void read_dso(char *path, unsigned char *buf, size_t len, bool as_needed) {
  if (len < EHDR_SIZE || buf[4] != 2 || buf[5] != 1 || rd16(buf + 18) != EM_X86_64) {
    lerror("%s: not an x86-64 ELF-64 little-endian file", path);
    return;
  }
  if (!dynamic) {
    lerror("%s: a shared object, but the link is static", path);
    return;
  }
  DynLib *d = xcalloc(1, sizeof(DynLib));
  d->path = path;
  d->buf = buf;
  d->len = len;
  d->as_needed = as_needed;
  int shnum = rd16(buf + 60);
  if (rd64(buf + 40) + (uint64_t)shnum * SHDR_SIZE > len) {
    lerror("%s: truncated shared object", path);
    return;
  }
  unsigned char *verdef = NULL;
  int nverdef = 0;
  for (int i = 0; i < shnum; i++) {
    uint32_t type = dso_shdr(d, i, 4, 4);
    uint64_t off = dso_shdr(d, i, 24, 8), size = dso_shdr(d, i, 32, 8);
    if (off + size > len && type != SHT_NOBITS) {
      lerror("%s: section beyond the end of the file", path);
      return;
    }
    if (type == SHT_DYNSYM) {
      d->dynsym = buf + off;
      d->ndynsym = size / SYM_SIZE;
      d->dynstr = (char *)buf + dso_shdr(d, dso_shdr(d, i, 40, 4), 24, 8);
    } else if (type == SHT_GNU_VERSYM) {
      d->versym = buf + off;
    } else if (type == SHT_GNU_VERDEF) {
      verdef = buf + off;
      nverdef = dso_shdr(d, i, 44, 4);
    }
  }
  if (!d->dynsym) {
    lerror("%s: a shared object without dynamic symbols", path);
    return;
  }
  for (int i = 0; i < shnum; i++)
    if (dso_shdr(d, i, 4, 4) == SHT_DYNAMIC) {
      unsigned char *dyn = buf + dso_shdr(d, i, 24, 8);
      uint64_t n = dso_shdr(d, i, 32, 8) / 16;
      char *str = (char *)buf + dso_shdr(d, dso_shdr(d, i, 40, 4), 24, 8);
      for (uint64_t k = 0; k < n && rd64(dyn + 16 * k) != DT_NULL; k++)
        if (rd64(dyn + 16 * k) == DT_SONAME)
          d->soname = str + rd64(dyn + 16 * k + 8);
    }
  if (!d->soname)
    d->soname = base_name(path);
  for (int i = 0; i < ndlibs; i++)
    if (!strcmp(dlibs[i]->soname, d->soname))
      return;  /* the same library again */
  /* version definitions: the name of each index */
  for (int k = 0; verdef && k < nverdef; k++) {
    int ndx = rd16(verdef + 4), cnt = rd16(verdef + 6);
    uint32_t aux = rd32(verdef + 12), next = rd32(verdef + 16);
    if (cnt > 0 && !(rd16(verdef + 2) & 1)) {  /* not VER_FLG_BASE */
      if (ndx >= d->nvernames) {
        d->vernames = xrealloc(d->vernames, sizeof(char *) * (ndx + 1));
        for (int j = d->nvernames; j <= ndx; j++)
          d->vernames[j] = NULL;
        d->nvernames = ndx + 1;
      }
      d->vernames[ndx] = d->dynstr + rd32(verdef + aux);
    }
    if (!next)
      break;
    verdef += next;
  }
  dlibs = xrealloc(dlibs, sizeof(DynLib *) * (ndlibs + 1));
  dlibs[ndlibs++] = d;

  for (int i = 1; i < d->ndynsym; i++) {
    unsigned char *y = d->dynsym + (uint64_t)i * SYM_SIZE;
    int bind = y[4] >> 4, type = y[4] & 15, shndx = rd16(y + 6);
    if (bind == STB_LOCAL)
      continue;
    char *name = d->dynstr + rd32(y);
    if (shndx == SHN_UNDEF) {
      get_gsym(name)->dso_ref = true;
      continue;
    }
    int ver = d->versym ? rd16(d->versym + 2 * i) : 1;
    if (ver & 0x8000)
      continue;  /* a hidden (older) version */
    GSym *g = get_gsym(name);
    if (g->defined || g->common) {
      g->dso_ref = true;  /* interposed by the executable */
      continue;
    }
    if (g->dso)
      continue;  /* the first shared object wins */
    g->dso = d;
    g->dso_value = rd64(y + 8);
    g->size = rd64(y + 16);
    g->type = type;
    g->bind = bind;
    g->dso_ver = ver;
  }
}

static void add_input(LinkOptions *opts, char *path, char *name, bool group, bool as_needed,
                      int depth);
static Archive **group_ars;

/* SECTIONS in a linker script (a kernel's): the subset c17 understands.
   ". = ADDR;" before the first output section sets the address of the
   image (its ELF header); "NAME = .;" after the last one, perhaps after
   ". = ALIGN(N);", defines NAME at the (aligned) end of the image.  The
   output section descriptions themselves are not interpreted: sections
   are placed by c17's usual rules, so code must not rely on their order
   (but a ".boot" section comes first, right after the ELF headers, for a
   boot loader's header that must be near the start of the image). */
static bool script_has_base;
static uint64_t script_base;
static uint64_t *script_end_align;

static bool script_number(char **pp, uint64_t *v, bool *is_align) {
  char *p = *pp;
  while (isspace((unsigned char)*p))
    p++;
  if (!strncmp(p, "ALIGN", 5)) {
    p += 5;
    while (isspace((unsigned char)*p))
      p++;
    if (*p != '(')
      return false;
    p++;
    bool inner;
    if (!script_number(&p, v, &inner) || inner)
      return false;
    while (isspace((unsigned char)*p))
      p++;
    if (*p != ')')
      return false;
    *pp = p + 1;
    *is_align = true;
    return true;
  }
  *is_align = false;
  if (!isdigit((unsigned char)*p))
    return false;
  errno = 0;
  char *e;
  unsigned long long n = strtoull(p, &e, 0);
  if (errno)
    return false;
  if (*e == 'K' || *e == 'M') {
    n <<= *e == 'K' ? 10 : 20;
    e++;
  }
  *v = n;
  *pp = e;
  return true;
}

static void read_sections(char *path, char **pp) {
  char *p = *pp;
  bool seen_output = false;
  uint64_t align = 1;
  while (isspace((unsigned char)*p))
    p++;
  if (*p != '{') {
    lerror("%s: '{' expected after SECTIONS", path);
    return;
  }
  p++;
  for (;;) {
    while (isspace((unsigned char)*p) || *p == ';')
      p++;
    if (p[0] == '/' && p[1] == '*') {
      char *e = strstr(p + 2, "*/");
      p = e ? e + 2 : p + strlen(p);
      continue;
    }
    if (!*p) {
      lerror("%s: '}' missing after SECTIONS", path);
      return;
    }
    if (*p == '}') {
      *pp = p + 1;
      return;
    }
    char *start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != ':' && *p != ';' &&
           *p != '{')
      p++;
    char *word = xstrndup(start, p - start);
    while (isspace((unsigned char)*p))
      p++;
    if (*p == '=') {  /* an assignment */
      p++;
      while (isspace((unsigned char)*p))
        p++;
      uint64_t v = 0;
      bool is_align = false;
      bool dot = *p == '.' && !isalnum((unsigned char)p[1]) && p[1] != '_';
      if (dot)
        p++;
      else if (!script_number(&p, &v, &is_align)) {
        lerror("%s: SECTIONS: expression '%.20s' not supported", path, p);
        return;
      }
      if (!strcmp(word, ".")) {
        if (dot)
          ;
        else if (is_align)
          align = v;
        else if (!seen_output) {
          script_has_base = true;
          script_base = v;
        } else {
          lerror("%s: SECTIONS: setting '.' between output sections not supported", path);
          return;
        }
      } else if (dot && seen_output) {
        script_end_syms = xrealloc(script_end_syms, sizeof(char *) * (nscript_end_syms + 1));
        script_end_align = xrealloc(script_end_align, sizeof(uint64_t) * (nscript_end_syms + 1));
        script_end_syms[nscript_end_syms] = word;
        script_end_align[nscript_end_syms++] = align ? align : 1;
      } else {
        lerror("%s: SECTIONS: assignment to '%s' not supported", path, word);
        return;
      }
      continue;
    }
    /* an output section description: name [AT(...)] : ... { ... }; what
       /DISCARD/ names is left out anyway (notes, comments, unwind tables) */
    bool discard = !strcmp(word, "/DISCARD/");
    if (nscript_end_syms && !discard) {
      lerror("%s: SECTIONS: symbols may only be defined after the last output section", path);
      return;
    }
    if (!discard) {
      seen_output = true;
      align = 1;
    }
    while (*p && *p != '{')
      p++;
    int depth = 0;
    for (; *p; p++) {
      if (*p == '{')
        depth++;
      else if (*p == '}' && --depth == 0) {
        p++;
        break;
      }
    }
  }
}
static int ngroup_ars;

/* A linker script of the kind C libraries install as libc.so: GROUP (
   files ... AS_NEEDED ( files ) ), INPUT ( ... ), OUTPUT_FORMAT ( ... ) */
static void read_script(LinkOptions *opts, char *path, char *text, bool group, int depth) {
  char *p = text;
  int as_needed_depth = -1, depth_paren = 0;
  for (;;) {
    while (*p && (isspace((unsigned char)*p) || *p == ','))
      p++;
    if (!*p)
      break;
    if (p[0] == '/' && p[1] == '*') {
      char *e = strstr(p + 2, "*/");
      p = e ? e + 2 : p + strlen(p);
      continue;
    }
    if (*p == '(') {
      depth_paren++;
      p++;
      continue;
    }
    if (*p == ')') {
      if (depth_paren-- == as_needed_depth)
        as_needed_depth = -1;
      p++;
      continue;
    }
    char *start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != ')' && *p != ',')
      p++;
    char *word = xstrndup(start, p - start);
    if (!strcmp(word, "GROUP") || !strcmp(word, "INPUT"))
      continue;
    if (!strcmp(word, "AS_NEEDED")) {
      as_needed_depth = depth_paren + 1;
      continue;
    }
    if (!strcmp(word, "ENTRY")) {
      /* ENTRY ( symbol ): the entry point */
      while (*p && (isspace((unsigned char)*p) || *p == '('))
        p++;
      char *e = p;
      while (*p && !isspace((unsigned char)*p) && *p != ')')
        p++;
      opts->entry = xstrndup(e, p - e);
      while (*p && *p != ')')
        p++;
      if (*p)
        p++;
      continue;
    }
    if (!strcmp(word, "SECTIONS") && depth_paren == 0) {
      read_sections(path, &p);
      continue;
    }
    if (!strcmp(word, "OUTPUT_FORMAT") || !strcmp(word, "OUTPUT_ARCH") ||
        !strcmp(word, "SEARCH_DIR")) {
      /* skip its parenthesised argument */
      while (*p && *p != ')')
        p++;
      if (*p)
        p++;
      continue;
    }
    if (depth_paren == 0) {
      lerror("%s: linker script command '%s' not supported", path, word);
      return;
    }
    char *file = word;
    if (word[0] != '/') {
      if (startswith(word, "-l")) {
        lerror("%s: -l in a linker script not supported", path);
        return;
      }
      /* relative to the script's directory */
      char *slash = strrchr(path, '/');
      if (slash)
        file = format("%.*s/%s", (int)(slash - path), path, word);
    }
    add_input(opts, file, file, group, as_needed_depth >= 0, depth + 1);
  }
}

/* Reads one input: an object, an archive, a shared object or a script */
static void add_input(LinkOptions *opts, char *path, char *name, bool group, bool as_needed,
                      int depth) {
  size_t len;
  unsigned char *buf = slurp(path, &len);
  if (!buf)
    return;
  if (len >= 8 && !memcmp(buf, "!<arch>\n", 8)) {
    Archive *ar = read_archive(path, buf, len);
    search_archive(ar);
    if (group) {
      group_ars = xrealloc(group_ars, sizeof(Archive *) * (ngroup_ars + 1));
      group_ars[ngroup_ars++] = ar;
    }
    return;
  }
  if (len >= 18 && !memcmp(buf, "\177ELF", 4)) {
    if (rd16(buf + 16) == ET_DYN) {
      read_dso(path, buf, len, as_needed);
      return;
    }
    ObjFile *o = xcalloc(1, sizeof(ObjFile));
    o->hide = reading_hidden;
    o->name = name;
    o->buf = buf;
    o->len = len;
    if (parse_object(o))
      include_object(o);
    return;
  }
  /* a linker script: text */
  bool text = len > 0;
  for (size_t i = 0; i < len; i++)
    if (!buf[i] || (buf[i] < 32 && !isspace(buf[i])))
      text = false;
  if (text && depth < 8) {
    char *t = xstrndup((char *)buf, len);
    if (strstr(t, "GROUP") || strstr(t, "INPUT") || strstr(t, "ENTRY") ||
        strstr(t, "SECTIONS")) {
      read_script(opts, path, t, group, depth);
      return;
    }
  }
  lerror("%s: not an object file or archive", name);
}

/* --------------------------------------------------------- layout */

/* Output section name for an input section, or NULL to discard it. */
static char *out_name(InSec *s, int *prio) {
  char *n = s->name;
  *prio = 0;
  if (s->type == SHT_NULL || s->type == SHT_SYMTAB || s->type == SHT_STRTAB ||
      s->type == SHT_RELA || s->type == SHT_GROUP || s->type == SHT_X86_64_UNWIND)
    return NULL;
  if (!strcmp(n, ".eh_frame") || !strcmp(n, ".comment") || (startswith(n, ".note") && strcmp(n, ".note.v86bsd.ident")) ||
      startswith(n, ".gnu.") || !strcmp(n, ".llvm_addrsig"))
    return NULL;
  static char *prefixes[] = {".text", ".rodata", ".data.rel.ro", ".data", ".bss", ".tdata",
                             ".tbss", NULL};
  static char *targets[] = {".text", ".rodata", ".data", ".data", ".bss", ".tdata", ".tbss"};
  for (int i = 0; prefixes[i]; i++) {
    size_t l = strlen(prefixes[i]);
    if (!strncmp(n, prefixes[i], l) && (n[l] == '\0' || n[l] == '.'))
      return targets[i];
  }
  if (!strcmp(n, ".gcc_except_table") || startswith(n, ".gcc_except_table."))
    return ".rodata";
  static char *arrays[] = {".init_array", ".fini_array", ".preinit_array", NULL};
  for (int i = 0; arrays[i]; i++) {
    size_t l = strlen(arrays[i]);
    if (!strncmp(n, arrays[i], l) && (n[l] == '\0' || n[l] == '.')) {
      *prio = n[l] == '.' ? atoi(n + l + 1) : 65536;
      return arrays[i];
    }
  }
  if (!strcmp(n, ".ctors") || !strcmp(n, ".dtors") || startswith(n, ".ctors.") ||
      startswith(n, ".dtors.")) {
    if (s->size)
      lerror("%s: .ctors/.dtors sections are not supported", s->obj->name);
    return NULL;
  }
  return n;
}

static OutSec *get_out(char *name, InSec *s) {
  for (int i = 0; i < nouts; i++)
    if (!strcmp(outs[i]->name, name))
      return outs[i];
  OutSec *o = xcalloc(1, sizeof(OutSec));
  o->name = name;
  o->type = s ? s->type : SHT_PROGBITS;
  o->flags = s ? s->flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR | SHF_TLS) : 0;
  o->align = 1;
  if (!(o->flags & SHF_ALLOC))
    o->seg = -1;
  else if (o->flags & SHF_WRITE)
    o->seg = 2;
  else if (o->flags & SHF_EXECINSTR)
    o->seg = 1;
  else
    o->seg = 0;
  outs = xrealloc(outs, sizeof(OutSec *) * (nouts + 1));
  outs[nouts++] = o;
  return o;
}

static int cmp_prio(const void *a, const void *b) {
  InSec *x = *(InSec **)a, *y = *(InSec **)b;
  if (x->prio != y->prio)
    return x->prio < y->prio ? -1 : 1;
  /* stable: by original position (pointer order within and across objects
     is not meaningful, so compare object order and section index) */
  if (x->obj != y->obj) {
    for (int i = 0; i < nobjs; i++) {
      if (objs[i] == x->obj)
        return -1;
      if (objs[i] == y->obj)
        return 1;
    }
  }
  return x < y ? -1 : x > y ? 1 : 0;
}

/* Rank of an output section within its segment */
static int sec_rank(OutSec *o) {
  static char *order[] = {".boot", ".interp", ".hash", ".dynsym", ".dynstr", ".gnu.version",
                          ".gnu.version_r", ".rela.dyn", ".rela.plt", ".rela.iplt", ".rodata",
                          ".init", ".plt", ".iplt", ".text", ".fini", ".tdata", "(tls)",
                          ".tbss", ".preinit_array", ".init_array", ".fini_array", ".dynamic",
                          ".data", "(data)", ".dso_handle", ".got", ".got.plt", ".igot",
                          ".dynbss", ".bss", "(bss)", NULL};
  char *name = o->name;
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; order[i]; i++)
      if (!strcmp(name, order[i]))
        return i;
    /* others: thread-local data, data, zeroed data */
    name = (o->flags & SHF_TLS) ? (o->type == SHT_NOBITS ? ".tbss" : "(tls)")
           : o->type == SHT_NOBITS ? "(bss)" : "(data)";
  }
  return 0;
}

static int cmp_out(const void *a, const void *b) {
  OutSec *x = *(OutSec **)a, *y = *(OutSec **)b;
  int sx = x->seg < 0 ? 99 : x->seg, sy = y->seg < 0 ? 99 : y->seg;
  if (sx != sy)
    return sx < sy ? -1 : 1;
  int rx = sec_rank(x), ry = sec_rank(y);
  if (rx != ry)
    return rx < ry ? -1 : 1;
  return strcmp(x->name, y->name);
}

static uint64_t align_up(uint64_t v, uint64_t a) { return a ? (v + a - 1) / a * a : v; }

/* -------------------------------------------------- symbol values */

static uint64_t sec_addr(InSec *s) { return s->out ? s->out->addr + s->offset : 0; }

static uint64_t gsym_addr(GSym *g) {
  if (g->iplt >= 0)
    return iplt_sec->addr + 16 * (uint64_t)g->iplt;  /* the canonical address */
  if (g->copy_of)
    return gsym_addr(g->copy_of);
  if (g->copied)
    return dynbss_sec->addr + g->copy_off;
  if (g->imported)
    return g->plt >= 0 ? plt_sec->addr + 16 * (uint64_t)g->plt : 0;
  if (!g->defined && !g->common)
    return 0;  /* undefined weak */
  if (g->is_abs)
    return g->value;
  if (g->sec)
    return sec_addr(g->sec) + g->value;
  return g->value;  /* linker-defined, or common (assigned in .bss) */
}

/* Address of the symbol referred to by relocation entry symbol i of o.
   Sets *g to the global symbol, if any. */
static uint64_t local_sym_addr(ObjFile *o, int i, GSym **g) {
  *g = o->gsyms[i];
  if (*g)
    return gsym_addr(*g);
  int shndx = sym_shndx(o, i);
  if (shndx == SHN_ABS)
    return sym_value(o, i);
  if (shndx == SHN_UNDEF || shndx >= o->nsecs)
    return 0;
  return sec_addr(&o->secs[shndx]) + sym_value(o, i);
}

/* Offset from the thread pointer (variant II: TLS block below it) */
static int64_t tpoff(uint64_t addr) {
  return (int64_t)(addr - align_up(tls_end, tls_align));
}

/* --------------------------------------------------------- the GOT */

/* GOT entries are keyed by global symbol; locals referenced through the
   GOT (rare) get a private symbol. */
static GSym *got_key(ObjFile *o, int symi) {
  GSym *g = o->gsyms[symi];
  if (g)
    return g;
  char *name = format("\001%p.%d", (void *)o, symi);
  g = get_gsym(name);
  if (!g->defined) {
    int shndx = sym_shndx(o, symi);
    g->defined = true;
    g->def = o;
    g->bind = STB_LOCAL;
    g->type = sym_type(o, symi);
    if (shndx == SHN_ABS) {
      g->is_abs = true;
    } else if (shndx > 0 && shndx < o->nsecs) {
      g->sec = &o->secs[shndx];
    }
    g->value = sym_value(o, symi);
  }
  return g;
}

static void scan_got(void) {
  for (int k = 0; k < nobjs; k++) {
    ObjFile *o = objs[k];
    for (int i = 0; i < o->nsecs; i++) {
      InSec *s = &o->secs[i];
      if (!s->out || !s->rela)
        continue;
      for (uint64_t r = 0; r < s->nrela; r++) {
        unsigned char *re = s->rela + r * RELA_SIZE;
        uint64_t info = rd64(re + 8);
        int type = (uint32_t)info;
        int symi = info >> 32;
        if (shared && type == R_X86_64_TLSGD) {
          GSym *g = got_key(o, symi);
          if (g->gd < 0) {
            g->gd = ngd++;
            gd_syms = xrealloc(gd_syms, sizeof(GSym *) * ngd);
            gd_syms[ngd - 1] = g;
          }
          continue;
        }
        bool gd = type == R_X86_64_TLSGD && o->gsyms[symi] && o->gsyms[symi]->imported;
        if (type != R_X86_64_GOTPCREL && type != R_X86_64_GOTPCRELX &&
            type != R_X86_64_REX_GOTPCRELX && type != R_X86_64_GOTTPOFF &&
            type != R_X86_64_GOT32 && !gd)
          continue;
        GSym *g = got_key(o, symi);
        if (g->got >= 0)
          continue;
        g->got = ngot;
        g->got_tp = type == R_X86_64_GOTTPOFF || gd;
        got_syms = xrealloc(got_syms, sizeof(GSym *) * (ngot + 1));
        got_syms[ngot++] = g;
      }
    }
  }
}

/* ---------------------------------------------- indirect functions */

/* An indirect function (STT_GNU_IFUNC, as in glibc's libc.a) is reached
   through a PLT entry whose GOT slot the C library's start-up code fills
   by calling the resolver: an R_X86_64_IRELATIVE relocation between
   __rela_iplt_start and __rela_iplt_end. */
static unsigned char *put_rela(unsigned char *p, uint64_t off, int type, int sym, int64_t add);
static OutSec *synth(char *name, uint32_t type, uint64_t flags, uint64_t align, uint64_t size);

static void scan_ifuncs(void) {
  for (int k = 0; k < nobjs; k++) {
    ObjFile *o = objs[k];
    for (int i = 0; i < o->nsecs; i++) {
      InSec *s = &o->secs[i];
      if (!s->out || !s->rela)
        continue;
      for (uint64_t r = 0; r < s->nrela; r++) {
        unsigned char *re = s->rela + r * RELA_SIZE;
        int symi = rd64(re + 8) >> 32;
        GSym *g = symi ? o->gsyms[symi] : NULL;
        if (!g && symi && sym_type(o, symi) == STT_GNU_IFUNC) {
          lerror("%s: a local indirect function '%s'", o->name, sym_name(o, symi));
          continue;
        }
        if (!g || g->type != STT_GNU_IFUNC || !g->defined || g->iplt >= 0)
          continue;
        if (dynamic) {
          lerror("%s: the indirect function '%s' in a dynamic executable", o->name, g->name);
          continue;
        }
        g->iplt = niplt++;
        iplt_syms = xrealloc(iplt_syms, sizeof(GSym *) * niplt);
        iplt_syms[niplt - 1] = g;
      }
    }
  }
  if (!niplt)
    return;
  iplt_sec = synth(".iplt", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 16, 16 * (uint64_t)niplt);
  igot_sec = synth(".igot", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8, 8 * (uint64_t)niplt);
  rela_iplt_sec = synth(".rela.iplt", SHT_RELA, SHF_ALLOC, 8, (uint64_t)RELA_SIZE * niplt);
  rela_iplt_sec->entsize = RELA_SIZE;
}

static void fill_ifuncs(void) {
  for (int i = 0; i < niplt; i++) {
    GSym *g = iplt_syms[i];
    uint64_t resolver = (g->sec ? sec_addr(g->sec) : 0) + g->value;
    uint64_t slot = igot_sec->addr + 8 * (uint64_t)i, at = iplt_sec->addr + 16 * (uint64_t)i;
    unsigned char *e = iplt_sec->data + 16 * i;
    e[0] = 0xff;
    e[1] = 0x25;
    wr32(e + 2, (uint32_t)(slot - (at + 6)));
    memset(e + 6, 0xcc, 10);
    wr64(igot_sec->data + 8 * i, resolver);
    put_rela(rela_iplt_sec->data + RELA_SIZE * i, slot, R_X86_64_IRELATIVE, 0, (int64_t)resolver);
  }
}

/* ------------------------------------------------ dynamic linking */

static bool is_func_sym(GSym *g) {
  return g->type == STT_FUNC || g->type == STT_GNU_IFUNC ||
         (g->type == STT_NOTYPE && g->size == 0);
}

/* Symbols the link defines (with the addresses of the output) */
static char *linker_syms[] = {"__init_array_start", "__init_array_end",
                              "__fini_array_start", "__fini_array_end",
                              "__preinit_array_start", "__preinit_array_end",
                              "_etext", "etext", "_edata", "edata", "_end", "end",
                              "__bss_start", "__ehdr_start", "__executable_start",
                              "_GLOBAL_OFFSET_TABLE_", "_DYNAMIC", "__dso_handle",
                              "__rela_iplt_start", "__rela_iplt_end", NULL};

static bool is_linker_name(char *name) {
  for (int k = 0; linker_syms[k]; k++)
    if (!strcmp(name, linker_syms[k]))
      return true;
  for (int k = 0; k < nscript_end_syms; k++)
    if (!strcmp(name, script_end_syms[k]))
      return true;
  return startswith(name, "__start_") || startswith(name, "__stop_");
}

/* Does a reference to g bind to the output itself?  In a shared library:
   calls of its functions (call), its symbols of other than default
   visibility (hidden, internal, protected), c17's run-time support (made
   hidden), and the symbols the link defines; not its other variables,
   which a program may copy, nor the addresses of its functions, which a
   program may have made canonical (a PLT entry), nor what it does not
   define. */
static bool binds_to_self(GSym *g, bool call) {
  if (g->imported)
    return false;
  if (!g->defined && !g->common)
    return is_linker_name(g->name) || !shared;
  if (!shared)
    return true;
  return g->vis != STV_DEFAULT || g->linker_defined || g->bind == STB_LOCAL ||
         (call && is_func_sym(g));
}

static bool binds_locally(GSym *g) { return binds_to_self(g, false); }

/* An address that needs no relocation: an absolute symbol */
static bool is_constant(ObjFile *o, int symi, GSym *g) {
  if (g)
    return g->defined && g->is_abs && !g->linker_defined && !is_linker_name(g->name);
  return symi && sym_shndx(o, symi) == SHN_ABS;
}

/* -G: the dynamic relocations of the data (R_X86_64_64): relative, or
   against a symbol bound elsewhere */
typedef struct {
  InSec *s;
  uint64_t r;       /* index in s->rela */
  GSym *sym;        /* NULL: relative */
} DataRel;
static DataRel *data_rels;
static int ndata_rels, nrelative;

static void scan_shared(void) {
  data_rels = NULL;
  ndata_rels = nrelative = 0;
  for (int k = 0; k < nobjs; k++) {
    ObjFile *o = objs[k];
    for (int i = 0; i < o->nsecs; i++) {
      InSec *s = &o->secs[i];
      if (!s->out || !s->rela || !(s->flags & SHF_ALLOC))
        continue;
      for (uint64_t r = 0; r < s->nrela; r++) {
        unsigned char *re = s->rela + r * RELA_SIZE;
        uint64_t info = rd64(re + 8);
        int type = (uint32_t)info, symi = info >> 32;
        GSym *g = symi ? o->gsyms[symi] : NULL;
        char *sname = symi ? sym_name(o, symi) : "";
        bool local = !g || binds_to_self(g, type != R_X86_64_64);
        switch (type) {
        case R_X86_64_64:
          if (is_constant(o, symi, g))
            break;
          data_rels = xrealloc(data_rels, sizeof(DataRel) * (ndata_rels + 1));
          data_rels[ndata_rels++] = (DataRel){s, r, local ? NULL : g};
          if (local)
            nrelative++;
          break;
        case R_X86_64_32: case R_X86_64_32S: case R_X86_64_16: case R_X86_64_8:
          if (!is_constant(o, symi, g))
            lerror("%s: an absolute address of '%s' in %s: not in a shared library "
                   "(compile it with -G)", o->name, *sname ? sname : "(section)", s->name);
          break;
        case R_X86_64_PC32: case R_X86_64_PC64: case R_X86_64_PC16: case R_X86_64_PC8:
          if (!local)
            lerror("%s: '%s' used directly (not through the GOT) in %s: not in a shared "
                   "library (compile it with -G)", o->name, sname, s->name);
          break;
        case R_X86_64_TPOFF32: case R_X86_64_TPOFF64: case R_X86_64_DTPOFF32:
        case R_X86_64_DTPOFF64: case R_X86_64_TLSLD:
          lerror("%s: the thread-local variable '%s' in a model a shared library cannot use "
                 "(compile it with -G)", o->name, sname);
          break;
        }
      }
    }
  }
}

/* Does the GOT entry of g get the value of a symbol from the dynamic
   linker? */
static bool got_dynamic(GSym *g) {
  if (shared)
    return !binds_locally(g);
  return g->imported && !g->copied && !g->copy_of && !g->canonical;
}

/* -G: does the GOT entry of g need a dynamic relocation at all (all but
   absolute values)? */
static bool got_reloc(GSym *g) {
  return got_dynamic(g) || g->got_tp || !(g->is_abs && !g->linker_defined);
}

/* Decides how each imported symbol is reached: a PLT entry for
   functions, a copy for variables. */
static void scan_imports(void) {
  for (int i = 0; i < ngsyms; i++) {
    GSym *g = gsym_list[i];
    if (g->dso && !g->defined && !g->common && (g->referenced || g->weak_ref)) {
      g->imported = true;
      g->dso->needed = true;
    } else if (shared && !g->dso && !g->defined && !g->common &&
               (g->referenced || g->weak_ref) && !is_linker_name(g->name) &&
               g->name[0] != '\001') {
      g->imported = true;  /* left to the dynamic linker */
    }
  }
  for (int k = 0; k < nobjs; k++) {
    ObjFile *o = objs[k];
    for (int i = 0; i < o->nsecs; i++) {
      InSec *s = &o->secs[i];
      if (!s->out || !s->rela || !(s->flags & SHF_ALLOC))
        continue;
      for (uint64_t r = 0; r < s->nrela; r++) {
        unsigned char *re = s->rela + r * RELA_SIZE;
        uint64_t info = rd64(re + 8);
        int type = (uint32_t)info;
        GSym *g = o->gsyms[info >> 32];
        if (!g || !g->imported)
          continue;
        switch (type) {
        case R_X86_64_PLT32: case R_X86_64_PC32: case R_X86_64_PC64: case R_X86_64_64:
        case R_X86_64_32: case R_X86_64_32S: case R_X86_64_16: case R_X86_64_8:
        case R_X86_64_PC16: case R_X86_64_PC8:
          if (g->type == STT_TLS) {
            lerror("%s: the thread-local variable '%s' of %s used as an address", o->name,
                   g->name, g->dso ? g->dso->soname : "another object");
          } else if (shared) {
            /* calls through the PLT; addresses in data: R_X86_64_64 */
            if (type == R_X86_64_PLT32 && g->plt < 0) {
              g->plt = nplt++;
              plt_syms = xrealloc(plt_syms, sizeof(GSym *) * nplt);
              plt_syms[nplt - 1] = g;
            }
          } else if (is_func_sym(g)) {
            if (g->plt < 0) {
              g->plt = nplt++;
              plt_syms = xrealloc(plt_syms, sizeof(GSym *) * nplt);
              plt_syms[nplt - 1] = g;
            }
            if (type != R_X86_64_PLT32)
              g->canonical = true;  /* its address is taken */
          } else if (!g->copied) {
            g->copied = true;
            copy_syms = xrealloc(copy_syms, sizeof(GSym *) * (ncopy + 1));
            copy_syms[ncopy++] = g;
          }
          break;
        case R_X86_64_TPOFF32: case R_X86_64_TPOFF64:
          if (!shared)  /* (scan_shared reports it) */
            lerror("%s: the thread-local variable '%s' of %s with the local-exec model "
                   "(compile it with the initial-exec model)", o->name, g->name,
                   g->dso ? g->dso->soname : "another object");
          break;
        }
      }
    }
  }
  /* the aliases of copied variables (environ, __environ) move with them */
  int nprimary = ncopy;
  if (shared)
    return;
  for (int c = 0; c < nprimary; c++) {
    GSym *g = copy_syms[c];
    DynLib *d = g->dso;
    for (int i = 1; i < d->ndynsym; i++) {
      unsigned char *y = d->dynsym + (uint64_t)i * SYM_SIZE;
      if (rd16(y + 6) == SHN_UNDEF || rd64(y + 8) != g->dso_value || (y[4] >> 4) == STB_LOCAL)
        continue;
      if (d->versym && (rd16(d->versym + 2 * i) & 0x8000))
        continue;
      GSym *a = hashmap_get(&gsyms, d->dynstr + rd32(y));
      if (!a || a == g || a->dso != d || a->defined || a->common || a->copied || a->copy_of)
        continue;
      a->copy_of = g;
      a->imported = false;
    }
  }
}

static Buf dynstr;
static HashMap dynstr_map;

static uint32_t dynstr_add(char *s) {
  void *v = hashmap_get(&dynstr_map, s);
  if (v)
    return (uint32_t)(uintptr_t)v - 1;
  uint32_t off = dynstr.len;
  buf_write(&dynstr, s, strlen(s) + 1);
  hashmap_put(&dynstr_map, s, (void *)(uintptr_t)(off + 1));
  return off;
}

static uint32_t elf_hash(const char *name) {
  uint32_t h = 0;
  for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
    h = (h << 4) + *p;
    uint32_t g = h & 0xf0000000;
    if (g)
      h ^= g >> 24;
    h &= ~g;
  }
  return h;
}

/* versions needed: for each library, the version names bound */
typedef struct {
  DynLib *lib;
  char *name;
  int index;
} VerNeed;
static VerNeed *verneeds;
static int nverneeds;
static int nverneed_libs;

static OutSec *synth(char *name, uint32_t type, uint64_t flags, uint64_t align, uint64_t size) {
  OutSec *o = xcalloc(1, sizeof(OutSec));
  o->name = name;
  o->type = type;
  o->flags = flags;
  o->align = align;
  o->size = size;
  o->seg = !(flags & SHF_ALLOC) ? -1 : (flags & SHF_WRITE) ? 2 : (flags & SHF_EXECINSTR) ? 1 : 0;
  o->nin = -1;  /* marks a synthesized section */
  outs = xrealloc(outs, sizeof(OutSec *) * (nouts + 1));
  outs[nouts++] = o;
  return o;
}

static int out_index(OutSec *o) {
  for (int i = 0; i < nouts; i++)
    if (outs[i] == o)
      return i + 1;
  return 0;
}

int out_index_by_name(char *name);
int out_index_by_name(char *name) {
  if (!name)
    return 0;
  for (int i = 0; i < nouts; i++)
    if (!strcmp(outs[i]->name, name))
      return i + 1;
  return 0;
}

static OutSec *find_out(char *name);

/* The sections of a dynamic executable, sized; filled by fill_dynamic */
static int ndynamic_entries;
static int nrela_dyn;
static uint32_t runpath_off, soname_off;

static void make_dynamic_sections(LinkOptions *opts) {
  dynstr = (Buf){0};
  dynstr_map = (HashMap){0};
  verneeds = NULL;
  nverneeds = nverneed_libs = 0;
  buf_putc(&dynstr, 0);
  for (int i = 0; i < ndlibs; i++)
    if (dlibs[i]->needed || !dlibs[i]->as_needed) {
      dlibs[i]->needed = true;
      dlibs[i]->soname_off = dynstr_add(dlibs[i]->soname);
    }
  if (opts->nrpath) {
    Buf rp = {0};
    for (int i = 0; i < opts->nrpath; i++) {
      if (i)
        buf_putc(&rp, ':');
      buf_write(&rp, opts->rpath[i], strlen(opts->rpath[i]));
    }
    buf_putc(&rp, 0);
    runpath_off = dynstr_add(rp.buf);
  }
  /* the dynamic symbols */
  for (int i = 0; i < ngsyms; i++) {
    GSym *g = gsym_list[i];
    bool exported = (g->dso_ref || shared) && (g->defined || g->common) &&
                    g->bind != STB_LOCAL && g->name[0] != '\001' && !g->imported &&
                    !g->linker_defined && (g->vis == STV_DEFAULT || g->vis == STV_PROTECTED);
    if (!g->imported && !g->copied && !g->copy_of && !exported)
      continue;
    dyn_syms = xrealloc(dyn_syms, sizeof(GSym *) * (ndyn + 1));
    dyn_syms[ndyn++] = g;
    g->dynidx = ndyn;
    dynstr_add(g->name);
    if (g->imported && g->dso_ver >= 2 && g->dso_ver < g->dso->nvernames &&
        g->dso->vernames[g->dso_ver]) {
      char *vn = g->dso->vernames[g->dso_ver];
      int k;
      for (k = 0; k < nverneeds; k++)
        if (verneeds[k].lib == g->dso && !strcmp(verneeds[k].name, vn))
          break;
      if (k == nverneeds) {
        verneeds = xrealloc(verneeds, sizeof(VerNeed) * (nverneeds + 1));
        verneeds[k].lib = g->dso;
        verneeds[k].name = vn;
        verneeds[k].index = nverneeds + 2;
        nverneeds++;
        dynstr_add(vn);
      }
      g->vna = verneeds[k].index;
    }
  }
  for (int i = 0; i < ndlibs; i++)
    for (int k = 0; k < nverneeds; k++)
      if (verneeds[k].lib == dlibs[i]) {
        nverneed_libs++;
        break;
      }
  /* GOT entries filled at load time; -G: also the data's relocations */
  nrela_dyn = ncopy + ndata_rels + 2 * ngd;
  for (int i = 0; i < ngot; i++)
    if (shared ? got_reloc(got_syms[i]) : got_dynamic(got_syms[i]))
      nrela_dyn++;

  int nbucket = ndyn / 2 + 1;
  OutSec *o;
  if (!shared)
    synth(".interp", SHT_PROGBITS, SHF_ALLOC, 1, strlen(opts->interp) + 1);
  else
    soname_off = dynstr_add(opts->soname);
  o = synth(".hash", SHT_HASH, SHF_ALLOC, 8, 4 * (2 + nbucket + ndyn + 1));
  o->link_name = ".dynsym";
  o->entsize = 4;
  o = synth(".dynsym", SHT_DYNSYM, SHF_ALLOC, 8, (uint64_t)SYM_SIZE * (ndyn + 1));
  o->link_name = ".dynstr";
  o->entsize = SYM_SIZE;
  o = synth(".dynstr", SHT_STRTAB, SHF_ALLOC, 1, dynstr.len);
  if (nverneeds) {
    o = synth(".gnu.version", SHT_GNU_VERSYM, SHF_ALLOC, 2, 2 * (uint64_t)(ndyn + 1));
    o->link_name = ".dynsym";
    o->entsize = 2;
    o = synth(".gnu.version_r", SHT_GNU_VERNEED, SHF_ALLOC, 8,
              16 * (uint64_t)nverneed_libs + 16 * (uint64_t)nverneeds);
    o->link_name = ".dynstr";
  }
  if (nrela_dyn) {
    o = synth(".rela.dyn", SHT_RELA, SHF_ALLOC, 8, (uint64_t)RELA_SIZE * nrela_dyn);
    o->link_name = ".dynsym";
    o->entsize = RELA_SIZE;
  }
  if (nplt) {
    o = synth(".rela.plt", SHT_RELA, SHF_ALLOC | SHF_INFO_LINK, 8, (uint64_t)RELA_SIZE * nplt);
    o->link_name = ".dynsym";
    o->info_name = ".got.plt";
    o->entsize = RELA_SIZE;
    plt_sec = synth(".plt", SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 16, 16 * (uint64_t)nplt);
    plt_sec->entsize = 16;
  }
  gotplt_sec = synth(".got.plt", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8, 8 * (uint64_t)(3 + nplt));
  gotplt_sec->entsize = 8;
  if (ncopy) {
    uint64_t off = 0, maxal = 1;
    for (int i = 0; i < ncopy; i++) {
      GSym *g = copy_syms[i];
      uint64_t al = g->dso_value & -g->dso_value;
      if (!al || al > 64)
        al = 64;
      if (al > maxal)
        maxal = al;
      off = align_up(off, al);
      g->copy_off = off;
      off += g->size ? g->size : 8;
    }
    dynbss_sec = synth(".dynbss", SHT_NOBITS, SHF_ALLOC | SHF_WRITE, maxal, off);
  }
  /* the entries of .dynamic */
  int n = 0;
  for (int i = 0; i < ndlibs; i++)
    n += dlibs[i]->needed;
  n += opts->nrpath ? 1 : 0;
  n += 2;  /* DT_INIT, DT_FINI (if defined) */
  n += 6;  /* the arrays */
  n += 5;  /* hash, strtab, symtab, strsz, syment */
  n += 1;  /* debug, or (-G) soname */
  n += nplt ? 4 : 0;
  n += nrela_dyn ? 3 : 0;
  n += nverneeds ? 3 : 0;
  n += 3;  /* flags, flags_1, null */
  ndynamic_entries = n;
  dynamic_sec = synth(".dynamic", SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, 8, 16 * (uint64_t)n);
  dynamic_sec->link_name = ".dynstr";
  dynamic_sec->entsize = 16;
}

static void dyn_entry(unsigned char **p, uint64_t tag, uint64_t val) {
  wr64(*p, tag);
  wr64(*p + 8, val);
  *p += 16;
}

static unsigned char *put_rela(unsigned char *p, uint64_t off, int type, int sym, int64_t add) {
  wr64(p, off);
  wr64(p + 8, (uint64_t)sym << 32 | (uint32_t)type);
  wr64(p + 16, (uint64_t)add);
  return p + RELA_SIZE;
}

/* Fills the dynamic sections, once addresses are known */
static void fill_dynamic(LinkOptions *opts) {
  if (!shared)
    memcpy(find_out(".interp")->data, opts->interp, strlen(opts->interp) + 1);
  memcpy(find_out(".dynstr")->data, dynstr.buf, dynstr.len);

  unsigned char *y = find_out(".dynsym")->data + SYM_SIZE;
  for (int i = 0; i < ndyn; i++, y += SYM_SIZE) {
    GSym *g = dyn_syms[i];
    int bind, type = g->type, shndx;
    uint64_t value, size = g->size;
    if (g->copied || g->copy_of) {
      bind = STB_GLOBAL;
      shndx = out_index(dynbss_sec);
      value = gsym_addr(g);
      if (g->copy_of)
        size = g->copy_of->size;
    } else if (g->imported) {
      bind = g->referenced ? STB_GLOBAL : STB_WEAK;
      if (type == STT_GNU_IFUNC)
        type = STT_FUNC;  /* (glibc's strchr ...): resolved by its own library */
      shndx = SHN_UNDEF;
      value = g->canonical ? gsym_addr(g) : 0;
      size = 0;
    } else {
      bind = g->bind == STB_WEAK ? STB_WEAK : STB_GLOBAL;
      shndx = g->sec && g->sec->out ? out_index(g->sec->out) : SHN_ABS;
      value = gsym_addr(g);
      if (type == STT_TLS)
        value -= tls_start;
    }
    wr32(y, dynstr_add(g->name));
    y[4] = bind << 4 | type;
    y[5] = 0;
    wr16(y + 6, shndx);
    wr64(y + 8, value);
    wr64(y + 16, size);
  }

  unsigned char *h = find_out(".hash")->data;
  uint32_t nbucket = ndyn / 2 + 1, nchain = ndyn + 1;
  wr32(h, nbucket);
  wr32(h + 4, nchain);
  unsigned char *bucket = h + 8, *chain = h + 8 + 4 * nbucket;
  for (int i = ndyn; i >= 1; i--) {  /* chains in .dynsym order */
    uint32_t b = elf_hash(dyn_syms[i - 1]->name) % nbucket;
    wr32(chain + 4 * i, rd32(bucket + 4 * b));
    wr32(bucket + 4 * b, i);
  }

  if (nverneeds) {
    unsigned char *v = find_out(".gnu.version")->data;
    for (int i = 0; i < ndyn; i++)
      wr16(v + 2 * (i + 1), dyn_syms[i]->vna ? dyn_syms[i]->vna : 1);
    unsigned char *r = find_out(".gnu.version_r")->data;
    int done = 0;
    for (int i = 0; i < ndlibs; i++) {
      int cnt = 0;
      for (int k = 0; k < nverneeds; k++)
        cnt += verneeds[k].lib == dlibs[i];
      if (!cnt)
        continue;
      done++;
      wr16(r, 1);
      wr16(r + 2, cnt);
      wr32(r + 4, dlibs[i]->soname_off);
      wr32(r + 8, 16);
      wr32(r + 12, done == nverneed_libs ? 0 : 16 + 16 * cnt);
      unsigned char *a = r + 16;
      int j = 0;
      for (int k = 0; k < nverneeds; k++) {
        if (verneeds[k].lib != dlibs[i])
          continue;
        j++;
        wr32(a, elf_hash(verneeds[k].name));
        wr16(a + 4, 0);
        wr16(a + 6, verneeds[k].index);
        wr32(a + 8, dynstr_add(verneeds[k].name));
        wr32(a + 12, j == cnt ? 0 : 16);
        a += 16;
      }
      r = a;
    }
  }

  if (nrela_dyn && shared) {
    /* the relative relocations first (the C library's dynamic linker
       applies those of itself before anything else) */
    unsigned char *p = find_out(".rela.dyn")->data;
    for (int pass = 0; pass < 2; pass++) {
      for (int i = 0; i < ndata_rels; i++) {
        DataRel *d = &data_rels[i];
        if ((d->sym == NULL) != (pass == 0))
          continue;
        unsigned char *re = d->s->rela + d->r * RELA_SIZE;
        uint64_t info = rd64(re + 8);
        int64_t A = (int64_t)rd64(re + 16);
        uint64_t where = sec_addr(d->s) + rd64(re);
        GSym *g;
        uint64_t S = local_sym_addr(d->s->obj, info >> 32, &g);
        if (d->sym)
          p = put_rela(p, where, R_X86_64_64, d->sym->dynidx, A);
        else
          p = put_rela(p, where, R_X86_64_RELATIVE, 0, (int64_t)S + A);
      }
      for (int i = 0; i < ngot; i++) {
        GSym *g = got_syms[i];
        uint64_t where = got_sec->addr + 8 * (uint64_t)i;
        if (!got_reloc(g))
          continue;
        bool rel = !got_dynamic(g) && !g->got_tp;
        if (rel != (pass == 0))
          continue;
        if (rel)
          p = put_rela(p, where, R_X86_64_RELATIVE, 0, (int64_t)gsym_addr(g));
        else if (g->got_tp)  /* in its own TLS block, or another object's */
          p = put_rela(p, where, R_X86_64_TPOFF64, got_dynamic(g) ? g->dynidx : 0,
                       got_dynamic(g) ? 0 : (int64_t)(gsym_addr(g) - tls_start));
        else
          p = put_rela(p, where, R_X86_64_GLOB_DAT, g->dynidx, 0);
      }
      for (int i = 0; i < ngd && pass == 1; i++) {
        /* the module and the offset in its TLS block */
        GSym *g = gd_syms[i];
        uint64_t where = got_sec->addr + 8 * ((uint64_t)ngot + 2 * (uint64_t)i);
        bool own = binds_locally(g);
        p = put_rela(p, where, R_X86_64_DTPMOD64, own ? 0 : g->dynidx, 0);
        p = put_rela(p, where + 8, R_X86_64_DTPOFF64, own ? 0 : g->dynidx,
                     own ? (int64_t)(gsym_addr(g) - tls_start) : 0);
      }
    }
  } else if (nrela_dyn) {
    unsigned char *p = find_out(".rela.dyn")->data;
    for (int i = 0; i < ncopy; i++)
      p = put_rela(p, gsym_addr(copy_syms[i]), R_X86_64_COPY, copy_syms[i]->dynidx, 0);
    for (int i = 0; i < ngot; i++) {
      GSym *g = got_syms[i];
      if (got_dynamic(g))
        p = put_rela(p, got_sec->addr + 8 * (uint64_t)i,
                     g->got_tp ? R_X86_64_TPOFF64 : R_X86_64_GLOB_DAT, g->dynidx, 0);
    }
  }
  if (nplt) {
    unsigned char *p = find_out(".rela.plt")->data;
    for (int i = 0; i < nplt; i++)
      p = put_rela(p, gotplt_sec->addr + 8 * (uint64_t)(3 + i), R_X86_64_JUMP_SLOT,
                   plt_syms[i]->dynidx, 0);
    unsigned char *code = plt_sec->data;
    for (int i = 0; i < nplt; i++) {
      unsigned char *e = code + 16 * i;
      uint64_t at = plt_sec->addr + 16 * (uint64_t)i;
      e[0] = 0xff;  /* jmp *slot(%rip) */
      e[1] = 0x25;
      wr32(e + 2, (uint32_t)(gotplt_sec->addr + 8 * (uint64_t)(3 + i) - (at + 6)));
      memset(e + 6, 0xcc, 10);
    }
  }
  wr64(gotplt_sec->data, dynamic_sec->addr);

  unsigned char *d = dynamic_sec->data;
  for (int i = 0; i < ndlibs; i++)
    if (dlibs[i]->needed)
      dyn_entry(&d, DT_NEEDED, dlibs[i]->soname_off);
  if (opts->nrpath)
    dyn_entry(&d, DT_RUNPATH, runpath_off);
  GSym *g = hashmap_get(&gsyms, "_init");
  if (g && g->defined)
    dyn_entry(&d, DT_INIT, gsym_addr(g));
  g = hashmap_get(&gsyms, "_fini");
  if (g && g->defined)
    dyn_entry(&d, DT_FINI, gsym_addr(g));
  static struct { char *name; int tag, sztag; } arrays[] = {
      {".preinit_array", DT_PREINIT_ARRAY, DT_PREINIT_ARRAYSZ},
      {".init_array", DT_INIT_ARRAY, DT_INIT_ARRAYSZ},
      {".fini_array", DT_FINI_ARRAY, DT_FINI_ARRAYSZ},
  };
  for (int i = 0; i < 3; i++) {
    OutSec *a = find_out(arrays[i].name);
    if (a && a->size) {
      dyn_entry(&d, arrays[i].tag, a->addr);
      dyn_entry(&d, arrays[i].sztag, a->size);
    }
  }
  dyn_entry(&d, DT_HASH, find_out(".hash")->addr);
  dyn_entry(&d, DT_STRTAB, find_out(".dynstr")->addr);
  dyn_entry(&d, DT_SYMTAB, find_out(".dynsym")->addr);
  dyn_entry(&d, DT_STRSZ, dynstr.len);
  dyn_entry(&d, DT_SYMENT, SYM_SIZE);
  if (shared)
    dyn_entry(&d, DT_SONAME, soname_off);
  else
    dyn_entry(&d, DT_DEBUG, 0);
  if (nplt) {
    dyn_entry(&d, DT_PLTGOT, gotplt_sec->addr);
    dyn_entry(&d, DT_PLTRELSZ, (uint64_t)RELA_SIZE * nplt);
    dyn_entry(&d, DT_PLTREL, DT_RELA);
    dyn_entry(&d, DT_JMPREL, find_out(".rela.plt")->addr);
  }
  if (nrela_dyn) {
    dyn_entry(&d, DT_RELA, find_out(".rela.dyn")->addr);
    dyn_entry(&d, DT_RELASZ, (uint64_t)RELA_SIZE * nrela_dyn);
    dyn_entry(&d, DT_RELAENT, RELA_SIZE);
  }
  if (nverneeds) {
    dyn_entry(&d, DT_VERSYM, find_out(".gnu.version")->addr);
    dyn_entry(&d, DT_VERNEED, find_out(".gnu.version_r")->addr);
    dyn_entry(&d, DT_VERNEEDNUM, nverneed_libs);
  }
  dyn_entry(&d, DT_FLAGS, DF_BIND_NOW);
  dyn_entry(&d, DT_FLAGS_1, DF_1_NOW);
  /* the rest (unused room) is DT_NULL: the buffer is zeroed */
}

/* ---------------------------------------------------- relocation */

static void apply_relocs(InSec *s) {
  ObjFile *o = s->obj;
  unsigned char *base = s->out->data + s->offset;
  bool skip_next = false;
  for (uint64_t r = 0; r < s->nrela; r++) {
    unsigned char *re = s->rela + r * RELA_SIZE;
    uint64_t off = rd64(re);
    uint64_t info = rd64(re + 8);
    int64_t A = (int64_t)rd64(re + 16);
    int type = (uint32_t)info;
    int symi = info >> 32;
    if (skip_next) {
      skip_next = false;
      continue;
    }
    if (type == R_X86_64_NONE)
      continue;
    if (off >= s->size) {
      lerror("%s: relocation outside section %s", o->name, s->name);
      continue;
    }
    GSym *g = NULL;
    uint64_t S = symi ? local_sym_addr(o, symi, &g) : 0;
    uint64_t P = sec_addr(s) + off;
    unsigned char *loc = base + off;
    uint64_t gotaddr = got_sec ? got_sec->addr : 0;
    char *sname = symi ? sym_name(o, symi) : "";
    int64_t v;
    int size;
    bool sign = true;
    switch (type) {
    case R_X86_64_64: v = S + A; size = 8; break;
    case R_X86_64_PC64: v = S + A - P; size = 8; break;
    case R_X86_64_PC32:
    case R_X86_64_PLT32: v = S + A - P; size = 4; break;
    case R_X86_64_32: v = S + A; size = 4; sign = false; break;
    case R_X86_64_32S: v = S + A; size = 4; break;
    case R_X86_64_16: v = S + A; size = 2; sign = false; break;
    case R_X86_64_PC16: v = S + A - P; size = 2; break;
    case R_X86_64_8: v = S + A; size = 1; sign = false; break;
    case R_X86_64_PC8: v = S + A - P; size = 1; break;
    case R_X86_64_GOTPCREL:
    case R_X86_64_GOTPCRELX:
    case R_X86_64_REX_GOTPCRELX:
    case R_X86_64_GOTTPOFF:
      v = gotaddr + 8 * got_key(o, symi)->got + A - P;
      size = 4;
      break;
    case R_X86_64_GOT32: v = 8 * got_key(o, symi)->got + A; size = 4; break;
    case R_X86_64_GOTOFF64: v = S + A - gotaddr; size = 8; break;
    case R_X86_64_GOTPC32: v = gotaddr + A - P; size = 4; break;
    case R_X86_64_TPOFF32: v = tpoff(S) + A; size = 4; break;
    case R_X86_64_TPOFF64: v = tpoff(S) + A; size = 8; break;
    case R_X86_64_DTPOFF32:
      /* after the local-dynamic rewrite below, the base is the TP */
      v = (s->flags & SHF_ALLOC) ? tpoff(S) + A : (int64_t)(S - tls_start) + A;
      size = 4;
      break;
    case R_X86_64_DTPOFF64:
      v = (s->flags & SHF_ALLOC) ? tpoff(S) + A : (int64_t)(S - tls_start) + A;
      size = 8;
      break;
    case R_X86_64_SIZE32: v = (g ? g->size : sym_size(o, symi)) + A; size = 4; break;
    case R_X86_64_SIZE64: v = (g ? g->size : sym_size(o, symi)) + A; size = 8; break;
    case R_X86_64_TLSGD:
      if (shared) {  /* the GOT pair; __tls_get_addr is called */
        v = gotaddr + 8 * ((uint64_t)ngot + 2 * (uint64_t)got_key(o, symi)->gd) + A - P;
        size = 4;
        break;
      } {
      /* 66 48 8d 3d <x@tlsgd> 66 66 48 e8 <__tls_get_addr@plt>
         -> mov %fs:0,%rax; lea x@tpoff(%rax),%rax
         or, for a variable of a shared object (initial-exec),
         -> mov %fs:0,%rax; add x@gottpoff(%rip),%rax */
      static const unsigned char gd[4] = {0x66, 0x48, 0x8d, 0x3d};
      if (off < 4 || off + 12 > s->size || memcmp(loc - 4, gd, 4) || loc[4] != 0x66 ||
          loc[5] != 0x66 || loc[6] != 0x48 || loc[7] != 0xe8) {
        lerror("%s: unrecognised general-dynamic TLS sequence for '%s'", o->name, sname);
        continue;
      }
      if (g && g->imported) {
        static const unsigned char ie[12] = {0x64, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0,
                                             0x48, 0x03, 0x05};
        memcpy(loc - 4, ie, 12);
        wr32(loc + 8, (uint32_t)(gotaddr + 8 * (uint64_t)g->got - (P + 12)));
        skip_next = true;
        continue;
      }
      static const unsigned char le[12] = {0x64, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0,
                                           0x48, 0x8d, 0x80};
      memcpy(loc - 4, le, 12);
      wr32(loc + 8, (uint32_t)tpoff(S));
      skip_next = true;
      continue;
    }
    case R_X86_64_TLSLD: {
      /* 48 8d 3d <x@tlsld> e8 <__tls_get_addr@plt> -> 66 66 66 mov %fs:0,%rax */
      if (off < 3 || off + 9 > s->size || loc[-3] != 0x48 || loc[-2] != 0x8d ||
          loc[-1] != 0x3d || loc[4] != 0xe8) {
        lerror("%s: unrecognised local-dynamic TLS sequence", o->name);
        continue;
      }
      static const unsigned char le[12] = {0x66, 0x66, 0x66, 0x64, 0x48, 0x8b, 0x04, 0x25,
                                           0, 0, 0, 0};
      memcpy(loc - 3, le, 12);
      skip_next = true;
      continue;
    }
    default:
      lerror("%s: unsupported relocation type %d against '%s'", o->name, type, sname);
      continue;
    }
    if (size == 4 && type != R_X86_64_GOTOFF64) {
      bool ok = sign ? v >= INT32_MIN && v <= INT32_MAX : (uint64_t)v <= UINT32_MAX;
      if (!ok) {
        lerror("%s: relocation against '%s' in %s out of range", o->name,
               *sname ? sname : "(section)", s->name);
        continue;
      }
    }
    switch (size) {
    case 1: loc[0] = v; break;
    case 2: wr16(loc, v); break;
    case 4: wr32(loc, v); break;
    case 8: wr64(loc, v); break;
    }
  }
}

/* -------------------------------------------------------- output */

static void b16(Buf *b, uint16_t v) { buf_putc(b, v & 0xff); buf_putc(b, v >> 8); }
static void b32(Buf *b, uint32_t v) { b16(b, v & 0xffff); b16(b, v >> 16); }
static void b64(Buf *b, uint64_t v) { b32(b, (uint32_t)v); b32(b, (uint32_t)(v >> 32)); }

static void define_linker_sym(char *name, uint64_t value, OutSec *rel) {
  (void)rel;
  GSym *g = hashmap_get(&gsyms, name);
  if (!g || g->defined || (!g->referenced && !g->weak_ref))
    return;
  g->defined = true;
  g->linker_defined = true;
  g->is_abs = true;
  g->value = value;
  g->bind = STB_GLOBAL;
}

static OutSec *find_out(char *name) {
  for (int i = 0; i < nouts; i++)
    if (!strcmp(outs[i]->name, name))
      return outs[i];
  return NULL;
}

int link_executable(LinkOptions *opts) {
  gsyms = (HashMap){0};
  gsym_list = NULL;
  ngsyms = 0;
  objs = NULL;
  nobjs = 0;
  outs = NULL;
  nouts = 0;
  failed = false;
  got_syms = NULL;
  ngot = 0;
  gd_syms = NULL;
  ngd = 0;
  got_sec = bss_sec = NULL;
  has_tls = false;
  tls_align = 1;
  shared = opts->shared;
  dynamic = opts->interp != NULL || shared;
  base_addr = shared ? 0 : BASE_ADDR;
  script_has_base = false;
  nscript_end_syms = 0;
  dlibs = NULL;
  ndlibs = 0;
  plt_syms = copy_syms = dyn_syms = NULL;
  nplt = ncopy = ndyn = 0;
  plt_sec = gotplt_sec = dynbss_sec = dynamic_sec = dsohandle_sec = NULL;
  iplt_syms = NULL;
  niplt = 0;
  iplt_sec = igot_sec = rela_iplt_sec = NULL;

  /* the entry point: _start, or (a shared library: none) what a linker
     script names */
  char *entry = opts->entry;
  GSym *eg = NULL;
  if (entry) {
    eg = get_gsym(entry);
    eg->referenced = true;
  }

  /* Read the inputs in order */
  group_ars = NULL;
  ngroup_ars = 0;
  for (int i = 0; i < opts->ninputs; i++) {
    reading_hidden = shared && opts->hide_from >= 0 && i >= opts->hide_from;
    add_input(opts, opts->inputs[i],
              opts->names && opts->names[i] ? opts->names[i] : opts->inputs[i],
              i >= opts->group_start, false, 0);
  }
  reading_hidden = false;
  if (script_has_base)
    base_addr = script_base;
  if (opts->entry && (!entry || strcmp(entry, opts->entry))) {  /* ENTRY ( ... ) */
    entry = opts->entry;
    eg = get_gsym(entry);
    eg->referenced = true;
  }
  if (!entry && !shared) {  /* the default: _start (from crt1.o) */
    entry = "_start";
    eg = get_gsym(entry);
    eg->referenced = true;
  }
  for (bool changed = ngroup_ars > 1; changed;) {
    changed = false;
    for (int i = 0; i < ngroup_ars; i++)
      if (search_archive(group_ars[i]))
        changed = true;
  }
  if (failed)
    return 1;

  /* Undefined symbols (a shared library imports them) */
  for (int i = 0; i < ngsyms; i++) {
    GSym *g = gsym_list[i];
    if (g->defined || g->common || g->dso || !g->referenced)
      continue;
    if (is_linker_name(g->name))
      continue;  /* __start_/__stop_ checked when the sections are known */
    if (shared && g != eg) {
      if (g->vis != STV_DEFAULT)
        lerror("undefined symbol '%s' of %s visibility (first referenced in %s)", g->name,
               g->vis == STV_PROTECTED ? "protected" : "hidden",
               g->first_ref ? g->first_ref->name : "?");
      continue;
    }
    if (g == eg)
      lerror("entry symbol '%s' is not defined", entry);
    else
      lerror("undefined symbol '%s' (first referenced in %s)", g->name,
             g->first_ref ? g->first_ref->name : "?");
  }
  if (failed)
    return 1;

  /* Assign input sections to output sections */
  for (int k = 0; k < nobjs; k++) {
    ObjFile *o = objs[k];
    for (int i = 1; i < o->nsecs; i++) {
      InSec *s = &o->secs[i];
      if (s->discard)
        continue;
      int prio;
      char *n = out_name(s, &prio);
      if (!n)
        continue;
      if (opts->strip && !(s->flags & SHF_ALLOC))
        continue;
      if (!(s->flags & SHF_ALLOC) && s->type != SHT_PROGBITS)
        continue;
      s->prio = prio;
      OutSec *out = get_out(n, s);
      if (s->type == SHT_PROGBITS && out->type == SHT_NOBITS)
        out->type = SHT_PROGBITS;
      out->flags |= s->flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR | SHF_TLS);
      out->in = xrealloc(out->in, sizeof(InSec *) * (out->nin + 1));
      out->in[out->nin++] = s;
      s->out = out;
    }
  }
  /* Writable sections end up in the writable segment even if some inputs
     were read-only */
  for (int i = 0; i < nouts; i++) {
    OutSec *o = outs[i];
    if (o->seg >= 0)
      o->seg = (o->flags & SHF_WRITE) ? 2 : (o->flags & SHF_EXECINSTR) ? 1 : 0;
    if (!strcmp(o->name, ".init_array") || !strcmp(o->name, ".fini_array") ||
        !strcmp(o->name, ".preinit_array"))
      qsort(o->in, o->nin, sizeof(InSec *), cmp_prio);
  }

  /* Commons go in .bss */
  for (int i = 0; i < ngsyms; i++)
    if (gsym_list[i]->common && !gsym_list[i]->defined) {
      bss_sec = find_out(".bss");
      if (!bss_sec) {
        bss_sec = get_out(".bss", NULL);
        bss_sec->type = SHT_NOBITS;
        bss_sec->flags = SHF_ALLOC | SHF_WRITE;
        bss_sec->seg = 2;
      }
      break;
    }

  if (dynamic)
    scan_imports();
  if (shared)
    scan_shared();
  scan_ifuncs();
  if (failed)
    return 1;
  scan_got();
  if (ngot || ngd) {
    got_sec = get_out(".got", NULL);
    got_sec->flags = SHF_ALLOC | SHF_WRITE;
    got_sec->seg = 2;
    got_sec->align = 8;
  }
  if (hashmap_get(&gsyms, "_GLOBAL_OFFSET_TABLE_") && !got_sec && !dynamic) {
    got_sec = get_out(".got", NULL);
    got_sec->flags = SHF_ALLOC | SHF_WRITE;
    got_sec->seg = 2;
    got_sec->align = 8;
  }
  /* __dso_handle (glibc's atexit refers to it; gcc's crtbegin.o would
     define it): a variable holding 0 */
  GSym *dh = hashmap_get(&gsyms, "__dso_handle");
  if (dh && !dh->defined && !dh->common && !dh->imported && (dh->referenced || dh->weak_ref))
    dsohandle_sec = synth(".dso_handle", SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 8, 8);
  if (dynamic)
    make_dynamic_sections(opts);

  qsort(outs, nouts, sizeof(OutSec *), cmp_out);

  /* Sizes */
  for (int i = 0; i < nouts; i++) {
    OutSec *o = outs[i];
    if (o->nin < 0)
      continue;  /* synthesized: sized already */
    uint64_t off = 0;
    for (int k = 0; k < o->nin; k++) {
      InSec *s = o->in[k];
      if (s->align > o->align)
        o->align = s->align;
      off = align_up(off, s->align);
      s->offset = off;
      off += s->size;
    }
    if (o == got_sec)
      off = 8 * ((uint64_t)ngot + 2 * (uint64_t)ngd);
    if (o == bss_sec) {
      for (int k = 0; k < ngsyms; k++) {
        GSym *g = gsym_list[k];
        if (!g->common || g->defined)
          continue;
        uint64_t al = g->common_align ? g->common_align : 1;
        if (al > o->align)
          o->align = al;
        off = align_up(off, al);
        g->value = off;  /* made absolute below */
        off += g->size;
      }
    }
    o->size = off;
  }

  /* Addresses and file offsets */
  int nseg_used = 0;
  bool seg_present[3] = {false, false, false};
  for (int i = 0; i < nouts; i++)
    if (outs[i]->seg >= 0)
      seg_present[outs[i]->seg] = true;
  seg_present[0] = true;  /* the headers */
  for (int i = 0; i < 3; i++)
    nseg_used += seg_present[i];
  for (int i = 0; i < nouts; i++)
    if (outs[i]->flags & SHF_TLS)
      has_tls = true;
  /* + PT_GNU_STACK; PT_PHDR, PT_INTERP and PT_DYNAMIC if dynamic (a
     shared library: PT_DYNAMIC only); PT_NOTE for the V86BSD brand */
  OutSec *note_sec = find_out(".note.v86bsd.ident");
  int nphdr = nseg_used + 1 + (has_tls ? 1 : 0) + (dynamic ? (shared ? 1 : 3) : 0) +
              (note_sec ? 1 : 0);

  uint64_t seg_vaddr[3] = {0}, seg_off[3] = {0}, seg_filesz[3] = {0}, seg_memsz[3] = {0};
  uint64_t addr = base_addr, off = 0;
  off = EHDR_SIZE + (uint64_t)nphdr * PHDR_SIZE;
  addr += off;
  seg_vaddr[0] = base_addr;
  seg_off[0] = 0;
  int cur_seg = 0;
  bool tls_started = false;
  for (int i = 0; i < nouts; i++) {
    OutSec *o = outs[i];
    if (o->seg < 0)
      continue;
    if (o->seg != cur_seg) {
      /* close the previous segment and start a new page */
      seg_filesz[cur_seg] = off - seg_off[cur_seg];
      seg_memsz[cur_seg] = addr - seg_vaddr[cur_seg];
      cur_seg = o->seg;
      addr = align_up(addr, PAGE) + off % PAGE;
      seg_vaddr[cur_seg] = addr;
      seg_off[cur_seg] = off;
    }
    uint64_t al = o->align;
    if ((o->flags & SHF_TLS) && !tls_started) {
      /* the TLS segment starts aligned to its largest alignment */
      for (int k = 0; k < nouts; k++)
        if ((outs[k]->flags & SHF_TLS) && outs[k]->align > tls_align)
          tls_align = outs[k]->align;
      al = tls_align;
    }
    uint64_t na = align_up(addr, al);
    off += na - addr;
    addr = na;
    o->addr = addr;
    o->fileoff = off;
    if (o->flags & SHF_TLS) {
      if (!tls_started) {
        tls_started = true;
        tls_start = addr;
      }
      if (o->align > tls_align)
        tls_align = o->align;
      if (o->type == SHT_NOBITS) {
        tls_end = addr + o->size;
        continue;  /* .tbss occupies no address space here */
      }
      tls_end = addr + o->size;
      tls_filesz = addr + o->size - tls_start;
    }
    addr += o->size;
    if (o->type != SHT_NOBITS)
      off += o->size;
  }
  seg_filesz[cur_seg] = off - seg_off[cur_seg];
  seg_memsz[cur_seg] = addr - seg_vaddr[cur_seg];
  /* The writable segment's file size excludes trailing .bss */
  for (int s = 0; s < 3; s++) {
    uint64_t end_file = seg_off[s];
    for (int i = 0; i < nouts; i++)
      if (outs[i]->seg == s && outs[i]->type != SHT_NOBITS &&
          outs[i]->fileoff + outs[i]->size > end_file)
        end_file = outs[i]->fileoff + outs[i]->size;
    if (s == 0 && end_file < EHDR_SIZE + (uint64_t)nphdr * PHDR_SIZE)
      end_file = EHDR_SIZE + (uint64_t)nphdr * PHDR_SIZE;
    seg_filesz[s] = end_file - seg_off[s];
  }
  if (has_tls && tls_start % tls_align)
    lerror("internal error: TLS segment misaligned");

  /* Common symbols: make their values absolute */
  if (bss_sec)
    for (int k = 0; k < ngsyms; k++) {
      GSym *g = gsym_list[k];
      if (g->common && !g->defined) {
        g->defined = true;
        g->is_abs = true;
        g->value += bss_sec->addr;
        g->sec = NULL;
      }
    }

  /* Linker-defined symbols */
  OutSec *o;
  uint64_t etext = seg_vaddr[1] ? seg_vaddr[1] + seg_memsz[1] : addr;
  o = find_out(".init_array");
  define_linker_sym("__init_array_start", o ? o->addr : 0, o);
  define_linker_sym("__init_array_end", o ? o->addr + o->size : 0, o);
  o = find_out(".fini_array");
  define_linker_sym("__fini_array_start", o ? o->addr : 0, o);
  define_linker_sym("__fini_array_end", o ? o->addr + o->size : 0, o);
  o = find_out(".preinit_array");
  define_linker_sym("__preinit_array_start", o ? o->addr : 0, o);
  define_linker_sym("__preinit_array_end", o ? o->addr + o->size : 0, o);
  uint64_t edata = seg_vaddr[2] ? seg_vaddr[2] + seg_filesz[2] : etext;
  uint64_t end = seg_vaddr[2] ? seg_vaddr[2] + seg_memsz[2] : etext;
  /* Initialise the variable tracking the end of the program */
  if (!seg_present[2]) {
    edata = end = addr;
  }
  define_linker_sym("_etext", etext, NULL);
  define_linker_sym("etext", etext, NULL);
  define_linker_sym("_edata", edata, NULL);
  define_linker_sym("edata", edata, NULL);
  define_linker_sym("_end", end, NULL);
  define_linker_sym("end", end, NULL);
  o = find_out(".bss");
  define_linker_sym("__bss_start", o ? o->addr : edata, NULL);
  define_linker_sym("__ehdr_start", base_addr, NULL);
  for (int i = 0; i < nscript_end_syms; i++)
    define_linker_sym(script_end_syms[i], align_up(end, script_end_align[i]), NULL);
  define_linker_sym("__executable_start", base_addr, NULL);
  define_linker_sym("_GLOBAL_OFFSET_TABLE_",
                    gotplt_sec ? gotplt_sec->addr : got_sec ? got_sec->addr : 0, NULL);
  /* _DYNAMIC: undefined (0) in a static executable */
  if (dynamic)
    define_linker_sym("_DYNAMIC", dynamic_sec->addr, NULL);
  if (dsohandle_sec)
    define_linker_sym("__dso_handle", dsohandle_sec->addr, NULL);
  define_linker_sym("__rela_iplt_start", rela_iplt_sec ? rela_iplt_sec->addr : 0, NULL);
  define_linker_sym("__rela_iplt_end",
                    rela_iplt_sec ? rela_iplt_sec->addr + rela_iplt_sec->size : 0, NULL);
  /* __start_NAME and __stop_NAME: the bounds of the output section NAME
     (a C identifier), as GNU ld defines them */
  for (int i = 0; i < ngsyms; i++) {
    GSym *g = gsym_list[i];
    if (g->defined || g->dso || (!g->referenced && !g->weak_ref))
      continue;
    char *sec = startswith(g->name, "__start_") ? g->name + 8
                : startswith(g->name, "__stop_") ? g->name + 7 : NULL;
    OutSec *os = sec ? find_out(sec) : NULL;
    if (os)
      define_linker_sym(g->name, g->name[2] == 's' && g->name[3] == 't' && g->name[4] == 'a'
                                     ? os->addr : os->addr + os->size, NULL);
  }

  /* Contents */
  for (int i = 0; i < nouts; i++) {
    OutSec *os = outs[i];
    if (os->type == SHT_NOBITS)
      continue;
    os->data = xcalloc(os->size ? os->size : 1, 1);
    for (int k = 0; k < os->nin; k++) {
      InSec *s = os->in[k];
      if (s->data)
        memcpy(os->data + s->offset, s->data, s->size);
    }
  }
  if (got_sec)
    for (int i = 0; i < ngot; i++) {
      GSym *g = got_syms[i];
      if (got_dynamic(g))
        continue;  /* filled by the dynamic linker */
      uint64_t a = gsym_addr(g);
      wr64(got_sec->data + 8 * i, g->got_tp ? (uint64_t)tpoff(a) : a);
    }
  if (dynamic)
    fill_dynamic(opts);
  if (niplt)
    fill_ifuncs();
  for (int i = 0; i < nouts; i++) {
    OutSec *os = outs[i];
    if (os->type == SHT_NOBITS)
      continue;
    for (int k = 0; k < os->nin; k++)
      if (os->in[k]->rela)
        apply_relocs(os->in[k]);
  }
  if (failed)
    return 1;

  /* Symbol table (unless -s) */
  Buf symtab = {0}, strtab = {0};
  int first_global = 1;
  if (!opts->strip) {
    buf_putc(&strtab, 0);
    for (int k = 0; k < SYM_SIZE; k++)
      buf_putc(&symtab, 0);
    int nsym = 1;
    /* locals: named local symbols of each object */
    for (int pass = 0; pass < 2; pass++) {
      if (pass == 0) {
        for (int k = 0; k < nobjs; k++) {
          ObjFile *ob = objs[k];
          for (int i = 1; i < ob->first_global && i < ob->nsyms; i++) {
            int t = sym_type(ob, i);
            char *n = sym_name(ob, i);
            if (t == STT_SECTION || !*n || startswith(n, ".L"))
              continue;
            int shndx = sym_shndx(ob, i);
            uint64_t v;
            uint16_t ndx;
            if (t == STT_FILE) {
              v = 0;
              ndx = SHN_ABS;
            } else if (shndx == SHN_ABS) {
              v = sym_value(ob, i);
              ndx = SHN_ABS;
            } else if (shndx > 0 && shndx < ob->nsecs && ob->secs[shndx].out &&
                       (ob->secs[shndx].out->flags & SHF_ALLOC)) {
              GSym *dummy;
              v = local_sym_addr(ob, i, &dummy);
              ndx = 0;  /* fixed up below */
              ndx = 0xfff0;
              for (int q = 0; q < nouts; q++)
                if (outs[q] == ob->secs[shndx].out)
                  ndx = q + 1;
            } else {
              continue;
            }
            if (t == STT_TLS)
              v = v - tls_start;
            b32(&symtab, strtab.len);
            buf_write(&strtab, n, strlen(n) + 1);
            buf_putc(&symtab, STB_LOCAL << 4 | t);
            buf_putc(&symtab, 0);
            b16(&symtab, ndx);
            b64(&symtab, v);
            b64(&symtab, sym_size(ob, i));
            nsym++;
          }
        }
        first_global = nsym;
      } else {
        for (int k = 0; k < ngsyms; k++) {
          GSym *g = gsym_list[k];
          if (g->name[0] == '\001' ||
              (!g->defined && !g->weak_ref && !g->referenced && !g->copy_of))
            continue;
          uint16_t ndx = SHN_UNDEF;
          uint64_t v = gsym_addr(g);
          bool def = g->defined || g->copied || g->copy_of;
          if (g->copied || g->copy_of) {
            ndx = out_index(dynbss_sec);
          } else if (g->defined) {
            ndx = SHN_ABS;
            OutSec *gs = g->sec ? g->sec->out : (g->common || (g->linker_defined) ? NULL : NULL);
            if (gs)
              for (int q = 0; q < nouts; q++)
                if (outs[q] == gs)
                  ndx = q + 1;
          }
          int t = g->type;
          if (t == STT_TLS)
            v = v - tls_start;
          b32(&symtab, strtab.len);
          buf_write(&strtab, g->name, strlen(g->name) + 1);
          int bind = def ? (g->bind == STB_WEAK ? STB_WEAK : STB_GLOBAL)
                         : (g->referenced ? STB_GLOBAL : STB_WEAK);
          buf_putc(&symtab, bind << 4 | t);
          buf_putc(&symtab, 0);
          b16(&symtab, ndx);
          b64(&symtab, def || g->canonical ? v : 0);
          b64(&symtab, g->size);
          nsym++;
        }
      }
    }
  }

  /* File layout: segments' data, then non-allocated sections, then
     .symtab, .strtab, .shstrtab, then section headers */
  Buf out = {0};
  uint64_t filesize = 0;
  for (int i = 0; i < nouts; i++)
    if (outs[i]->seg >= 0 && outs[i]->type != SHT_NOBITS &&
        outs[i]->fileoff + outs[i]->size > filesize)
      filesize = outs[i]->fileoff + outs[i]->size;
  if (filesize < EHDR_SIZE + (uint64_t)nphdr * PHDR_SIZE)
    filesize = EHDR_SIZE + (uint64_t)nphdr * PHDR_SIZE;
  out.buf = xcalloc(filesize, 1);
  out.len = out.cap = filesize;
  for (int i = 0; i < nouts; i++)
    if (outs[i]->seg >= 0 && outs[i]->type != SHT_NOBITS)
      memcpy(out.buf + outs[i]->fileoff, outs[i]->data, outs[i]->size);
  for (int i = 0; i < nouts; i++)
    if (outs[i]->seg < 0) {
      outs[i]->fileoff = out.len;
      buf_write(&out, (char *)outs[i]->data, outs[i]->size);
    }

  Buf shstr = {0};
  buf_putc(&shstr, 0);
  uint32_t *name_off = xcalloc(nouts + 4, sizeof(uint32_t));
  for (int i = 0; i < nouts; i++) {
    name_off[i] = shstr.len;
    buf_write(&shstr, outs[i]->name, strlen(outs[i]->name) + 1);
  }
  uint64_t symoff = 0, stroff = 0;
  uint32_t sym_name_off = 0, str_name_off = 0;
  if (!opts->strip) {
    while (out.len % 8)
      buf_putc(&out, 0);
    symoff = out.len;
    buf_write(&out, symtab.buf, symtab.len);
    stroff = out.len;
    buf_write(&out, strtab.buf, strtab.len);
    sym_name_off = shstr.len;
    buf_write(&shstr, ".symtab", 8);
    str_name_off = shstr.len;
    buf_write(&shstr, ".strtab", 8);
  }
  uint32_t shstr_name_off = shstr.len;
  buf_write(&shstr, ".shstrtab", 10);
  uint64_t shstroff = out.len;
  buf_write(&out, shstr.buf, shstr.len);
  while (out.len % 8)
    buf_putc(&out, 0);
  uint64_t shoff = out.len;
  int nshdr = 1 + nouts + (opts->strip ? 0 : 2) + 1;
  int symtab_idx = 1 + nouts;
  for (int k = 0; k < SHDR_SIZE; k++)
    buf_putc(&out, 0);
  for (int i = 0; i < nouts; i++) {
    OutSec *os = outs[i];
    b32(&out, name_off[i]);
    b32(&out, os->type == SHT_NULL ? SHT_PROGBITS : os->type);
    b64(&out, os->flags);
    b64(&out, os->seg >= 0 ? os->addr : 0);
    b64(&out, os->fileoff);
    b64(&out, os->size);
    b32(&out, out_index_by_name(os->link_name));
    b32(&out, os->type == SHT_DYNSYM ? 1 : os->type == SHT_GNU_VERNEED ? (uint32_t)nverneed_libs
                                         : (uint32_t)out_index_by_name(os->info_name));
    b64(&out, os->align);
    b64(&out, os->entsize);
  }
  if (!opts->strip) {
    b32(&out, sym_name_off);
    b32(&out, SHT_SYMTAB);
    b64(&out, 0);
    b64(&out, 0);
    b64(&out, symoff);
    b64(&out, symtab.len);
    b32(&out, symtab_idx + 1);
    b32(&out, first_global);
    b64(&out, 8);
    b64(&out, SYM_SIZE);
    b32(&out, str_name_off);
    b32(&out, SHT_STRTAB);
    b64(&out, 0);
    b64(&out, 0);
    b64(&out, stroff);
    b64(&out, strtab.len);
    b32(&out, 0);
    b32(&out, 0);
    b64(&out, 1);
    b64(&out, 0);
  }
  b32(&out, shstr_name_off);
  b32(&out, SHT_STRTAB);
  b64(&out, 0);
  b64(&out, 0);
  b64(&out, shstroff);
  b64(&out, shstr.len);
  b32(&out, 0);
  b32(&out, 0);
  b64(&out, 1);
  b64(&out, 0);

  /* ELF header and program headers */
  unsigned char *h = (unsigned char *)out.buf;
  memcpy(h, "\177ELF", 4);
  h[4] = 2;
  h[5] = 1;
  h[6] = 1;
  wr16(h + 16, shared ? ET_DYN : ET_EXEC);
  wr16(h + 18, EM_X86_64);
  wr32(h + 20, 1);
  wr64(h + 24, eg ? gsym_addr(eg) : 0);
  wr64(h + 32, EHDR_SIZE);
  wr64(h + 40, shoff);
  wr32(h + 48, 0);
  wr16(h + 52, EHDR_SIZE);
  wr16(h + 54, PHDR_SIZE);
  wr16(h + 56, nphdr);
  wr16(h + 58, SHDR_SIZE);
  wr16(h + 60, nshdr);
  wr16(h + 62, nshdr - 1);
  unsigned char *ph = h + EHDR_SIZE;
  if (dynamic && !shared) {
    wr32(ph, PT_PHDR);
    wr32(ph + 4, PF_R);
    wr64(ph + 8, EHDR_SIZE);
    wr64(ph + 16, base_addr + EHDR_SIZE);
    wr64(ph + 24, base_addr + EHDR_SIZE);
    wr64(ph + 32, (uint64_t)nphdr * PHDR_SIZE);
    wr64(ph + 40, (uint64_t)nphdr * PHDR_SIZE);
    wr64(ph + 48, 8);
    ph += PHDR_SIZE;
    OutSec *in = find_out(".interp");
    wr32(ph, PT_INTERP);
    wr32(ph + 4, PF_R);
    wr64(ph + 8, in->fileoff);
    wr64(ph + 16, in->addr);
    wr64(ph + 24, in->addr);
    wr64(ph + 32, in->size);
    wr64(ph + 40, in->size);
    wr64(ph + 48, 1);
    ph += PHDR_SIZE;
  }
  static const int seg_flags[3] = {PF_R, PF_R | PF_X, PF_R | PF_W};
  for (int s = 0; s < 3; s++) {
    if (!seg_present[s])
      continue;
    wr32(ph, PT_LOAD);
    wr32(ph + 4, seg_flags[s]);
    wr64(ph + 8, seg_off[s]);
    wr64(ph + 16, seg_vaddr[s]);
    wr64(ph + 24, seg_vaddr[s]);
    wr64(ph + 32, seg_filesz[s]);
    wr64(ph + 40, s == 0 && seg_memsz[0] < seg_filesz[0] ? seg_filesz[0] : seg_memsz[s]);
    wr64(ph + 48, PAGE);
    ph += PHDR_SIZE;
  }
  if (note_sec) {
    wr32(ph, PT_NOTE);
    wr32(ph + 4, PF_R);
    wr64(ph + 8, note_sec->fileoff);
    wr64(ph + 16, note_sec->addr);
    wr64(ph + 24, note_sec->addr);
    wr64(ph + 32, note_sec->size);
    wr64(ph + 40, note_sec->size);
    wr64(ph + 48, 4);
    ph += PHDR_SIZE;
  }
  if (has_tls) {
    OutSec *first = NULL;
    for (int i = 0; i < nouts; i++)
      if ((outs[i]->flags & SHF_TLS) && !first)
        first = outs[i];
    wr32(ph, PT_TLS);
    wr32(ph + 4, PF_R);
    wr64(ph + 8, first->fileoff);
    wr64(ph + 16, tls_start);
    wr64(ph + 24, tls_start);
    wr64(ph + 32, tls_filesz);
    wr64(ph + 40, tls_end - tls_start);
    wr64(ph + 48, tls_align);
    ph += PHDR_SIZE;
  }
  if (dynamic) {
    wr32(ph, PT_DYNAMIC);
    wr32(ph + 4, PF_R | PF_W);
    wr64(ph + 8, dynamic_sec->fileoff);
    wr64(ph + 16, dynamic_sec->addr);
    wr64(ph + 24, dynamic_sec->addr);
    wr64(ph + 32, dynamic_sec->size);
    wr64(ph + 40, dynamic_sec->size);
    wr64(ph + 48, 8);
    ph += PHDR_SIZE;
  }
  wr32(ph, PT_GNU_STACK);
  wr32(ph + 4, PF_R | PF_W);
  wr64(ph + 48, 16);

  unlink(opts->output);
  int fd = open(opts->output, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd == -1) {
    lerror("cannot create %s: %s", opts->output, strerror(errno));
    return 1;
  }
  size_t done = 0;
  while (done < out.len) {
    ssize_t n = write(fd, out.buf + done, out.len - done);
    if (n <= 0) {
      if (n == -1 && errno == EINTR)
        continue;
      lerror("cannot write %s: %s", opts->output, strerror(errno));
      close(fd);
      unlink(opts->output);
      return 1;
    }
    done += n;
  }
  if (close(fd) == -1) {
    lerror("cannot write %s: %s", opts->output, strerror(errno));
    unlink(opts->output);
    return 1;
  }
  return failed ? 1 : 0;
}
