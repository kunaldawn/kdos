# kinstall

kinstall is the KDOS [installer]{.idx}: the terminal wizard that partitions a disk, copies the system onto
it and writes the boot loader. It is the last step of the bootstrap, compiled by hand the same way
as [kpkg](120-kpkg.md), so that every tree from this phase onward, and every image made from one,
has an installer. `130_kinstall.sh` builds it from `src/system/kdos-installer` with the cross
compiler and installs it as `/usr/bin/kinstall`.

**Approximate build time:** 0.2 SBU

## What it is built from

kinstall links no library at all beyond musl, not even ncurses. Its sources are:

- every `.c` file in `src/system/kdos-installer`, by glob;
- `catalogue.c` from `src/system/kdos-appbox`, the reader for the application catalogue. The
  installer runs before anything it could call exists, so it reads the catalogue itself, and the
  file needs nothing from `libkbase` the installer does not already carry. Its header lives beside
  it, which is why that directory is on the include path;
- three of KDOS's libraries, as source: `libkbase`, `libktui`, the terminal toolkit the wizard is
  drawn with, and `libkcolor`, which holds the palette numbers `libktui`'s theme code includes.

Each of the three keeps [the constraint](../../kdos/05-developer/c-libraries.md#the-constraint) of
linking nothing but the C library. A library on this line that needed a real `-l` would move the
installer to a package phase and take it off every tree before that phase.

## Installation of kinstall

The step names its sources and its output:

<!-- kfs:include script/phases/10_bootstrap/130_kinstall.sh lines=34-36 pin=aa7be7b34ddb -->

and compiles everything in one invocation:

<!-- kfs:include script/phases/10_bootstrap/130_kinstall.sh lines=51-62 pin=19e6fba6a704 -->

The flags are those of [kpkg's compile line](120-kpkg.md#the-compile-line), for the same reason:
no later phase rebuilds the binary, so the line carries the tree's hardening. The include paths are
the three libraries, the installer's own directory and `src/system/kdos-appbox` for
`catalogue.c`'s header.

Every `.c` file in `src/system/kdos-installer` is compiled by glob, never by a list of names. The
port recipe beside the sources builds the same program from the same directory, and the two must
compile the same files; a name list in either place would miss the next file added on one side
only, and the two installers would stop being the same program.

`install -Dm755` places the result at `/usr/bin/kinstall`. The step touches
`build/mark/bootstrap/kinstall` and is skipped while that file exists. Unlike kpkg's, this mark
is empty, so a change to the installer is built only when a build plan names the step; see
[Rebuilding one thing](../../kdos/05-developer/developing.md#rebuilding-one-thing).

## Contents of kinstall

**Installed program:** kinstall

## Built again later

Not by the build. `src/system/kdos-installer` has a recipe, the
[kdos-installer](#port-kdos-installer) port, so that a running KDOS can rebuild the installer
natively, but no phase list names it: the kinstall on every image is this step's binary. The port
compiles the same sources by the same globs, with native `gcc`, and takes its flags from the
chroot's [release flags](../../kdos/05-developer/writing-ports.md#the-release-flags) (`$CFLAGS`
before its own options and `$LDFLAGS` after the sources) instead of this step's fixed line. It
depends on `sysvinit`.

## See also

- [kpkg](120-kpkg.md), the step before this one, and [what you have](what-you-have.md) when the
  phase ends
- [kinstall](../../kdos/04-programs/kinstall.md): the installer's pages, steps and answer files
- [libktui](../../kdos/05-developer/c-libraries.md#libktui): the toolkit the wizard is drawn with

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 29.* Previous: [28. kpkg](120-kpkg.md) · [Contents](../README.md) · Next: [30. What the bootstrap leaves](what-you-have.md)
