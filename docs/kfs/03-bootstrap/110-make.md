# Make

GNU make reads a makefile and runs the commands that bring its targets up to date. Almost every
upstream build ends in a call to [`make`]{.idx}, including every port `20_selfhost` builds. `110_make.sh`
cross-compiles make from the [make](#port-make) port's source tarball and installs it into
`build/fs`, so the root has it once the container's own make is out of reach.

**Approximate build time:** 0.1 SBU

## Installation of make

The step unpacks the port's tarball and builds in a `build` directory inside it:

<!-- kfs:include script/phases/10_bootstrap/110_make.sh lines=24-36 pin=b9bf33298357 -->

### The meaning of the configure options

`--host=$KDOS_TARGET`
: Build for KDOS with the cross compiler. No `--build` is given, so configure decides for itself
  whether it is cross-compiling, and concludes it is not (see
  [Part III](intro.md#the-shape-of-a-step)).

`--prefix=/usr`
: The program goes to `/usr/bin/make`, its header `gnumake.h` to `/usr/include`.

`--disable-nls`
: Build no message catalogues.

`--without-guile`
: Build make without GNU Guile, the Scheme interpreter it can embed for `$(guile …)` in makefiles.
  The root has no Guile, and naming the switch keeps configure from looking for one.

The step then removes the source and touches `build/mark/bootstrap/make`.

## Built again later

`30_foundation` installs the [make](#port-make) port in its opening run of build tools, which
adopts `/usr/bin/make`. The port passes `--prefix=/usr` and `--without-guile` as this step does,
with two differences:

- It adds `-std=gnu99` to the chroot's [release flags](../../kdos/05-developer/writing-ports.md#the-release-flags),
  whose own standard is `-std=gnu11`. The bootstrap build already compiles as `gnu99`, the standard
  `script/env/host.env` sets.
- It passes no `--disable-nls`, so whether the packaged make carries translations is left to
  configure.

## See also

- [GCC](100-gcc.md), the step before this one, and [kpkg](120-kpkg.md), the step after it
- [The make port](#port-make): its version, source and contents
- [The phase environment](../../kdos/05-developer/build-system.md#the-phase-environment): the
  flags a host step and a chroot step each compile with

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 27.* Previous: [26. GCC](100-gcc.md) · [Contents](../README.md) · Next: [28. kpkg](120-kpkg.md)
