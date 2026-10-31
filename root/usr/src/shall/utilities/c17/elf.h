/* elf.h - the parts of the ELF-64 format (System V ABI, x86-64 supplement)
   used by the assembler and the link editor.  Defined here rather than
   taken from <elf.h>, which is not a POSIX header. */

enum {
  ET_REL = 1, ET_EXEC = 2, ET_DYN = 3,
  EM_X86_64 = 62,
};

enum {
  SHT_NULL = 0, SHT_PROGBITS = 1, SHT_SYMTAB = 2, SHT_STRTAB = 3, SHT_RELA = 4,
  SHT_HASH = 5, SHT_DYNAMIC = 6, SHT_NOTE = 7, SHT_NOBITS = 8, SHT_REL = 9,
  SHT_DYNSYM = 11, SHT_INIT_ARRAY = 14, SHT_FINI_ARRAY = 15, SHT_PREINIT_ARRAY = 16,
  SHT_GROUP = 17, SHT_SYMTAB_SHNDX = 18,
  SHT_X86_64_UNWIND = 0x70000001,
  SHT_GNU_VERDEF = 0x6ffffffd, SHT_GNU_VERNEED = 0x6ffffffe, SHT_GNU_VERSYM = 0x6fffffff,
};

enum {
  SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4, SHF_MERGE = 0x10,
  SHF_STRINGS = 0x20, SHF_INFO_LINK = 0x40, SHF_GROUP = 0x200, SHF_TLS = 0x400,
};

enum {
  SHN_UNDEF = 0, SHN_ABS = 0xfff1, SHN_COMMON = 0xfff2, SHN_XINDEX = 0xffff,
};

enum { STB_LOCAL = 0, STB_GLOBAL = 1, STB_WEAK = 2 };
enum {
  STT_NOTYPE = 0, STT_OBJECT = 1, STT_FUNC = 2, STT_SECTION = 3, STT_FILE = 4,
  STT_COMMON = 5, STT_TLS = 6, STT_GNU_IFUNC = 10,
};
enum { STV_DEFAULT = 0, STV_INTERNAL = 1, STV_HIDDEN = 2, STV_PROTECTED = 3 };

enum { PT_NULL = 0, PT_LOAD = 1, PT_DYNAMIC = 2, PT_INTERP = 3, PT_NOTE = 4,
       PT_PHDR = 6, PT_TLS = 7, PT_GNU_STACK = 0x6474e551 };
enum { PF_X = 1, PF_W = 2, PF_R = 4 };

/* x86-64 relocation types */
enum {
  R_X86_64_NONE = 0,
  R_X86_64_64 = 1,
  R_X86_64_PC32 = 2,
  R_X86_64_GOT32 = 3,
  R_X86_64_PLT32 = 4,
  R_X86_64_COPY = 5,
  R_X86_64_GLOB_DAT = 6,
  R_X86_64_JUMP_SLOT = 7,
  R_X86_64_RELATIVE = 8,
  R_X86_64_GOTPCREL = 9,
  R_X86_64_32 = 10,
  R_X86_64_32S = 11,
  R_X86_64_16 = 12,
  R_X86_64_PC16 = 13,
  R_X86_64_8 = 14,
  R_X86_64_PC8 = 15,
  R_X86_64_DTPMOD64 = 16,
  R_X86_64_DTPOFF64 = 17,
  R_X86_64_TPOFF64 = 18,
  R_X86_64_TLSGD = 19,
  R_X86_64_TLSLD = 20,
  R_X86_64_DTPOFF32 = 21,
  R_X86_64_GOTTPOFF = 22,
  R_X86_64_TPOFF32 = 23,
  R_X86_64_PC64 = 24,
  R_X86_64_GOTOFF64 = 25,
  R_X86_64_GOTPC32 = 26,
  R_X86_64_SIZE32 = 32,
  R_X86_64_SIZE64 = 33,
  R_X86_64_GOTPCRELX = 41,
  R_X86_64_REX_GOTPCRELX = 42,
  R_X86_64_IRELATIVE = 37,
};

/* dynamic section tags */
enum {
  DT_NULL = 0, DT_NEEDED = 1, DT_PLTRELSZ = 2, DT_PLTGOT = 3, DT_HASH = 4, DT_STRTAB = 5,
  DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8, DT_RELAENT = 9, DT_STRSZ = 10, DT_SYMENT = 11,
  DT_INIT = 12, DT_FINI = 13, DT_SONAME = 14, DT_PLTREL = 20, DT_DEBUG = 21,
  DT_JMPREL = 23, DT_INIT_ARRAY = 25, DT_FINI_ARRAY = 26, DT_INIT_ARRAYSZ = 27,
  DT_FINI_ARRAYSZ = 28, DT_RUNPATH = 29, DT_FLAGS = 30, DT_PREINIT_ARRAY = 32,
  DT_PREINIT_ARRAYSZ = 33, DT_VERSYM = 0x6ffffff0, DT_FLAGS_1 = 0x6ffffffb,
  DT_VERNEED = 0x6ffffffe, DT_VERNEEDNUM = 0x6fffffff,
  DF_BIND_NOW = 8, DF_1_NOW = 1,
};

#define EHDR_SIZE 64
#define SHDR_SIZE 64
#define PHDR_SIZE 56
#define SYM_SIZE 24
#define RELA_SIZE 24

/* Little-endian accessors */
static inline uint16_t rd16(const unsigned char *p) { return p[0] | (uint16_t)p[1] << 8; }
static inline uint32_t rd32(const unsigned char *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }
static inline uint64_t rd64(const unsigned char *p) { return rd32(p) | (uint64_t)rd32(p + 4) << 32; }
static inline void wr16(unsigned char *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static inline void wr32(unsigned char *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }
static inline void wr64(unsigned char *p, uint64_t v) { wr32(p, v); wr32(p + 4, v >> 32); }

/* asm.c */
bool assemble(char *src, char *filename, char *outpath, bool debug);

/* link.c */
typedef struct {
  char **inputs;     /* object files and archives, in order */
  char **names;      /* names for diagnostics (NULL: the pathnames) */
  int ninputs;
  char *output;
  char *entry;       /* entry symbol */
  bool strip;
  int group_start;    /* inputs from here on are searched as a group */
  char *interp;       /* dynamic linking: the dynamic linker; NULL: static */
  char **rpath;       /* -R directories */
  int nrpath;
} LinkOptions;
int link_executable(LinkOptions *opts);
