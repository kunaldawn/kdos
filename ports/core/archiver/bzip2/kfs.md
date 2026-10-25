## About

bzip2 is the Burrows–Wheeler block-sorting compressor: the library `libbz2` and the `bzip2`,
`bunzip2` and `bzcat` commands, with the `bzgrep`, `bzmore` and `bzdiff` scripts. Programs that
open `.bz2` files link the library, among them [file](#port-file), [libarchive](#port-libarchive),
[elfutils](#port-elfutils) and [ffmpeg](#port-ffmpeg). It is the first of the archiver shelf in
`30_foundation`; no port of `20_selfhost` needs it.

The source has no configure script, only two makefiles: `Makefile-libbz2_so` builds the shared
library and a `bzip2` linked against it, and `Makefile` builds the static library, a statically
linked `bzip2` and `bzip2recover`. Neither installs everything a distribution needs, so most of
`build.sh` is the install done by hand.

## Flags

- `CC="gcc $LDFLAGS"` — the compiler, with the tree's linker flags attached. The shared-library
  makefile's link lines read no `LDFLAGS` variable at all, so carrying the flags in `CC` is the one
  way to put them on every link.
- `CFLAGS="$CFLAGS -fPIC"` — given on the command line, this replaces each makefile's own
  `CFLAGS` line, which is `-Wall -Winline -O2 -g` with `-D_FILE_OFFSET_BITS=64`: the tree's flags
  are used, and no `-g`. On musl `off_t` is always 64 bits wide, so the dropped define changes
  nothing. `-fPIC` restates what the shared-library makefile's own line carries; the tree's `CFLAGS` hold it
  already, and the static build is given the plain `$CFLAGS`.
- `PREFIX=$PKG/usr` — the makefile's install target writes straight under `PREFIX` and takes no
  `DESTDIR`, so the staging directory goes into the prefix.

## Notes

The `make clean` between the two builds deletes the first build's objects. Both makefiles compile
the same object names, so without it the second `make` would archive the shared-library build's
objects into `libbz2.a` instead of compiling its own. `Makefile`'s `clean` leaves
`libbz2.so.<version>` and `bzip2-shared` in place for the install that follows.

The second `make` is upstream's default target, which ends by running bzip2's self-test: it
compresses and decompresses the sample files and compares each result with `cmp`, from
[diffutils](#port-diffutils). A root without `cmp` fails this port's build.

What `build.sh` does after `make install`, and why each step is there:

- It installs `libbz2.so.<version>` and its two links, `libbz2.so.1.0` (the soname the library
  records) and `libbz2.so`, which the makefile's install leaves out.
- It replaces the statically linked `bzip2` the install copied with `bzip2-shared`, the one
  linked against `libbz2.so`, and makes `bunzip2` and `bzcat`, which the install wrote as copies,
  links to it.
- It writes `bzip2.pc`. The release ships no pkg-config file, and a build that looks for bzip2
  through pkg-config reads this one.
- It moves the manual pages from `/usr/man`, where the makefile puts them, to `/usr/share/man`.
- It remakes `bzegrep`, `bzfgrep`, `bzless` and `bzcmp` as relative links. The makefile creates
  them as links to `$(PREFIX)/bin/…`, which here is a path inside the staging directory, and would
  point nowhere once the package is installed.
