## About

cdparanoia is the Paranoia III audio CD ripper: the `cdparanoia` program, and the
`libcdda_interface` and `libcdda_paranoia` libraries it is built on, which read audio sectors and
correct jitter, scratches and drive caching by overlapping reads. [audiocd-kio](#port-audiocd-kio)
requires the libraries and their headers for its `audiocd:/` worker. The program is GPL and the
libraries LGPL; both licence texts ship.

## Flags

- `env -u CC` — Run `configure` with `CC` unset. `configure.in` calls `AC_PROG_CC` only when
  `CC` is empty, and the tree exports `CC=gcc`, so the macro is skipped and the object-file
  extension it would detect stays empty: every later compile test then looks for `conftest.` and
  fails, and `configure` stops with `No 16 bit type found on this platform!`. Unset, the macro
  runs and finds `gcc` itself, which it writes into the makefiles.
- `make -j1` — The tree exports `MAKEFLAGS=-j$KDOS_JOBS`, and the `interface/` and `paranoia/`
  makefiles build the static library and the shared one from the same object names, with the
  shared build's `lessmessy` step deleting every `.o` first. In parallel the two targets delete
  each other's objects.
- `CC="${CC:-cc} $LDFLAGS"` — The two shared libraries are linked by a rule that reads only `CC`,
  the objects and `-lm -lrt`, never `LDFLAGS`, so they would be linked without `-z now`,
  `--as-needed` or the build ID. Putting the flags in `CC` hands them to every compile and link;
  the compiler ignores linker options on a `-c` line.
- `prefix=$PKG/usr MANDIR=$PKG/usr/share/man` — The makefiles have no `DESTDIR`; the install
  writes under `prefix`. This configure script predates `datarootdir` and puts manual pages in
  `$prefix/man`, so `MANDIR` is named.

## Patches

Each patch carries a `Subject:` line, and all three are Alpine's (`main/cdparanoia`).

## Notes

`configure` copies the 2006 `configure.guess` and `configure.sub` to `config.guess` and
`config.sub` as it runs, after `kpkg` has looked for files of those names, so the old scripts
decide the host triplet: `x86_64-unknown-linux-gnu`. The triplet only chooses upstream's
optimisation flags (`-O2 -fsigned-char` for anything that is not 32-bit x86), and the exported
`CFLAGS` follow them, so the wrong C library in the name changes nothing in the build.

The install copies the static archives `libcdda_interface.a` and `libcdda_paranoia.a` beside the
shared libraries; the package keeps them, as it keeps every archive a build installs.
