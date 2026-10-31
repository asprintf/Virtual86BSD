/* c17.c - c17: compile standard C programs: the utility (options, operands,
   the C library, temporary files, link editing) */
#include "c17.h"
#include "elf.h"
#include <signal.h>

bool opt_pic;
bool opt_g;
int opt_O = 1;  /* -O 0 disables the code generator's fast paths */
bool opt_E;
bool opt_fcommon;
bool input_is_preprocessed;
StrArray input_paths;
StrArray opt_defines;  /* "-D" definitions, and "\x01name" for -U */
char *libdir;

static bool opt_c;
static bool opt_S;
static bool opt_s;
static bool opt_G;
static bool opt_v;
static bool opt_static;
static bool opt_dynamic;
static char *opt_o;

static StrArray tmpfiles;

/* Link items in command-line order */
typedef enum { LI_FILE, LI_LIB, LI_LDIR, LI_RPATH } LinkKind;
typedef struct {
  LinkKind kind;
  char *arg;
} LinkItem;
static LinkItem *link_items;
static int nlink_items;

static void add_link_item(LinkKind kind, char *arg) {
  link_items = xrealloc(link_items, sizeof(LinkItem) * (nlink_items + 1));
  link_items[nlink_items].kind = kind;
  link_items[nlink_items].arg = arg;
  nlink_items++;
}

NORETURN static void usage(int status) {
  fprintf(status ? stderr : stdout,
          "usage: %s [-c] [-E] [-g] [-G] [-s] [-B mode] [-D name[=value]]... [-I directory]...\n"
          "           [-L directory]... [-O optlevel] [-o outfile] [-R directory]...\n"
          "           [-U name]... pathname [[pathname] [-I directory] [-L directory]\n"
          "           [-l library] [-R directory]]...\n",
          progname);
  exit(status);
}

static void cleanup(void) {
  for (int i = 0; i < tmpfiles.len; i++)
    unlink(tmpfiles.data[i]);
}

static void on_signal(int sig) {
  cleanup();
  signal(sig, SIG_DFL);
  raise(sig);
}

static char *tmpdir(void) {
  char *d = getenv("TMPDIR");
  if (d && *d)
    return d;
  return "/tmp";
}

static char *create_tmpfile(char *suffix) {
  (void)suffix;
  char *path = format("%s/c17-XXXXXX", tmpdir());
  int fd = mkstemp(path);
  if (fd == -1)
    fatal("cannot create a temporary file in %s: %s", tmpdir(), strerror(errno));
  close(fd);
  strarray_push(&tmpfiles, path);
  return path;
}

/* -------------------------------------------------------- paths */

/* c17's own directory: headers in libdir/include, libc17rt.a, liby.a,
   libl.a.  Set at build time; C17_LIBDIR overrides it. */
static char *find_libdir(void) {
  char *env = getenv("C17_LIBDIR");
  if (env && *env)
    return env;
#ifdef C17_LIBDIR
  return C17_LIBDIR;
#else
  return ".";
#endif
}

/* The C library, as a profile: a file of lines "key value" (# begins a
   comment):
     include DIR      a directory of its headers (one or more, in order)
     libdir DIR       a directory of crt1.o, libc.a, libc.so (one or more)
     crt1 FILE        the start file (crt1.o); crti, crtn: optional
     interp PATH      the dynamic linker: dynamic linking is possible
     link MODE        static or dynamic: the default
   C17_LIBC names a profile in $libdir/libc, or is the pathname of one, or
   of a directory with include/ and lib/ (a static library); without it,
   the profile chosen when c17 was built (make LIBC=name). */
static StrArray libc_inc, libc_lib;
static char *libc_crt[3], *libc_interp, *libc_name;
static bool libc_dynamic;

