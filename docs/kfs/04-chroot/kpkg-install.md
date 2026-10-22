# Anatomy of a kpkg install

From `20_selfhost` on, every package of KDOS is built and installed by one command run in the
chroot, `kpkg install <port>`, about two thousand times over. This chapter follows that command
once, from start to finish, with a real port: [pv](#port-pv), a small tool that shows the progress
of data through a pipe. It shows the port's two files, what kpkg decides before it builds anything,
each of the steps that turn the source into a package, how kpkgadd installs that package, and the
entry the package database is left with. Every package page in the book stands for these steps.

## The port

`pv` is filed on the `cli` shelf, so its port is the directory `ports/core/cli/pv/`, which in the
chroot is `/ports/core/cli/pv`. git tracks two files in it. The first is the recipe, [`kpkgbuild`]{.idx},
metadata that kpkg parses and never runs:

<!-- kfs:include ports/core/cli/pv/kpkgbuild -->

The second is [`build.sh`]{.idx}, the only part of a port that is a shell script:

<!-- kfs:include ports/core/cli/pv/build.sh -->

The third file in the directory is the source itself, `pv-1.12.0.tar.gz`, which `make fetch` put
there and git does not track; see [Sources and the archive](../01-preparing/sources.md).

`pv` is installed by `41_system`, whose list for the `cli` shelf names it, in
`script/phases/41_system/packages.d/cli.txt`. When that phase runs, kdosbuild makes it one step,
and the step's command, run from the top of the repository in the container, is the one its
package page gives:

```bash
script/chroot/exec.sh bash -c 'source script/phases/41_system/phase.env && kpkg install pv'
```

[Entering the chroot](entering.md) explains the first half. What follows is the second: `kpkg
install pv`, run as root in `/kdos` inside the chroot, with the `41_system` environment loaded.

## What kpkg prints

Before the detail, the outline. These are the lines kpkg itself prints for `pv`, each starting
`==>`, in the order a serial `kpkg install pv` prints them; the build's own output, the configure
checks and compiler lines, falls between `Building pv-1.12.0-1...` and `Creating package...` and
is left out here:

```text
==> Resolving dependencies...
==> Packages to install: pv
==> Building pv...
==> Preparing...
==> Extracting pv-1.12.0.tar.gz with stripping...
==> Refreshed 2 config.guess/config.sub from automake (2026-05-17)
==> Building pv-1.12.0-1...
==> Creating package...
==> Checking the staged tree...
==> Packaged pv: 0 MB in 0.0 s
==> Cleaning up...
==> Package created: /var/cache/kpkg/packages/pv-1.12.0-1.tar.xz
==> Installing pv-1.12.0-1
==> Installing usr/bin/pv -> //usr/bin/pv
==> Installing usr/share/doc/pv/ACKNOWLEDGEMENTS.md -> //usr/share/doc/pv/ACKNOWLEDGEMENTS.md
==> Installing usr/share/doc/pv/COPYING -> //usr/share/doc/pv/COPYING
==> Installing usr/share/doc/pv/DEVELOPERS.md -> //usr/share/doc/pv/DEVELOPERS.md
==> Installing usr/share/doc/pv/INSTALL -> //usr/share/doc/pv/INSTALL
==> Installing usr/share/doc/pv/NEWS.md -> //usr/share/doc/pv/NEWS.md
==> Installing usr/share/doc/pv/README.md -> //usr/share/doc/pv/README.md
==> Installing usr/share/man/man1/pv.1 -> //usr/share/man/man1/pv.1
==> index skipped: man
==> Package 'pv' installed successfully
==> Removing cached package /var/cache/kpkg/packages/pv-1.12.0-1.tar.xz...
```

All of it goes to the step's log, `build/logs/41_system/<NNNN>_pv.install.log`. In the log, kpkg's
own lines can come after the build output they introduce: kpkg buffers its standard output when it
is not writing to a terminal, and the programs the build runs write theirs directly.

## Deciding: resolution and the recipe hash

`Resolving dependencies...` asks the same solver `kpkgdepends` uses for the install order of `pv`
and everything it depends on, leaving out every port that is already installed and current.
**Current** means that the hash of the port's recipe as it stands now equals the hash recorded when
the package was installed. The phase environment sets `KPKG_STRICT_RECIPE=1`, which turns that
comparison on; without it, as on a running system, an installed package counts as current whatever
its recipe says.

`pv` depends on [`ncurses`]{.idx}, which `20_selfhost` installed long before and whose recipe has not
changed, so `ncurses` is dropped and the order is `pv` alone: `Packages to install: pv`. On a tree
where `pv` itself is installed and current, the order is empty and kpkg prints `Nothing to do`. The
same command with the database hidden shows the whole order instead, as
[Entering the chroot](entering.md#a-shell-in-the-chroot) shows.

The **[recipe hash]{.idx}** is a SHA-256 over `kpkgbuild`, `build.sh`, any `postinstall.sh` and every
patch, and over every other file in the port directory that no `sha256 =` line names, except the
port's note for this book, `kfs.md` (see
[the recipe hash](../../kdos/03-architecture/packaging.md#e--the-recipe-hash)). The source tarball
is not hashed again, because its own line already pins it. For `pv`, that is the two files
above. Editing either is therefore enough for the next build to rebuild the port without being
told to, and editing nothing is enough for it to be skipped. kpkg takes the hash before it starts
building and records it only after the install succeeds, so a build that fails part-way records
nothing, and an edit made during the build is seen as a change on the next run. The rules, and
what a port of KDOS's own hashes, are in
[Deciding what to rebuild](../../kdos/03-architecture/packaging.md#deciding-what-to-rebuild).

With `KDOS_PKG_STORE` on, kpkg also computes the port's key in the [package store]{.idx} at this point, and
installs a stored package with that key in place of everything up to [Adopting](#adopting-kpkgadd)
below, printing `Reusing pv from the store` with the start of the key.

`Building pv...` starts the build proper. `kpkg install -f pv` would print `Rebuilding pv
(forced)...` instead, and build it whether it is current or not; only the ports named on the
command line are forced, never their dependencies.

## Building

kpkg now works in the port directory, `/ports/core/cli/pv`. It parses `kpkgbuild` again, refuses a
recipe with no `name`, `version` or `release` or with no `build.sh` beside it, and sets the umask to
022, so that every file the build creates without an explicit mode gets the same mode on every
machine.

### Verify

Every file a `sha256 =` line names is hashed, if it is in the port directory or in kpkg's own
source directory, `/var/cache/kpkg/sources`, before anything is unpacked. A mismatch stops the
build with `sha256 MISMATCH for <file>` and both hashes. For `pv` this is the equivalent of:

```bash
cd /ports/core/cli/pv
echo '31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8  pv-1.12.0.tar.gz' | sha256sum -c -
```

A source that the recipe names but gives no hash for is refused when it is about to be unpacked,
with `No sha256 for <file> in the recipe`. See
[Checksums](../../kdos/05-developer/writing-ports.md#checksums).

### Unpack

`Preparing...` empties the port's work directory and creates the two directories the build uses:

```bash
rm -rf /var/cache/kpkg/work/pv
mkdir -p /var/cache/kpkg/work/pv/pv-1.12.0 /var/cache/kpkg/work/pv/pkg
```

`Extracting pv-1.12.0.tar.gz with stripping...` unpacks the first source into the first of them,
dropping the archive's own top directory:

```bash
tar --no-same-permissions --warning=no-unknown-keyword \
    -xf pv-1.12.0.tar.gz -C /var/cache/kpkg/work/pv/pv-1.12.0 --strip-components=1
```

`--no-same-permissions` makes tar apply the umask to every mode in the archive, which as root it
otherwise would not, so a world-writable file in an upstream tarball does not become one in the
package. `--warning=no-unknown-keyword` silences tar about the extra header fields that archives
made by `git archive` or `bsdtar` carry. A second tarball, in a recipe with more than one, would be
unpacked unstripped into `/var/cache/kpkg/work/pv`, and a source that is not a tarball would be
copied into the source directory as it is.

`Refreshed 2 config.guess/config.sub from automake (2026-05-17)` comes next. Every `config.guess`
and `config.sub` in the unpacked source older than automake's own is replaced with automake's,
because an old one does not know the `musl` target and names the machine as a glibc one. Age is
judged by the `timestamp=` line each script carries, and the date printed is that of automake's
copies. `pv`'s tarball carried two older ones.

### Build

`Building pv-1.12.0-1...` runs `build.sh`. kpkg hands bash a short script of its own, which sets
the recipe's values as shell variables and then sources `build.sh` in a subshell under `set -e`.
For `pv` that script is:

```bash
[ -f "$KPKG_CONF" ] && . "$KPKG_CONF" || true
name='pv'
version='1.12.0'
release='1'
source='https://codeberg.org/ivarch/pv/releases/download/v1.12.0/pv-1.12.0.tar.gz'
sha256='31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8  pv-1.12.0.tar.gz'
PKGNAME=pv-1.12.0-1.tar.xz
PORT_SRC=/ports/core/cli/pv
SRC_ROOT=/var/cache/kpkg/work/pv
SRC=/var/cache/kpkg/work/pv/pv-1.12.0
PKG=/var/cache/kpkg/work/pv/pkg
cd "$SRC"
( set -e; . "$PORT_SRC/build.sh" )
```

`KPKG_CONF` is kpkg's configuration file, `/etc/kpkg.conf`. The five values from `name` to `sha256`
are the recipe's own, as `kpkg meta pv` prints them, expanded and single-quoted; a recipe's helper
variables would follow them. `PORT_SRC` is the port directory, `SRC` the unpacked source and the
working directory, and `PKG` the empty staging directory the build installs into. They are shell
variables, not exported ones, so the programs the build runs do not see them.

`set -e` stops the build at the first command that fails. There is no `set -o pipefail` and no
`set -u`: a failure in the middle of a pipeline is not caught, and an unset variable is empty. See
[build.sh](../../kdos/05-developer/writing-ports.md#buildsh).

`pv`'s own `build.sh` is the ordinary three steps of an autotools package. The environment the
phase loaded supplies the compiler (`CC=gcc`), the release flags in `CFLAGS` and `LDFLAGS`, and the
job count in `MAKEFLAGS`, so none of them appears in the recipe.

#### The meaning of the configure options

`--prefix=/usr`
:   Install under `/usr`, as every package of KDOS does. Configuration that an autotools package
    would put under `/usr/etc` and `/usr/var` is placed in `/etc` and `/var` by the site file the
    chroot's environment names in `CONFIG_SITE`.

`--disable-static`
:   In `pv`'s configure, `--enable-static` adds `-static` and links the program statically. The
    switch keeps the program linked dynamically against the C library and ncurses.

`--with-ncurses`
:   Look up the terminal's size and capabilities through ncurses, the one dependency the recipe
    names. The link line in the log ends in `-lncurses`.

`--disable-nls`
:   Build no message catalogues. KDOS ships English only; kpkg removes every non-English catalogue
    from a package anyway, and this saves building them.

`make DESTDIR=$PKG install`
:   Install into the staging directory rather than into the live root. Nothing a build does touches
    the root itself; the files reach it only when kpkgadd installs the package.

### Package

`Creating package...` turns the staging directory into a package. For every port, kpkg first does
the same tidying, so that no recipe has to:

- It adds a `postinstall.sh` from the port directory, when there is one, as `.POSTINSTALL` at the
  top of the staging tree. `pv` has none.
- It deletes libtool's `.la` files, the info directory index `usr/share/info/dir`,
  `usr/share/applications/mimeinfo.cache` and every `fonts.dir`. Each is either wrong on the
  target or an index over every package's files, which kpkgadd rebuilds from what is installed.
- It copies the licence files from the top of the source tree (`LICENSE`, `COPYING`, `NOTICE` and
  the like) into `usr/share/licenses/<name>/`. `pv` keeps its `COPYING` under `docs/`, not at the
  top, so it gets no such directory; its own `make install` puts the file in `usr/share/doc/pv/`.
- It removes HTML and PDF manuals and every non-English message catalogue, handbook and manual page;
  see [Size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy).
- It strips the symbol table from any executable or shared library over 32 MiB, and compiles the
  bytecode of any Python modules the package ships.

`Checking the staged tree...` is the package lint, which reads the tree that is about to be
archived and refuses the package for a fault no later step could repair: a file outside the
directories a root has, a mode that is wrong for its place, a broken `.pc` file, an executable stack
or an RPATH into the build directories. See
[The package lint](../../kdos/03-architecture/packaging.md#the-package-lint).

The staging tree is then rolled into the package, with a command that makes the same tree give the
same bytes on any machine:

```bash
tar --sort=name --format=gnu --numeric-owner --owner=0 --group=0 \
    --mtime=@$SOURCE_DATE_EPOCH \
    --use-compress-program='xz -0 -T0 --block-size=8MiB --no-adjust' \
    -cf /var/cache/kpkg/packages/pv-1.12.0-1.tar.xz.part -C /var/cache/kpkg/work/pv/pkg .
```

`--sort=name` fixes the order of the entries, which a directory listing does not. `--owner=0`,
`--group=0` and `--numeric-owner` record every file as root's, by number. `--mtime` gives every
entry the time in `SOURCE_DATE_EPOCH`, which the phase environment fixes. `--format=gnu` holds long
paths without the clock times a pax header would add. The compressor and its settings are fixed,
and kpkg removes `XZ_OPT` and `XZ_DEFAULTS` from the environment while it runs. A package that is
installed and then deleted, as every package of an ordinary build is, is compressed with `xz -0`;
one that is kept, with `KDOS_MAKE_BINHOST=1`, with `xz -9 --block-size=32MiB` instead. The archive
is written to a `.part` file and renamed only when tar succeeds, so a package under its final name
is always complete. See
[Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages).

`Packaged pv: 0 MB in 0.0 s` reports the staged size and the time the roll took; `pv` stages less
than a megabyte. `Cleaning up...` removes `/var/cache/kpkg/work/pv` again.

## Adopting: kpkgadd

kpkg hands the package to kpkgadd, the part of kpkg that installs a built package. In order, it:

1. Checks the package's signature when a `.sig` file lies beside it. A locally built package has
   none, which is allowed; a signature that does not verify stops the install.
2. Reads the name, version and release back from the file name, and prints
   `Installing pv-1.12.0-1`, or `Upgrading` with both versions when an older `pv` is installed.
3. Unpacks the package into a staging directory under `/var/tmp`, on the root's own filesystem so
   that placing a file is a rename, with `tar -xp`, so that every mode, setuid and setgid bits
   included, comes out as packed.
4. Takes the package database's lock, `/var/lib/kpkg/db/.lock`, and checks every path in the
   package against every other package's: a path another package owns is refused, with
   `File conflict detected`, unless the recipe names that package in `replaces =`. See
   [Who owns a file](../../kdos/03-architecture/packaging.md#who-owns-a-file).
5. Places every file, printing one `Installing <path> -> <destination>` line each, and on an upgrade
   removes the files the old version had and the new one does not.
6. Writes the database entry, the package file's hash, and after any `.POSTINSTALL` hook has run,
   the record of each placed file's hash and mode.
7. Rebuilds the shared indexes the package's paths feed, such as the GSettings schemas, the MIME
   database, the font cache or the manual index. `pv` installs a manual page, so the manual index
   is due; the build sets `KPKG_SKIP_INDEX=man`, because `70_image` writes that index from scratch,
   and the line says `index skipped: man`. See
   [Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes).

`Package 'pv' installed successfully` ends the install. Back in `kpkg install`, the recipe hash
taken before the build is recorded, the package is copied into the package store when the store is
on, and the package file is deleted: `Removing cached package`.

## The database entry

What is left of the whole operation, besides the files in the root, is four small files under
`/var/lib/kpkg/db/`. In the chroot:

```bash
cat /var/lib/kpkg/db/pv
```

```text
1.12.0 1
./
./usr/
./usr/bin/
./usr/bin/pv
./usr/share/
./usr/share/doc/
./usr/share/doc/pv/
./usr/share/doc/pv/ACKNOWLEDGEMENTS.md
./usr/share/doc/pv/COPYING
./usr/share/doc/pv/DEVELOPERS.md
./usr/share/doc/pv/INSTALL
./usr/share/doc/pv/NEWS.md
./usr/share/doc/pv/README.md
./usr/share/man/
./usr/share/man/man1/
./usr/share/man/man1/pv.1
```

The first line is the installed version and release; every line after it is a path the package
contains, directories with a trailing `/`. This is the file kpkg reads to know that `pv` is
installed and what it owns, and `kpkg info pv` counts its sixteen paths as `Files: 16`.

`/var/lib/kpkg/db/.files/pv` holds, for each file placed, its SHA-256, its mode and its path, which
`kpkg check pv` compares against the disk:

```text
c9ecd94f5c0b0689776babee0ec2748093d983be77f2ff0d9d1dfe39728ea146 0755 ./usr/bin/pv
3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986 0644 ./usr/share/doc/pv/COPYING
e3389c24af07398e6a79a3f210743abf4dc74fc21d72340be9b7b947ecab3529 0644 ./usr/share/man/man1/pv.1
```

(three of its eight lines). `/var/lib/kpkg/db/.recipe/pv` holds the recipe hash the package was
built from, the value the next build compares the port against:

```text
621123df9c942d44aa50a89dc543033a5d8f4aa21d023f5fc035d69e299eb903
```

and `/var/lib/kpkg/db/.pkgsha/pv` the SHA-256 of the package file itself, which the package store
keys every port that depends on `pv` with. These values are those of one build of the tree. A
package rebuilt from the same recipe, on the same tree and with the same compressor settings, is
meant to reproduce every one of them, which is what makes them worth recording. The layout of every file kpkg keeps is in
[Files kpkg keeps to itself](../../kdos/03-architecture/packaging.md#files-kpkg-keeps-to-itself).

## Building in levels

Under `make build BUILD_ARGS="--port-jobs N"` a package phase builds several ports at once, and the
same install is split in two. `kpkg install --build-only pv` runs everything up to the end of
[Package](#package), keeps the package file, and prints `pv: built` with its path; a dependency that
is not installed and current is an error there, never a build. Later, once every port of the level
is built, one step runs `kpkg install --commit` over the whole level, which runs
[Adopting](#adopting-kpkgadd) and the recording for each in turn. A port's log then holds the build
and the level's `L<n>.commit.log` holds the install. See
[Building apart from installing](../../kdos/03-architecture/packaging.md#building-apart-from-installing).

## See also

- [What a port build is](../../kdos/05-developer/how-kdos-is-built.md#what-a-port-build-is): the same walk through `pv` in the KDOS book
- [What kpkg does around build.sh](../../kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh): the contract a recipe is written against
- [Packaging](../../kdos/03-architecture/packaging.md): ports, packages, the solver and the recipe hash
- [Reading a package page](../00-front/reading-a-package-page.md): how a generated page sets out these steps for its port

<!-- book-nav -->
---

*Part IV. The chroot and kpkg, chapter 32.* Previous: [31. Entering the chroot](entering.md) · [Contents](../README.md) · Next: [Part V. Self-hosting](../05-selfhost/intro.md)
