# Ncurses

ncurses is the terminal library: it knows, from the terminfo database, how to move the cursor and
draw on every kind of terminal. readline and bash link it in the next steps, and every full-screen
program after them. The step builds it twice from one source: first a `tic` for the container, then
the library for KDOS. This page shows the commands of `script/phases/10_bootstrap/040_ncurses.sh`
and explains both halves.

**Source:** the archive of the [ncurses](#port-ncurses) port\
**Installs into:** `build/fs/usr`, and one program into `build/cross/bin`\
**Approximate build time:** 1.0 SBU

## A tic for the container

<!-- kfs:include script/phases/10_bootstrap/040_ncurses.sh lines=24-32 pin=c0b98a87c643 -->

`tic` compiles terminal descriptions into the terminfo database, and the install of the KDOS build
below runs it to write that database into `build/fs`. The `tic` the KDOS build compiles cannot run
in the container, and a `tic` from another ncurses might write a database format this one does not
read. So the first configure, with no `--host`, builds for the container: only the generated
headers (`make -C include`) and `tic` itself (`make -C progs tic`), with no library to install.

`tic` is copied into `build/cross/bin`, which `host.env` puts first on `PATH`. When the second
configure looks for a `tic` to run, as a cross build of ncurses does, this is the one it finds.

## Installation of ncurses

<!-- kfs:include script/phases/10_bootstrap/040_ncurses.sh lines=34-50 pin=81b17a06dbbe -->

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(../config.guess)`, `--prefix=/usr`
: A cross build for KDOS, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--mandir=/usr/share/man`
: Put the manual pages where the rest of the system's are.

`--with-manpage-format=normal`
: Install the manual pages uncompressed. Left to itself, configure guesses the format from the
  container's existing pages.

`--with-shared`, `--without-normal`
: Build shared libraries and no static ones. Nothing in the bootstrap links ncurses statically.

`--with-cxx-shared`
: Build the C++ binding as a shared library too.

`--without-debug`
: Build no debugging variants of the libraries.

`--without-ada`
: Build no Ada binding, and do not look for an Ada compiler.

`--enable-widec`
: Build the wide-character library, which handles UTF-8. Its files carry a `w`: `libncursesw.so`,
  `libformw.so`, `libmenuw.so`, `libpanelw.so`. readline is linked with `-lncursesw` in step 070 for
  that reason; this step makes no `libncurses.so` without the `w`.

`--disable-stripping`
: Do not strip the programs during install. The install would run the container's `strip` on
  programs compiled for KDOS.

## Contents of ncurses

**Installed programs:** [`clear`]{.idx}, [`infocmp`]{.idx}, [`tabs`]{.idx}, [`tic`]{.idx}, [`toe`]{.idx}, [`tput`]{.idx}, [`tset`]{.idx}, and the links
[`captoinfo`]{.idx}, [`infotocap`]{.idx} and [`reset`]{.idx}; [`ncursesw6-config`]{.idx}.

**Installed libraries:** [`libncursesw.so`]{.idx}, [`libformw.so`]{.idx}, [`libmenuw.so`]{.idx}, [`libpanelw.so`]{.idx} and
[`libncurses++w.so`]{.idx}.

**Installed directories:** the headers in `/usr/include`, and the terminfo database in
`/usr/share/terminfo`, with `/usr/lib/terminfo` a link to it.

The [ncurses](#port-ncurses) port's page describes each program.

## Built again later

The [ncurses](#port-ncurses) port is built in `20_selfhost`, as a dependency of readline and gawk.
It builds natively, so it needs no `tic` of its own first, and it differs in these switches:

- It adds `--enable-pc-files` and `--with-pkg-config-libdir=/usr/lib/pkgconfig`, so the libraries
  come with `pkg-config` files, and `--disable-pkg-ldflags`, so those files carry no build-time
  linker flags.
- It leaves out `--without-normal`, so the static libraries are built too. It leaves out
  `--with-manpage-format=normal` and `--disable-stripping`, which guard against the container's
  pages and the container's `strip`, and `--without-ada`, which changes nothing where there is no
  Ada compiler: KDOS's GCC builds none.
- After the install it links the names without the `w`, `libncurses.so`, `libform.so`,
  `libpanel.so`, `libmenu.so` and their `.pc` files, to the wide libraries, and `libcurses.so` to
  `libncurses.so`, so that a build asking for `-lncurses` or `-lcurses` gets the wide library.

## See also

- [Part III](intro.md): the bootstrap
- [Readline](070-readline.md): the first library built against this one
- [Bash](080-bash.md): the shell that links it

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 12.* Previous: [11. libstdc++](030-libstdcxx.md) · [Contents](../README.md) · Next: [13. Xz](050-xz.md)
