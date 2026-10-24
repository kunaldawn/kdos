## About

Expat is a stream-oriented XML parser in C: it calls back into the program for each element and
piece of text as it reads, and builds no tree. `wayland-scanner` reads the protocol XML of every
Wayland interface with it, so the compositor, the panel and every Wayland client are built through
it; dbus reads its bus configuration with it, fontconfig its `fonts.conf`, and mesa its driver
configuration. Python's `pyexpat`, the parser under `xml.etree`, is linked against it, and so are
cmake, git, polkit, libarchive and Prosody's [lua54-luaexpat](#port-lua54-luaexpat). `xmlwf`,
installed with it, checks that a file is well-formed XML.

## Flags

- `./buildconf.sh` — Runs upstream's autotools bootstrap, which regenerates `configure` and every
  `Makefile.in` with the tree's autoconf, automake and libtool, the three ports in `depends`.
- `--with-getrandom` — Takes the hash salt from `getrandom()`, which musl has. Expat salts its
  internal hash tables with random bytes, which upstream documents as a defence against denial of
  service by a document built to predict the hash function. A `--with-` form makes configure stop
  when the function is missing, rather than quietly pick another source.
- `--with-getentropy` — Allows `getentropy()` as well, which musl also has; the same rule holds.
- `--without-arc4random` — Skips the check for `arc4random()`, which musl does not provide.
- `--without-arc4random-buf` — Skips the check for `arc4random_buf()`, for the same reason.
- `--without-docbook` — Installs the `xmlwf.1` the release tarball ships instead of rendering it
  again from `doc/xmlwf.xml`. Left to check, configure renders the page with the first of
  `docbook2x-man`, `db2x_docbook2man`, `docbook2man` and `docbook-to-man` it finds, so the page would depend on what
  happens to be in the build root (see
  [An optional tool found in the tree](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#an-optional-tool-found-in-the-tree)).
- `--without-examples` — Does not build the example programs, which upstream never installs.
- `--without-tests` — Does not build the test suite, which the recipe does not run.

## Notes

Expat is among the first ports `30_foundation` builds: python3 and cmake, in the phase's opening
run, depend on it, so the build order puts it ahead of them.
