# locale — get locale-specific information (POSIX.1-2024)

An implementation of the `locale` utility of POSIX.1-2024 (XCU locale), in C
with no dependencies beyond libc.

    locale [-a|-m]
    locale [-ck] name...

## Build and test

    make           # builds ./locale
    make test      # the tests (sh test_locale.sh)
    make difftest  # compares with the GNU C library's locale
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The values of the C library for the locale of the environment, through
  nl_langinfo() and localeconv(): every keyword of LC_MONETARY, LC_NUMERIC,
  LC_TIME and LC_MESSAGES of XBD 7.3, and charmap; the formats of -k and
  without it, -c.
* Without operands: LANG, the categories and LC_ALL, values quoted for the
  shell.
* -a: C, POSIX and the locales of the C library (its locale archive,
  /usr/lib/locale and LOCPATH), each one newlocale() accepts; -m: the
  charmaps of /usr/share/i18n/charmaps.

## Choices where the standard leaves room

* No keyword values for LC_CTYPE and LC_COLLATE: their keywords select the
  category but give no lines.
* Compound values as one string, entries separated by semicolons; with -k
  the semicolons, backslashes, double quotes and control characters in an
  entry escaped with a backslash. Unspecified numbers (CHAR_MAX) and an
  empty grouping are -1.
* The categories in the order LC_CTYPE, LC_COLLATE, LC_TIME, LC_NUMERIC,
  LC_MONETARY, LC_MESSAGES; the implied value is LC_ALL, else LANG, else
  POSIX.
* An unknown name: a message and status 1. A locale that cannot be used: a
  warning, the POSIX locale's values, status 0. Usage errors exit with
  status 2.

## Testing

`test_locale.sh` runs the utility on prepared input and compares the output
and the exit status: every value of the POSIX locale, -c and -k, several
names, the summary for settings of the variables (and read back by the
shell), -a and -m, errors. The binary tested is `./locale`, or `$U` if set.

`tests/localediff.sh` makes locales with localedef and compares every
keyword of the standard, with -k and without, the summary and -a with the
GNU C library's locale.

## Files

| file | contents |
|------|----------|
| `locale.c` | the utility |
| `tests/localediff.sh` | the comparison with GNU locale (`make difftest`) |
| `test_locale.sh` | the tests: `make test`, or `sh test_locale.sh` |
| `Makefile` | build, test, install, clean |
