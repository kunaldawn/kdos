## About

Snowball is a small language for writing stemmers, the algorithms that reduce a word to its stem
("connected", "connecting" and "connection" to "connect") so that a search matches every form of a
word. The port installs the `snowball` compiler, `stemwords`, which stems the words given on its
input, and `libstemmer`, the C library of the stemmers for every language the project ships.
[appstream](#port-appstream) uses it to stem the words of its search index, and
[calibre](#port-calibre) builds against it as well.

## Flags

- `CFLAGS="$CFLAGS -fPIC"` — Compile with the tree's flags and position-independent code. The
  makefile assigns `CFLAGS` itself, which hides the ones in the environment; given on the `make`
  command line they reach every object, and `-fPIC` is what each object needs to go into the
  shared library.

## Patches

- `libstemmer-library.patch` — Upstream's makefile builds `libstemmer` only as a static archive. The patch adds a `libstemmer.so` target to `all`, linked as `libstemmer.so.<version>` with the soname `libstemmer.so.<major>` and a version script, the new `libstemmer/symbol.map`, that exports only the `sb_stemmer_*` functions. It also changes the makefile's own `CFLAGS`, replacing `-g` with `-fPIC`; the `CFLAGS` that `build.sh` gives `make` replaces that line in this build. `build.sh` installs the library and makes the two shorter names links to it.
- `stemtest-fix.patch` — Deletes two lines that are repeated, out of place, in the table of test cases in `tests/stemtest.c`. As released the file does not compile, so `stemtest` cannot be built. Only the `check_stemtest` target builds it, and the recipe does not run that target.
