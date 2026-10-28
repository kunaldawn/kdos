## About

help2man writes a manual page from what a program prints for `--help` and `--version`. It is a
Perl script, and in KDOS it is purely a build tool: libgpiod, libqmi, libmbim, speech-dispatcher,
verilator, z3 and the other ports that depend on it run it on their freshly built programs to make
the pages they install (see
[Manual pages](../../../../docs/kdos/05-developer/writing-ports.md#manual-pages)).

Its absence switches nothing off. A Makefile rule that pipes a program's `--help` into `help2man`
exits 127 when the script is missing and fails the build.

## Flags

- `--disable-nls` — Builds without help2man's support for localised pages: the message catalogues
  for its own text, and the `bindtextdomain.so` preload that lets a program read its message
  catalogues from the build tree so that its translated `--help` can be captured. configure
  otherwise turns the support on by itself whenever it finds the Perl module `Locale::gettext`,
  `msgfmt`, `dlsym` and `bindtextdomain` together. Only English pages are wanted: the package step removes every
  translated manual page directory (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