static void read_profile(char *path) {
  char *text = read_file(path);
  if (!text)
    fatal("cannot read the C library profile %s: %s", path, strerror(errno));
  libc_crt[0] = "crt1.o";
  int lineno = 0;
  for (char *line = text; *line;) {
    char *end = strchr(line, '\n');
    if (end)
      *end = '\0';
    lineno++;
    char *hash = strchr(line, '#');
    if (hash)
      *hash = '\0';
    char *key = strtok(line, " \t\r");
    char *val = key ? strtok(NULL, " \t\r") : NULL;
    if (key) {
      if (!val)
        fatal("%s:%d: '%s' without a value", path, lineno, key);
      if (!strcmp(key, "include"))
        strarray_push(&libc_inc, xstrdup(val));
      else if (!strcmp(key, "libdir"))
        strarray_push(&libc_lib, xstrdup(val));
      else if (!strcmp(key, "crt1"))
        libc_crt[0] = xstrdup(val);
      else if (!strcmp(key, "crti"))
        libc_crt[1] = xstrdup(val);
      else if (!strcmp(key, "crtn"))
        libc_crt[2] = xstrdup(val);
      else if (!strcmp(key, "interp"))
        libc_interp = xstrdup(val);
      else if (!strcmp(key, "link") && (!strcmp(val, "static") || !strcmp(val, "dynamic")))
        libc_dynamic = !strcmp(val, "dynamic");
      else
        fatal("%s:%d: unknown line '%s %s'", path, lineno, key, val);
    }
    if (!end)
      break;
    line = end + 1;
  }
  if (!libc_inc.len || !libc_lib.len)
    fatal("%s: the profile needs 'include' and 'libdir'", path);
  if (libc_dynamic && !libc_interp)
    fatal("%s: dynamic linking without 'interp'", path);
}

static void find_libc(void) {
  char *env = getenv("C17_LIBC");
#ifdef C17_LIBC_DEFAULT
  char *name = env && *env ? env : C17_LIBC_DEFAULT;
#else
  char *name = env && *env ? env : "glibc";
#endif
  libc_name = name;
  struct stat st;
  if (strchr(name, '/') && stat(name, &st) == 0 && S_ISDIR(st.st_mode)) {
    strarray_push(&libc_inc, format("%s/include", name));
    strarray_push(&libc_lib, format("%s/lib", name));
    libc_crt[0] = "crt1.o";
    libc_crt[1] = "crti.o";
    libc_crt[2] = "crtn.o";
    return;
  }
  read_profile(strchr(name, '/') ? name : format("%s/libc/%s", libdir, name));
}

/* ------------------------------------------------------ compile */

static StrArray include_dirs;  /* -I */

/* the preprocessor: -D, -U (which wins whatever the order), -I, the
   C library's headers */
static struct cpp *setup_preprocessor(void) {
  struct cpp *p = cpp_new();
  cpp_define(p, "__c17__");
  cpp_define(p, "__STDC_IEC_559_COMPLEX__");
  if (opt_pic) {
    cpp_define(p, "__PIC__=2");
    cpp_define(p, "__pic__=2");
  }
  for (int i = 0; i < opt_defines.len; i++)
    if (opt_defines.data[i][0] != '\x01')
      cpp_define(p, opt_defines.data[i]);
  for (int i = 0; i < opt_defines.len; i++)
    if (opt_defines.data[i][0] == '\x01')
      cpp_undef(p, opt_defines.data[i] + 1);
  for (int i = 0; i < include_dirs.len; i++)
    cpp_include_dir(p, include_dirs.data[i]);
  for (int i = 0; i < libc_inc.len; i++)
    cpp_system_dir(p, libc_inc.data[i]);
  static struct cpp_hooks hooks = {.diag = cpp_diagnostic};
  cpp_set_hooks(p, &hooks);
  cpp_keep_pragmas(p, opt_E);
  return p;
}

/* Translates one source file to assembly in `out`.  Returns false on
   error (diagnostics already written). */
static bool translate(char *input, FILE *out) {
  jmp_buf jb;
  error_jmp = &jb;
  if (setjmp(jb))
    return false;

  input_is_preprocessed = endswith(input, ".i");
  struct cpp *p = setup_preprocessor();
  cpp_set_preprocessed(p, input_is_preprocessed);
  struct token *pt = cpp_run(p, input, input);
  if (!pt) {
    fprintf(stderr, "%s: %s: %s\n", progname, input, strerror(errno));
    return false;
  }
  if (opt_E) {
    cpp_print(pt, out);
    return true;
  }

  base_file = input;
  Obj *prog = parse(convert_tokens(pt));
  codegen(prog, out);
  return true;
}

/* Compiles input to an object file at output.  Runs in a child process
   so that each translation unit starts from a clean state. */
