## About
muon is an implementation of the Meson build system in C99, with samurai, an implementation of
Ninja, built into the same binary. KDOS builds exactly one port with it: [pkgconf](#port-pkgconf).
Every other Meson port is built with [meson](#port-meson), which is written in Python. muon
bootstraps with a C compiler alone, so it can be installed before any `pkg-config` exists, and the
phase's opening run names it ahead of pkgconf for that reason.

The build bootstraps itself in two stages. `./bootstrap.sh stage1` compiles the whole of muon from
one amalgamated C file, `src/amalgam.c`, with the C compiler alone, into `stage1/muon-bootstrap`.
That binary then configures the real build as a Meson project, builds it with its own built-in
samurai (`samu`), and the result, `build/muon`, installs itself.

## Flags
- `-Dlibcurl=disabled`, `-Dlibarchive=disabled` — Build without the two libraries muon uses to
  download and unpack wraps, Meson's mechanism for fetching a missing dependency's source. The
  build has no network and passes `-Dwrap_mode=nodownload`, and both libraries are built later in
  the same phase, so `auto` would decide differently on a rebuild.
- `-Dlibpkgconf=disabled` — Find pkg-config dependencies by running a `pkg-config` program rather
  than by linking libpkgconf. libpkgconf is pkgconf's library, and pkgconf is built with muon.
- `-Dtracy=disabled` — Build without the Tracy profiler, which is not a port in the tree.
- `-Dmeson-docs=disabled` — Do not embed the Meson project's reference documentation, from the
  bundled `meson-docs` subproject, in the binary. Building it runs `python3`, and with `auto` the
  option turns on whenever `python3` is found: python3 is built later in the same run, so a muon
  rebuilt after it would differ. The manual pages, `muon(1)`, `meson.build(5)` and
  `meson-reference(3)`, are built and installed either way.
- `-Dmeson-tests=disabled` — Leave out the bundled `meson-tests` subproject, the Meson project's
  test cases that muon's own test suite runs. The recipe runs no tests.
- `-Dwebsite=disabled` — Do not build muon's website. Off is upstream's default.

## Notes
`meson.build(5)` is written in scdoc, which is why the recipe depends on [scdoc](#port-scdoc).
