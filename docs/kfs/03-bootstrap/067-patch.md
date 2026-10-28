# Patch

GNU patch applies a diff to a tree of files. The bootstrap root needs it because a recipe's
`build.sh` applies its port's patches itself, and from `20_selfhost` on that `build.sh` runs inside
`build/fs` with nothing but this root's tools. `067_patch.sh` cross-compiles patch from the same
source tarball as the [patch](#port-patch) port and installs it into `build/fs`.

**Approximate build time:** 0.2 SBU

## Installation of patch

The step unpacks the port's tarball into `build/tmp` with `extract_port_source`, which reads the
recipe in `ports/core/base/patch/` and prints the path of the unpacked directory. It then
configures, compiles and installs from inside that directory:

<!-- kfs:include script/phases/10_bootstrap/067_patch.sh lines=23-33 pin=af6bbf5a65e6 -->

`make DESTDIR=$SYSROOT install` places every file under `build/fs` at the path it will have when
`build/fs` is the root, so `--prefix=/usr` puts the program at `build/fs/usr/bin/patch`. Nothing
records that the file was installed: no package owns it until `30_foundation` installs the port.

### The meaning of the configure options

`--host=$KDOS_TARGET`
: The system the program will run on, `x86_64-kdos-linux-musl`. configure looks for the compiler
  under that name, `x86_64-kdos-linux-musl-gcc`, and finds the cross compiler from
  [Part II](../02-cross/intro.md#the-environment) in `build/cross/bin`, which `host.env` puts on
  `PATH`.

`--build=$(build-aux/config.guess)`
: The machine doing the compiling, as patch's own copy of `config.guess` names it. With the build
  machine and the host both named and different, configure treats the build as a cross build from
  the start and never tries to run a program it compiled. `060_gzip.sh` to `068_sed.sh` all pass
  it this way.

`--prefix=/usr`
: The program goes to `/usr/bin` and its manual page under `/usr/share/man`.

Nothing else is passed. The root holds no `libattr` at this point, so configure builds patch
without extended-attribute support.

When the install is done the step removes the source directory and touches
`build/mark/bootstrap/patch`. While that file exists the step exits at once without building,
unless a build plan names it; see [Marks and KDOS_REPLAY](intro.md#marks-and-kdos_replay).

## Built again later

`30_foundation` installs the [patch](#port-patch) port over this copy. kpkg adopts the file this
step left (see [Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file)), and from
then on the program belongs to the package. The port build differs in three ways:

- It is a native build inside the chroot with KDOS's own GCC, so it passes no `--host` or
  `--build`, and it is compiled with the chroot's
  [release flags](../../kdos/05-developer/writing-ports.md#the-release-flags) rather than the plain
  `-O2 -pipe -std=gnu99` that `script/env/host.env` gives this step.
- It passes `--enable-xattr` and depends on `attr`, so [`patch`]{.idx} keeps a file's extended attributes
  when it rewrites the file.
- It names `--sysconfdir=/etc`, `--mandir=/usr/share/man` and `--localstatedir=/var` explicitly.

## See also

- [Grep](066-grep.md), the step before this one, and [Sed](068-sed.md), the step after it
- [The patch port](#port-patch): its version, source and contents
- [A minimal KDOS](../../kdos/05-developer/how-kdos-is-built.md#a-minimal-kdos-10_bootstrap): the
  phase as a whole

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 21.* Previous: [20. Grep](066-grep.md) · [Contents](../README.md) · Next: [22. Sed](068-sed.md)