static int compile_one(char *input, char *output) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid == -1)
    fatal("fork: %s", strerror(errno));
  if (pid == 0) {
    if (opt_E) {
      FILE *out = stdout;
      if (output) {
        out = fopen(output, "w");
        if (!out) {
          fprintf(stderr, "%s: cannot open %s: %s\n", progname, output, strerror(errno));
          _exit(1);
        }
      }
      bool ok = translate(input, out);
      if (fflush(out) != 0 || ferror(out)) {
        fprintf(stderr, "%s: write error: %s\n", progname, strerror(errno));
        _exit(1);
      }
      _exit(ok ? 0 : 1);
    }

    if (endswith(input, ".s")) {
      /* an assembly source operand */
      char *src = read_file(input);
      if (!src) {
        fprintf(stderr, "%s: %s: %s\n", progname, input, strerror(errno));
        _exit(1);
      }
      _exit(assemble(src, input, output, opt_g) ? 0 : 1);
    }

    char *text = NULL;
    size_t textlen = 0;
    FILE *out = opt_S ? fopen(output, "w") : open_memstream(&text, &textlen);
    if (!out) {
      fprintf(stderr, "%s: cannot open %s: %s\n", progname, opt_S ? output : "memory stream",
              strerror(errno));
      _exit(1);
    }
    bool ok = translate(input, out);
    if (fclose(out) != 0) {
      fprintf(stderr, "%s: %s: %s\n", progname, opt_S ? output : input, strerror(errno));
      ok = false;
    }
    if (!ok) {
      if (opt_S)
        unlink(output);
      _exit(1);
    }
    if (opt_S)
      _exit(0);
    if (!assemble(text, input, output, opt_g)) {
      fprintf(stderr, "%s: internal error: the generated assembly for %s was rejected\n",
              progname, input);
      unlink(output);
      _exit(1);
    }
    _exit(0);
  }
  int status;
  while (waitpid(pid, &status, 0) == -1)
    if (errno != EINTR)
      fatal("waitpid: %s", strerror(errno));
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  fprintf(stderr, "%s: internal error: compiler process terminated by signal %d\n", progname,
          WTERMSIG(status));
  return 1;
}

/* --------------------------------------------------------- link */

/* Resolves -l name to a pathname, searching the -L directories given
   before it, then the C library's directories, then c17's, then
   $(PREFIX)/lib, where yacc and lex install liby.a and libl.a (XCU c17 -l).
   With dynamic linking libname.so comes before libname.a in each
   directory; with static linking only libname.a is used. */
static bool link_dynamic;

static char *resolve_lib(char *name, StrArray *ldirs) {
  StrArray dirs = {0};
  for (int i = 0; i < ldirs->len; i++)
    strarray_push(&dirs, ldirs->data[i]);
  for (int i = 0; i < libc_lib.len; i++)
    strarray_push(&dirs, libc_lib.data[i]);
  strarray_push(&dirs, libdir);
#ifdef C17_SYSLIBDIR
  strarray_push(&dirs, C17_SYSLIBDIR);
#endif
  for (int i = 0; i < dirs.len; i++) {
    char *so = format("%s/lib%s.so", dirs.data[i], name);
    if (link_dynamic && file_exists(so))
      return so;
    char *a = format("%s/lib%s.a", dirs.data[i], name);
    if (file_exists(a))
      return a;
  }
  return NULL;
}

/* A file of the C library: in the first of its directories that has it */
static char *libc_file(char *name) {
  for (int i = 0; i < libc_lib.len; i++) {
    char *p = format("%s/%s", libc_lib.data[i], name);
    if (file_exists(p))
      return p;
  }
  return NULL;
}

/* Libraries whose contents are part of libc itself */
static bool in_libc(char *name) {
  static char *names[] = {"c", "m", "pthread", "rt", "xnet", "dl", "trace", "util",
                          "resolv", "crypt", "iconv", NULL};
  for (int i = 0; names[i]; i++)
    if (!strcmp(name, names[i]))
      return true;
  return false;
}

