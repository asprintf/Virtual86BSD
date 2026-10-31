# gettext, ngettext — retrieve text string from messages object (POSIX.1-2024)

Implementations of the `gettext` and `ngettext` utilities of POSIX.1-2024
(XCU gettext and ngettext), one program that acts by its name, in C with no
dependencies beyond libc.

    gettext [-e|-E] [-d textdomain] [textdomain] msgid
    gettext [-e|-E] [-n] -s [-d textdomain] msgid...
    ngettext [-e|-E] [-d textdomain] [textdomain] msgid msgid_plural n

## Build and test

    make           # builds ./gettext and the link ./ngettext
    make test      # the tests (sh test_gettext.sh)
    make difftest  # compares with GNU gettext and ngettext
    make install   # into $(PREFIX)/bin; PREFIX=/usr/local, DESTDIR= for staging
    make clean

## What is implemented

* The messages objects (the format of GNU gettext, which msgfmt writes) are
  read here, not through the C library, and looked for as the standard's
  gettext() does: NLSPATH (%N %L %l %t %c %%), then LANGUAGE, then the
  locale of LC_MESSAGES, in TEXTDOMAINDIR or /usr/share/locale, with the
  shorter forms of the locale names.
* Plural forms by the header's "nplurals=...; plural=..." (a C expression of
  n); the messages converted from their charset to the locale's codeset
  (iconv).
* -e (the escapes of C, \c), -E, -s, -n, -d, the text domain operand and
  TEXTDOMAIN.

## Choices where the standard leaves room

* Without -e and -E, escapes are not processed; \c ends all the output.
* With no text domain the msgid is written, status 0.
* LANGUAGE: if a messages object lacks the message, the next name; when
  LANGUAGE led to a messages object, the locale's own name is not tried.
* A message without plural forms is the translation for ngettext whatever n
  is; a plural form the expression does not select is as if not found.
* n must be a decimal integer. Usage errors exit with status 2, other errors
  1.

## Testing

`test_gettext.sh` runs the utility on prepared input and compares the output
and the exit status: messages objects written by tests/mkmo.tl (in either
byte order, sorted or not): the text domain's three sources, the C locale,
plural forms (the standard's examples), LANGUAGE and the forms of names,
NLSPATH with every conversion, charsets, escapes, -s and -n, errors. The
binary tested is `./gettext`, or `$U` if set.

`tests/gettextdiff.tl` makes a locale with localedef (GNU gettext ignores
LANGUAGE in the C locales), random messages objects with GNU msgfmt, and
compares random calls of gettext and ngettext with GNU's; where GNU departs
from the standard (NLSPATH, LANGUAGE without a messages object, -e, -s with
several msgids) the cases are left out.

## Files

| file | contents |
|------|----------|
| `gettext.c` | the utility |
| `mo.c, mo.h` | reading messages objects |
| `../lib/plural.c`, `../lib/plural.h` | plural expressions (shared with msgfmt) |
| `../lib/cesc.c`, `../lib/cesc.h` | C escape sequences (shared with msgfmt and xgettext) |
| `tests/mkmo.tl` | writes messages objects for the tests |
| `tests/gettextdiff.tl` | the comparison with GNU gettext (`make difftest`) |
| `test_gettext.sh` | the tests: `make test`, or `sh test_gettext.sh` |
| `Makefile` | build, test, install, clean |
