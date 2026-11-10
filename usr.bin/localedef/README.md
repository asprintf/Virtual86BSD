# localedef — define locale environment (POSIX.1-2024)

An implementation of the `localedef` utility of POSIX.1-2024 (XCU
localedef), in C with no dependencies beyond libc.

    localedef [-c] [-f charmap] [-i sourcefile] [-u code_set_name] name

## Build and test

    make           # builds ./localedef
    make test      # the tests (sh test_localedef.sh)
    make difftest  # compares with the GNU C library's localedef
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The locale definition (-i, or the standard input) and charmap (-f) of the
  standard, with the extensions of the GNU C library's sources (copy,
  include, translit_start, LC_PAPER, LC_NAME, LC_ADDRESS, LC_TELEPHONE,
  LC_MEASUREMENT, LC_IDENTIFICATION, sections and reorder-after in
  LC_COLLATE ...).
* The locale is written in the binary format of the GNU C library, the files
  of the categories byte for byte as its localedef writes them; so the C
  library, and every utility, uses it.
* A name with "/" is the directory written to; another is that directory in
  /usr/lib/locale (the codeset normalized: UTF-8 is utf8). copy names, and
  -i names that are not files, are looked for in I18NPATH and
  /usr/share/i18n/locales.

## Choices where the standard leaves room

* Exit status 0, no problems; 1, warnings, the locale made (also with -c
  after errors); 4, errors, nothing made. A problem of syntax is a warning
  (the rest of the line skipped): exit status 1, where the GNU C library's
  localedef reports it but exits with 0.
* Not glibc's locale archive: each locale is a directory.
* The ISO 639 and 3166 codes of LC_ADDRESS and LC_IDENTIFICATION are not
  checked against tables.

## Using a locale it made

The GNU C library takes a locale name as a directory of its locale path:
LOCPATH=/where LC_ALL=name (/where/name the directory made); a name in
/usr/lib/locale needs no LOCPATH.

## Testing

`test_localedef.sh` runs the utility on prepared input and compares the
output and the exit status: a charmap and locale sources of the test: the
files made, from -i and the standard input, copy, warnings and errors, -c,
I18NPATH, usage; and, with the GNU C library, the locale in use: classes
(one of its own too), case maps, collation (collating elements, IGNORE),
numbers, dates, money, yesexpr, transliteration. The binary tested is
`./localedef`, or `$U` if set.

`tests/localedefdiff.sh` makes locales from the GNU C library's sources with
its localedef and this one, in UTF-8 and in charmaps of one and of several
bytes (ISO-8859-*, KOI8-R, CP1251, EUC-JP, EUC-KR, BIG5, GB18030, TCVN5712-1
...), and compares the files byte for byte (`all`: every locale source).

## Files

| file | contents |
|------|----------|
| `localedef.c` | the utility |
| `lex.c` | the lexer of locale sources and charmaps |
| `charmap.c` | charmaps (and repertoire maps) |
| `simple.c` | LC_NUMERIC, LC_MONETARY, LC_TIME, LC_MESSAGES and the categories of glibc |
| `ctype.c` | LC_CTYPE: classes, maps, digits, transliteration, widths |
| `collate.c` | LC_COLLATE |
| `localedef.h` | the declarations shared by the parts |
| `tests/localedefdiff.sh` | the comparison with glibc's localedef (`make difftest`) |
| `test_localedef.sh` | the tests: `make test`, or `sh test_localedef.sh` |
| `Makefile` | build, test, install, clean |