static int do_link(StrArray *objs_by_operand, StrArray *names_by_operand, char *output) {
  StrArray in = {0}, names = {0};
  /* crt1.o is required; crti.o and crtn.o (the frame of .init and
     .fini) are used when the C library has them */
  char *crtpath[3] = {NULL, NULL, NULL};
  for (int i = 0; i < 3; i++) {
    if (!libc_crt[i])
      continue;
    crtpath[i] = libc_file(libc_crt[i]);
    if (!crtpath[i] && i == 0) {
      fprintf(stderr, "%s: cannot find %s (C library %s, in %s)\n", progname, libc_crt[i],
              libc_name, libc_lib.data[0]);
      return 1;
    }
  }
  strarray_push(&in, crtpath[0]);
  if (crtpath[1])
    strarray_push(&in, crtpath[1]);

  /* Operands and -L/-l in order */
  StrArray ldirs = {0}, rpath = {0};
  int obj_i = 0;
  for (int i = 0; i < nlink_items; i++) {
    LinkItem *it = &link_items[i];
    switch (it->kind) {
    case LI_FILE:
      while (names.len < in.len)
        strarray_push(&names, NULL);
      strarray_push(&names, names_by_operand->data[obj_i]);
      strarray_push(&in, objs_by_operand->data[obj_i++]);
      break;
    case LI_LDIR:
      strarray_push(&ldirs, it->arg);
      break;
    case LI_RPATH:
      strarray_push(&rpath, it->arg);
      break;
    case LI_LIB: {
      char *path = resolve_lib(it->arg, &ldirs);
      if (!path) {
        if (in_libc(it->arg))
          break;
        fprintf(stderr, "%s: cannot find library -l %s\n", progname, it->arg);
        return 1;
      }
      strarray_push(&in, path);
      break;
    }
    }
  }

  /* The equivalent of -l c after the last operand, and c17's run-time
     support, searched together */
  int group_start = in.len;
  char *libc = resolve_lib("c", &ldirs);
  if (!libc) {
    fprintf(stderr, "%s: cannot find libc.%s (C library %s, in %s)\n", progname,
            link_dynamic ? "so" : "a", libc_name, libc_lib.data[0]);
    return 1;
  }
  strarray_push(&in, libc);
  char *rt = format("%s/libc17rt.a", libdir);
  if (file_exists(rt))
    strarray_push(&in, rt);
  if (crtpath[2])
    strarray_push(&in, crtpath[2]);

  if (opt_v) {
    fprintf(stderr, "libc: %s (%s)\n", libc_name, link_dynamic ? "dynamic" : "static");
    fprintf(stderr, "link:");
    for (int i = 0; i < in.len; i++)
      fprintf(stderr, " %s", in.data[i]);
    fprintf(stderr, "\n");
  }
  LinkOptions lo = {0};
  while (names.len < in.len)
    strarray_push(&names, NULL);
  lo.inputs = in.data;
  lo.names = names.data;
  lo.ninputs = in.len;
  lo.output = output;
  lo.strip = opt_s;
  lo.group_start = group_start;
  if (link_dynamic) {
    lo.interp = libc_interp;
    lo.rpath = rpath.data;
    lo.nrpath = rpath.len;
  }
  return link_executable(&lo);
}

/* ------------------------------------------------------ options */

typedef enum { OP_SOURCE, OP_PREPROCESSED, OP_ASM, OP_OTHER } OperandKind;

static OperandKind operand_kind(char *path) {
  if (endswith(path, ".c"))
    return OP_SOURCE;
  if (endswith(path, ".i"))
    return OP_PREPROCESSED;
  if (endswith(path, ".s"))
    return OP_ASM;  /* extension: assembly source */
  return OP_OTHER;
}

static StrArray operands;

static bool valid_optlevel(char *s) {
  return !strcmp(s, "0") || !strcmp(s, "1") || !strcmp(s, "2") || !strcmp(s, "3") ||
         !strcmp(s, "s") || !strcmp(s, "z") || !strcmp(s, "fast") || !strcmp(s, "g");
}

