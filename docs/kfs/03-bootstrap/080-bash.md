# Bash

Bash is the GNU shell. Every recipe's `build.sh` is a bash script, kpkg runs it with bash, and
configure scripts run under `/bin/sh`, which on KDOS is bash too. `080_bash.sh` cross-compiles bash
from the [bash](#port-bash) port's source tarball, links it against the [readline](070-readline.md)
and [ncurses](040-ncurses.md) already in `build/fs`, and makes `sh` a link to it.

**Approximate build time:** 0.5 SBU

## Installation of bash

The step unpacks the port's tarball and builds in a `build` directory inside it:

<!-- kfs:include script/phases/10_bootstrap/080_bash.sh lines=24-46 pin=d5499a5633a8 -->

As with readline, only the tarball is unpacked: the upstream patch series beside the recipe
(`bash<ver>-NNN`, numbered from 001) is not applied to this build.

### The meaning of the configure options

`--host=$KDOS_TARGET`
: Build for KDOS with the cross compiler; the build machine is guessed by bash's own
  `config.guess`, and configure concludes it is not cross-compiling (see
  [Part III](intro.md#the-shape-of-a-step)).

`--prefix=/usr`
: The shell is installed as `/usr/bin/bash`. `/bin` is a link to `usr/bin`, so `/bin/bash` names
  the same file.

`--with-curses`
: Take the terminal-capability functions from the curses library, ncurses here, rather than from
  a separate termcap library.

`--enable-readline`
: Build the interactive line editor in. It is bash's default; the switch makes the dependency on
  the previous step explicit.

`--with-installed-readline`
: Link the readline in `/usr/lib` instead of compiling the copy bash carries in `lib/readline`.

`--without-bash-malloc`
: Use musl's `malloc` rather than the allocator bash ships, so the shell allocates memory the way
  every other program in the root does.

`--disable-nls`
: Build no message catalogues. The bash port builds them and kpkg prunes them from its package,
  so a copy installed here would be adopted by no package and outlive every install.

### Removing the HTML manuals

The `rm` deletes `bash.html` and `bashref.html`, which the install writes under
`/usr/share/doc/bash`. kpkg prunes HTML manuals from every package (see
[Size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy)), so
the bash port never installs them again, and these copies would be adopted by no package (see
[Part III](intro.md#what-the-bootstrap-leaves-behind)).

### The `sh` link

`ln -sv bash sh` in `build/fs/usr/bin` makes `/usr/bin/sh`, and through the merged `/bin` link also
`/bin/sh`, run bash. A `#!/bin/sh` script and a configure script both reach it.

The step then removes the source and touches `build/mark/bootstrap/bash`.

## Built again later

No phase before `30_foundation` lists bash. That phase builds the [bash](#port-bash) port, which
adopts [`bash`]{.idx} and the `sh` link left here. The port build:

- applies the upstream patch series in order before configuring;
- adds `-DSYS_BASHRC='"/etc/bash.bashrc"'` to `CFLAGS`, so an interactive shell reads the
  system-wide `/etc/bash.bashrc`. An interactive bootstrap shell reads no system-wide bashrc;
- passes the same four switches as this step, without `--disable-nls`;
- installs `bash` and the `sh` link under `/bin`, the same directory as `/usr/bin` on the merged
  layout, and is compiled with the chroot's
  [release flags](../../kdos/05-developer/writing-ports.md#the-release-flags).

## See also

- [Readline](070-readline.md), the step before this one, and [Binutils](090-binutils.md), the step
  after it
- [The bash port](#port-bash): its version, source and contents
- [What a port build is](../../kdos/05-developer/how-kdos-is-built.md#what-a-port-build-is): how
  kpkg runs a `build.sh` with this shell

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 24.* Previous: [23. Readline](070-readline.md) · [Contents](../README.md) · Next: [25. Binutils](090-binutils.md)
