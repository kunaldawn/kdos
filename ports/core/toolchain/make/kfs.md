## About
GNU make reads a makefile and runs the commands that bring its targets up to date. Nearly every
upstream build ends in it: an autotools port's `build.sh` runs `make` and
`make DESTDIR=$PKG install`, and CMake and qmake projects generate makefiles for it.

KDOS builds it twice. The bootstrap step `110_make.sh` cross-compiles it into `build/fs`, so
the root has a make before the first chroot phase; then this port, in the opening run of build
tools in `30_foundation`, rebuilds it with the chroot's own compiler and flags and adopts the
files the step installed. It is one of the build tools that recipes use without naming them in
`depends`, which is why that run is installed before anything else in the phase.

## Flags
- `--without-guile` — Build make without GNU Guile, the Scheme interpreter it can embed for
  `$(guile …)` in makefiles. Without the switch, configure looks for a `guile-3.0`, `2.2`,
  `2.0` or `1.8` pkg-config module and links whichever it finds. The tree has a
  [guile](#port-guile) port, built in `41_system`, so naming the switch keeps a later rebuild
  of make from linking it.
- `-std=gnu99` — Compile make as GNU C99. It comes after the chroot's `CFLAGS`, whose standard
  is `-std=gnu11` (see [The release flags](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-release-flags)), and the last `-std` on a
  GCC command line is the one that applies. The bootstrap build compiles make as `gnu99` too,
  the standard `script/env/host.env` sets.