static void parse_args(int argc, char **argv) {
  bool end_of_opts = false;
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];

    if (end_of_opts || arg[0] != '-' || arg[1] == '\0') {
      strarray_push(&operands, arg);
      if (operand_kind(arg) == OP_OTHER)
        add_link_item(LI_FILE, arg);
      else
        add_link_item(LI_FILE, NULL);
      continue;
    }
    if (!strcmp(arg, "--")) {
      end_of_opts = true;
      continue;
    }

    /* Options taking an argument: attached or separate */
    char opt = arg[1];
    if (strchr("BDILlOoRU", opt)) {
      char *val;
      if (arg[2])
        val = arg + 2;
      else {
        if (i + 1 >= argc) {
          fprintf(stderr, "%s: option requires an argument -- '%c'\n", progname, opt);
          usage(2);
        }
        val = argv[++i];
      }
      switch (opt) {
      case 'B':
        if (!strcmp(val, "dynamic"))
          opt_dynamic = true, opt_static = false;
        else if (!strcmp(val, "static"))
          opt_static = true, opt_dynamic = false;
        else
          fatal("-B: unsupported mode '%s'", val);
        break;
      case 'D':
        if (!*val || *val == '=')
          fatal("-D: macro name missing");
        strarray_push(&opt_defines, val);
        break;
      case 'U':
        strarray_push(&opt_defines, format("\x01%s", val));
        break;
      case 'I':
        strarray_push(&include_dirs, val);
        break;
      case 'L':
        add_link_item(LI_LDIR, val);
        break;
      case 'l':
        add_link_item(LI_LIB, val);
        break;
      case 'R':
        add_link_item(LI_RPATH, val);
        break;
      case 'O':
        if (!valid_optlevel(val))
          fatal("-O: invalid optimization level '%s'", val);
        opt_O = isdigit((unsigned char)*val) ? *val - '0' : 2;
        break;
      case 'o':
        opt_o = val;
        break;
      }
      continue;
    }

    /* Flags: each given separately per POSIX, but grouping is accepted. */
    for (char *p = arg + 1; *p; p++) {
      switch (*p) {
      case 'c': opt_c = true; break;
      case 'E': opt_E = true; break;
      case 'g': opt_g = true; break;
      case 'G': opt_G = true; break;
      case 's': opt_s = true; break;
      case 'S': opt_S = true; break;   /* extension: write assembly */
      case 'w': opt_w = true; break;   /* extension: no warnings */
      case 'v': opt_v = true; break;   /* extension: show commands */
      case 'h':
        usage(0);
      default:
        fprintf(stderr, "%s: illegal option -- '%c'\n", progname, *p);
        usage(2);
      }
    }
  }
}

int main(int argc, char **argv) {
  progname = "c17";
  atexit(cleanup);
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);
  signal(SIGHUP, on_signal);

  parse_args(argc, argv);

  if (operands.len == 0) {
    fprintf(stderr, "%s: no input files\n", progname);
    usage(2);
  }

  libdir = find_libdir();
  find_libc();
  if (opt_G)
    fatal("-G is not supported yet: no shared objects");
  link_dynamic = opt_dynamic ? true : opt_static ? false : libc_dynamic;
  if (link_dynamic && !libc_interp)
    fatal("-B dynamic: the C library %s has no dynamic linker ('interp')", libc_name);
  if (!link_dynamic && !opt_E && !opt_c && !opt_S)
    for (int i = 0; i < nlink_items; i++)
      if (link_items[i].kind == LI_RPATH) {
        fprintf(stderr, "%s: warning: -R ignored: static linking\n", progname);
        break;
      }
  opt_pic = false;
  if (opt_g && opt_s)
    opt_g = false;  /* unspecified; we strip */

  int ncompile = 0;
  for (int i = 0; i < operands.len; i++)
    if (operand_kind(operands.data[i]) != OP_OTHER)
      ncompile++;

  if ((opt_c || opt_S) && opt_o && ncompile > 1)
    fatal("cannot specify -o with -c or -S and multiple input files");

  bool failed = false;

  /* -E: preprocess only */
  if (opt_E) {
    for (int i = 0; i < operands.len; i++) {
      char *in = operands.data[i];
      if (operand_kind(in) == OP_OTHER || operand_kind(in) == OP_ASM) {
        fprintf(stderr, "%s: %s: not a C source file; ignored with -E\n", progname, in);
        continue;
      }
      if (compile_one(in, opt_o) != 0)
        failed = true;
    }
    return failed;
  }

  /* Compile each .c / .i operand */
  StrArray objs = {0}, objnames = {0};
  for (int i = 0; i < operands.len; i++) {
    char *in = operands.data[i];
    if (operand_kind(in) == OP_OTHER) {
      if (!file_exists(in)) {
        fprintf(stderr, "%s: %s: %s\n", progname, in, strerror(errno));
        failed = true;
      }
      strarray_push(&objs, in);
      strarray_push(&objnames, in);
      continue;
    }
    if (!file_exists(in)) {
      fprintf(stderr, "%s: %s: %s\n", progname, in, strerror(errno));
      failed = true;
      continue;
    }
    char *out;
    if (opt_S && operand_kind(in) == OP_ASM)
      continue;
    if (opt_S)
      out = opt_o ? opt_o : replace_extn(in, ".s");
    else if (opt_c)
      out = opt_o ? opt_o : replace_extn(in, ".o");
    else
      out = create_tmpfile(".o");
    if (compile_one(in, out) != 0) {
      failed = true;
      continue;
    }
    strarray_push(&objs, out);
    strarray_push(&objnames, in);
  }

  if (opt_c || opt_S || failed)
    return failed ? 1 : 0;

  return do_link(&objs, &objnames, opt_o ? opt_o : "a.out");
}
