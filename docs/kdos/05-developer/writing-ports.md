# Writing ports

This chapter is the reference for the recipe format: how a piece of upstream software is described
so that KDOS can fetch it, verify it, build it offline and package it. A
[port](../06-reference/glossary.md) is the recipe for one host package. The tree holds 2,000 of them
under `ports/core`, filed on 102 shelves by subject, and the 24 ports of KDOS's own under `src/` are
written in the same format. The chapter is for anyone adding a port, changing one, or bumping one to a new
upstream release. Read [How KDOS is built](how-kdos-is-built.md) for where ports sit in the build,
[Developing](developing.md) for setting up a checkout and `make fetch`, and
[Packaging](../03-architecture/packaging.md) for what a package is and how one is verified.

The chapter moves from the format to the procedure to the special cases. The first sections describe
the two files of a port, the environment the build runs in and the canonical build shape for each
build system. A worked example and a step-by-step procedure for adding a port follow. After them
come the rules every recipe keeps and the topics a particular port may need: desktop entries, manual
pages, install hooks, shared indexes, vendored dependencies, parallel versions and second builds of
one source. The last sections cover checking a change, keeping ports current with upstream, and
publishing sources to the archive other checkouts fetch from. When a build fails, go to
[Build troubleshooting](build-troubleshooting.md), which is indexed by the message you see.

## Anatomy of a port

A port is a directory. Most hold two files you write and one file `make fetch` places there:

```text
ports/core/games-rpg/frotz/
├── kpkgbuild                  declarative metadata: parsed, never run
├── build.sh                   the build: ordinary bash
├── postinstall.sh             optional install-time hook
├── *.patch                    optional patches, applied by build.sh
└── frotz-2.55.tar.gz          the source: fetched, gitignored, not in git
```

`kpkgbuild` has no interpreter line and is never executed. `kpkg` parses it with a small reader
written in C (`src/system/kdos-kpkg/decl.c`), so a tool that reads a recipe starts no shell and
cannot run anything by accident. `build.sh` is a real script, so syntax checking, linting,
highlighting and diffing all work on it; `testing/preflight.sh` runs `bash -n` on every one. The
reasoning behind the two-file split is in [Decisions](../01-philosophy/decisions.md), and
[How KDOS differs](../01-philosophy/how-kdos-differs.md#packages-and-recipes) sets it beside other
distributions' recipe formats.

The upstream source is not in git. Its `sha256 =` line in the recipe is its identity, and
`make fetch` puts the file beside the recipe from a local cache, the KDOS source archive, or
upstream (see [Where sources come from](developing.md#where-sources-come-from)). After you add or
change a source, [publish it](#publishing-sources) so other checkouts can fetch it.

A few ports keep other files in git beside the recipe. Leaving out `kpkgbuild`, `build.sh`,
`postinstall.sh` and `*.patch`, git tracks 41 files under `ports/core`. Thirty-five of them are named
by a `sha256 =` line: small inputs that are part of the source, such as `bash`'s and `readline`'s
upstream patch-level files and the IANA registries under `iana-etc`. A named file cannot change
without its `sha256 =` line changing, so an edit to it changes the recipe and rebuilds the port.

Every other file beside the recipe is hashed into the
[recipe hash](../06-reference/glossary.md) itself, subdirectories included, so an edit to it
rebuilds the port too. The six such files in git are `linux/kdos.config`,
`linux/kdos-logo-mono.pbm` and `epy/epy.desktop`, which `build.sh` reads from `$PORT_SRC`;
`pandoc/cabal.project.freeze`, which `ports/fetch` reads when it generates the vendor bundle; and
`linux/genlogo-mono.py` and `ffmpeg/LICENSE.notice`, which no build reads. The rule has no
exceptions, so a stray file beside a recipe, such as an editor backup or a `kpkgbuild.new`, also
changes the hash and rebuilds the port.

### Shelves, and how a port is found

The `games-rpg` in the path is the port's **shelf**: the subject directory it is filed under. The
shelves are a closed list, `ports/shelves`, one line per shelf giving its id, the source-archive
volume that keeps its files, and what belongs on it;
[Choosing a shelf](#choosing-a-shelf) is how a new port is placed. A port's identity is its bare
name, never its shelf. `name =` equals the directory's name, one name is one port across every
shelf and every `src/` area, and nothing that names a port (a `depends` line, a package list,
`kpkg`, `ports/fetch`, `ports/publish`) spells the shelf. Moving a port to another shelf is one
`git mv ports/core/<old>/<name> ports/core/<new>/`: the recipe hash covers the port's files and not
their path, and the installed database, the binary host and `ports/sources.idx` are keyed by name or
by hash; the port's archived sources stay where they are, under the old shelf's name and in its
volume, where the index still finds them, until `ports/publish --rehome` moves them. The one exception is a `.gitignore` pattern for files generated inside one port's
directory, which spells the shelf (`/ports/core/graphics/digikam/*.jpeg`); moving such a port means
editing that line too, and `testing/preflight.sh` fails with "names a port directory that holds no
port" until it is. For the same reason, prose, comments and notes
name a port by its name, not by a `ports/core/<shelf>/<name>` path that goes stale at the next move.

`kpkg` finds a port by name through `PORT_REPO`, a space-separated list of at most eight
repositories searched in order. In each repository it tries `<repo>/<name>/` and then
`<repo>/<shelf>/<name>/` under every shelf, where a shelf is any directory of the repository that
has no `kpkgbuild` of its own; the first repository holding the name wins. `ports/core` holds its
ports one shelf down, and each `src/` area holds its ports directly. Inside one repository, a name
found at two paths is an error that names both paths, and `kpkg`, `kdos update`, `kdos cve` and
`kdos-portup` stop on it rather than pick one. A walk over every port, such as `kdos update`'s,
also refuses a port nested deeper than one shelf.
Shelves are never listed on `PORT_REPO`; a list naming more than eight repositories warns and the
rest are ignored.

| Phases | `PORT_REPO` |
|---|---|
| `20_selfhost`, `30_foundation`, `31_compilers` | `/ports/core`, the default in `/etc/kpkg.conf` |
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

A port of KDOS's own names no upstream source and builds out of its own directory, `$PORT_SRC`; see
[A port of KDOS's own](#a-port-of-kdoss-own).

### A port of KDOS's own

KDOS's own programs are ports whose sources live in git beside the recipe, each at
`src/<area>/<name>/`, exactly two levels below `src/`. Of the 24 recipes under `src/system`,
`src/art`, `src/desktop` and `src/daemons`, 22 have no `source =` line and two have an empty one. With no
source there is nothing to fetch or hash, `$SRC` is created empty, and `build.sh` compiles out of
`$PORT_SRC`. `src/art/kdos-theme` is the smallest; its `kpkgbuild`, after the banner, is
five lines:

```ini
name        = kdos-theme
version     = 1.0.0
release     = 2
description = KDOS theme generators — GTK stylesheet, icons, cursors
homepage    = https://github.com/kunaldawn/kdos
```

and its `build.sh` compiles the program together with the two shared libraries it uses, straight
from `src/libs`:

```bash
LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$PORT_SRC" \
	-o kdos-theme \
	"$PORT_SRC"/main.c "$PORT_SRC"/gtk.c "$PORT_SRC"/icons.c \
	"$PORT_SRC"/cursors.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c $LDFLAGS

install -Dm755 kdos-theme "$PKG/usr/bin/kdos-theme"
```

For such a port the [recipe hash](../06-reference/glossary.md) covers the whole port directory,
subdirectories included, and the whole of `src/libs`, because nothing else records what the
program was built from. Any edit under the directory rebuilds the port, and an edit to any
library rebuilds every port of KDOS's own, not only the ones that use it. How to write the program
itself is in [Writing desktop software](writing-desktop-software.md) and
[The C libraries](c-libraries.md).

## kpkgbuild

Every recipe opens with the KDOS banner header: the fixed block of `#` comment lines, the KDOS
name drawn in box characters followed by the tagline, that begins frotz's `kpkgbuild` and every
other recipe. Copy it verbatim from any of them (`testing/preflight.sh` fails a `kpkgbuild`,
`build.sh` or `postinstall.sh` without it). A block of `key = value` lines follows, here
frotz's, with its checksum shortened:

```ini
name        = frotz
version     = 2.55
release     = 1
source      = $name-$version.tar.gz::https://gitlab.com/DavidGriffith/frotz/-/archive/$version/frotz-$version.tar.gz
sha256      = a8c4c4d7…  frotz-2.55.tar.gz
description = Z-machine interpreter — Infocom-era interactive fiction in a terminal
homepage    = https://661.org/proj/if/frotz/
depends     = ncurses pkgconf
```

A key is an identifier (letters, digits and `_`, not starting with a digit), followed by `=` and
the value; blank lines and lines starting with `#` are skipped. A recipe without `name`, `version`
or `release` is not a recipe: `kpkg` refuses it with `not a recipe: name, version or release is
missing`.

### Keys the package manager reads

`kpkg`'s parser recognises thirteen names. Everything else on a `key = value` line becomes a
recipe helper (see [Recipe helpers](#recipe-helpers)).

| Key | Required | Repeats | Means |
|---|---|---|---|
| `name` | yes | | The package name |
| `version` | yes | | Upstream's version. No hyphen: a package file is `<name>-<version>-<release>.tar.xz` and is taken apart from the right, so a hyphenated version makes that name ambiguous, and preflight refuses it. Spell a tag like `1.9.0-Jumbo-1` as `1.9.0.jumbo1` and carry upstream's own form in a helper the `source` line reads |
| `release` | yes | | Bump to force a rebuild for a reason the recipe hash cannot see |
| `source` | | yes | An upstream URL, a `filename::url` pair, or a bare file name beside the recipe |
| `sha256` | | yes | `<64 hex>  <filename>`, one per declared file: every `source`, plus a vendor bundle or anything else beside the recipe the build opens |
| `description` | | | One line. `kpkg info` prints it and the binary-package index carries it; it is not a comment |
| `homepage` | | | Upstream's home page. The version checker reads it |
| `depends` | | | One line of space-separated port names. The solver, which decides build order, reads the first `depends` line and ignores any other |
| `vendoring` | | | `rust`, `go`, `python`, `haskell` or `node`. See [Vendoring](#vendoring) |
| `pypackages` | | yes | An explicit list of Python packages to vendor: the package and everything it depends on, directly or indirectly |
| `secdb` | | | The name the security database uses, when it differs from ours. `kdos cve` reads it |
| `bench` | | | A command `kdos march` times |
| `bench_setup` | | | A command that runs once before `bench` and is not timed |

`description`, `homepage` and `depends` are keys, not comments. The parser skips a `#` line
entirely, so a fact written as a comment reaches nothing.

`source`, `sha256` and `pypackages` accumulate across lines. `depends` does not: the solver and
`kpkg info` both take the first `depends` line and stop, so a second line is a dependency the build order does not know
about. Keep every dependency on one line. The solver reads at most 64 names from it; the longest
line in the tree, `ffmpeg`'s, has 42.

`build.sh` does not receive `description`, `homepage` or `depends` as variables. `kpkg info`, the
version checker, `kdos cve` and `kdos march` read `description`, `homepage`, `depends`, `secdb`,
`bench` and `bench_setup` literally from their first line, without expanding helpers, so write
those values without `$` references.

### Keys other tools read

These are ordinary helpers as far as `kpkg` is concerned (it stores them as variables and passes
them to `build.sh`), but another tool gives each a defined meaning:

| Key | Read by | Means |
|---|---|---|
| `vendordir` | `ports/fetch` | Where the vendoring tool must run, when that is not the top of the tree |
| `hsplan` | `ports/fetch` | A bootstrap plan, relative to `vendordir`, that is the Haskell dependency set; see [The Haskell bundle](#the-haskell-bundle) |
| `vendorsync` | `ports/fetch` | Further Cargo manifests, relative to `vendordir`, whose crates go into the same Rust bundle; see [Where the bundle goes](#where-the-bundle-goes) |
| `pyrequirements` | `ports/fetch` | `no`: a requirements file is *not* the dependency set here |
| `pyruntime` | `ports/fetch` | `no`: do not vendor the runtime dependencies |
| `npmflags` | `ports/fetch` | Words added to the `npm install` that writes a node bundle, such as `--legacy-peer-deps`; see [Vendoring](#vendoring) |
| `group` | `ports/update` | Override the derived version-check grouping; see [Outcomes](#outcomes) |
| `devseries` | `ports/update` | Upstream's development-series convention: `odd-minor`, `preview-minor`, or both; see [Filtering](#filtering) |
| `series` | `ports/update` | The line the port stays on, as a version prefix matched on whole components: `21` for `llvm21`, `5.4` for `lua54`; see [Filtering](#filtering) |
| `watch` | `ports/update` | Where upstream lists its releases when nothing reached from the source URL does: a forge repository, whose tags are read, or a page, listing, JSON index or manifest that names the release files; see [Discovery](#discovery) |

### Recipe helpers

Any key that is not one of `kpkg`'s own is a helper: `_tag`, `_commit`, `_date`, `debrev`, `vrsn`
and the rest. Helpers reach `build.sh` as shell variables. A recipe holds at most 32 helpers, and
the longest in the tree has 19.

Declare them between `release` and `source`. Each value is expanded as it is read, so a helper can
use `$version` and any helper above it, and `source` can use the helpers above it; declared in any
other order, a reference expands to nothing.

Values expand:

```text
$var   ${var}   ${var#p}   ${var##p}   ${var%p}   ${var%%p}
${var/a/b}      ${var//a/b}            ${var:off:len}
```

`#` and `%` take a glob pattern of `*` and `?`; the needle of `/` is literal. A name that is neither
`name`, `version`, `release` nor a helper declared above is taken from the environment of the
process reading the recipe, which is rarely what a recipe wants. Command substitution is not
available. Where upstream's convention needs a transformation, use the pattern operators: a version
with its dots removed is `${version//./}` rather than a pipeline, and `linux` builds its mirror
directory from `v${version:0:1}.x`.

### Sources, and what each one becomes

`source` repeats to add more files. What happens to each depends on its position and its
extension. First, the name it is saved under:

| Source | Saved as |
|---|---|
| The first URL (`http://`, `https://`, `ftp://`), with a recognised archive extension | `<name>-<version>.<ext>`, whatever the URL's basename is |
| A later URL | The URL's basename |
| A bare file name, not a URL | That file name, looked for beside the recipe |
| Anything written `filename::url` | `filename`, first or not |

Then, how it is unpacked before `build.sh` runs:

| Source | Unpacked |
|---|---|
| A tarball, first | Into `$SRC`, with `--strip-components=1` |
| A tarball, later | Into `$SRC_ROOT`, unstripped, beside `$SRC` |
| Anything else: a data file, a `.zip`, a `.tar.zst` | Copied into `$SRC` as it is |

The first source is renamed on purpose. A forge that generates an archive named after a tag would
otherwise leave every port holding a file called `2.55.tar.gz`, and the standardised name is what
the port directory, the checksum line and the source archive all agree on. Use `filename::url` when
the saved name needs to be something else, for example when a second port builds the same tarball
(see [A second build of the same source](#a-second-build-of-the-same-source)).

The two extension sets differ. The standardised name is derived from `.tar.gz`, `.tgz`, `.tar.bz2`,
`.tbz2`, `.tar.xz`, `.txz`, `.tar.zst` and `.zip`; what is unpacked is `.tar.gz`, `.tgz`,
`.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz` and `.tar`. A first source that is a `.zip` or a `.tar.zst`
is therefore saved as `<name>-<version>.zip` or `.tar.zst` and copied into `$SRC` whole; a recipe
that wants one unpacks it in `build.sh`.

The strip of one component from the first tarball assumes upstream's usual layout, one wrapping
directory. Two other layouts are handled by the recipe, and preflight checks both once the tarball
has been fetched:

- **A flat archive**, with several entries at the top level. The strip removes every top-level file
  and promotes each subdirectory's contents in its place, so the build finds the wrong `Makefile`
  or none. The recipe unpacks the tarball itself, with a `tar x… $PORT_SRC/…` line in `build.sh`.
- **A `./`-prefixed archive with one wrapping directory** (`./dir/…`, as `tar -c ./dir` writes it).
  The strip removes the `.`, so the tree lands one level down at `$SRC/<dir>`, and `build.sh` has
  to `cd` into that directory, named from `$name` or `$version`.

A release archive carries a submodule's directory empty, because the archive is generated from the
repository without recursing. If upstream's build expects a submodule, add it as a later `source`
extracted where the build looks, or make it a port.

### Checksums

Checksums are matched to files by **basename**, not by position, so reordering the `source` lines
cannot pair a hash with the wrong file. Before anything is unpacked, `kpkg` checks every
`sha256 =` entry whose file is in the port directory or its source directory, including entries no
source names, such as a vendor bundle. A mismatch stops the build with `sha256 MISMATCH for <file>`
and both hashes. An entry whose file is present in neither place is skipped at that step; a source
the build needs is caught when it is extracted.

A source with no hash is a refusal, not a pass: `kpkg` prints `No sha256 for <file> in the recipe`
and stops before extracting that file; any source listed before it is already unpacked. While you
bring up a new port and do not know the hash yet, `KDOS_ALLOW_UNVERIFIED=1` in the environment
lets that one build through with an `UNVERIFIED` warning, but only where `kpkg` is run by hand: on
a KDOS system, or in a shell opened with `script/chroot/enter.sh`. `make build` cannot pass it,
because `script/chroot/exec.sh` enters the chroot with a cleared environment that does not name
it, so under `make build` the hash has to be recorded first. Preflight fails any recipe that
mentions the variable, so it cannot be committed as a permanent answer.

A source is looked for in the port directory first and then in `kpkg`'s source directory,
`/var/cache/kpkg/sources` (`SOURCE_DIR` in `/etc/kpkg.conf`). That directory is not the source
cache `ports/.srccache` that `make fetch` fills on the build host. If the file is in neither, the
build stops with `Source not found: <file> (checked <port dir> and <source dir>)`.

## build.sh

`kpkg`'s build command, `kpkgbuild` (a name of the same multi-call `kpkg` binary, not to be confused
with the recipe file of the same name), runs `build.sh` by sourcing it inside `( set -e )`, with the
working directory set to the unpacked source. There is no `pipefail` and no `set -u`: a failing
command stops the build, but a failure in the middle of a pipeline does not, and an unset variable
expands to nothing. Everything the script prints goes to the port's build log under
`build/logs/<phase>/`.

The recipe's keys and helpers arrive as shell variables, each single-quoted on the way in, and are
not exported, so a child process such as `make` does not see them unless you export them yourself.
Alongside them come five paths and names, and the settings of `/etc/kpkg.conf`, which the same
shell sources first:

| Variable | Is |
|---|---|
| `$name`, `$version`, `$release` | From the recipe |
| `$source`, `$sha256`, `$vendoring`, `$pypackages`, `$secdb`, `$bench`, `$bench_setup` | From the recipe, when set |
| Every helper | From the recipe |
| `$PKGNAME` | `<name>-<version>-<release>.tar.xz`, the package file being made |
| `$PORT_SRC` | The port's own directory, where patches and vendor bundles are |
| `$SRC_ROOT` | `$WORK_DIR/<name>`, where a later tarball lands |
| `$SRC` | `$SRC_ROOT/<name>-<version>`: the unpacked source, and the working directory. It exists, empty, for a port with no source |
| `$PKG` | `$SRC_ROOT/pkg`, the staging tree: install here, never into `/` |
| `$SOURCE_DIR`, `$WORK_DIR`, `$PACKAGE_DIR`, `$PORT_REPO`, `$PKGDB_DIR` | From `/etc/kpkg.conf`: kpkg's source directory, work area, package output, ports tree and package database. Each takes the environment's value when one is set |

The compiler flags (`CFLAGS`, `CXXFLAGS`, `LDFLAGS`), the job count (`KDOS_JOBS`, exported as
`MAKEFLAGS=-j$KDOS_JOBS`, `CMAKE_BUILD_PARALLEL_LEVEL` and `CARGO_BUILD_JOBS`), `CC=gcc` and `CXX=g++` from
`30_foundation` on, and the reproducibility settings (`SOURCE_DATE_EPOCH`, `TZ=UTC`, `LC_ALL=C`,
`-ffile-prefix-map` and `--build-id=sha1`) come from the phase's environment and are exported. The
`script/phases/<phase>/phase.env` of every chroot phase sources `script/env/chroot.env`, which
holds the compilers, the release flags and `PKG_CONFIG_PATH`, and sources `script/env/common.env` in
turn: the reproducibility settings, the job count and `KPKG_STRICT_RECIPE=1`. `20_selfhost` unsets
`CC` and `CXX` again, and a phase.env adds only what is its own, such as `PORT_REPO`. The flags
from `20_selfhost` on are listed in [The release flags](#the-release-flags), and no phase adds
`-Werror`. Extend the flags rather than replace them: `export CFLAGS="$CFLAGS -Wno-error"`. Ninja
takes the job count without being told: once ninja is installed, `chroot.env` puts
`script/bin/ninja` first on `PATH`, and it adds `-j$KDOS_JOBS` to every `ninja` call that names no
job count before running `/usr/bin/ninja`, so a bare `ninja`, `meson compile`, `meson install` and a
CMake Ninja build all follow `KDOS_JOBS`; a call that passes its own `-j`, a tool (`-t`) or
`--version` goes through unchanged. A recipe that has to pass a job count explicitly, to a build
system of its own, reads `$KDOS_JOBS`, never `nproc` and never a parse of `MAKEFLAGS`: `nproc`
ignores both a lowered `KDOS_JOBS` and the memory clamp.

The minimal autotools recipe is three lines:

```bash
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

### What kpkg does around build.sh

The build command, `kpkgbuild`, wraps the script in a fixed sequence:

1. Set the umask to `022`, before anything below, so `$SRC`, `$PKG` and every file the build
   creates without an explicit mode, including what `make install` writes, have the same mode on
   every builder.
2. Check every declared `sha256 =` entry, before the work directory is touched.
3. Remove `$SRC_ROOT`, create `$SRC` and `$PKG`, and unpack or copy the sources. Each source
   must have a `sha256 =` entry; its bytes are not hashed again, having been checked at step 2.
4. Run `build.sh`.
5. Copy `postinstall.sh`, when there is one, into the package as `.POSTINSTALL`, preceded by the
   recipe's keys and helpers as shell assignments.
6. Delete every libtool `.la` file under `$PKG`: each names build-time paths that do not exist on
   the target and would put them on every consumer's link line.
7. Delete `usr/share/info/dir` and every `usr/share/fonts/*/fonts.dir`. Each is a shared index that
   `kpkg` regenerates on install (see [Shared indexes](#shared-indexes)); shipped in two packages,
   one would conflict with the other.
8. Roll `$PKG` into `$PACKAGE_DIR/$PKGNAME` reproducibly: names sorted, owner and group 0, every
   modification time set to `SOURCE_DATE_EPOCH`, GNU tar format, and a pinned multi-threaded `xz`
   (`-9 --block-size=32MiB`, or `-0` for a package `kpkg install` deletes once installed), so a
   package built twice from the same tree is byte-identical. The archive is written as
   `$PKGNAME.part` and renamed once complete. The time it took is logged as
   `Packaged <name>: <N> MB in <s> s`.

A recipe therefore does not delete `.la` files, write an info `dir`, or pack anything itself.

`kpkg` strips nothing either: a package carries exactly what the port's flags compiled. No port
ships DWARF, so a recipe overrides every upstream default that adds `-g` by flag, never by editing
the source: meson's `--buildtype=release`, CMake's `-DCMAKE_BUILD_TYPE=Release`, Go's
`-ldflags "-s -w"`, and the build system's own variable where it has one (GCC's
`CFLAGS_FOR_TARGET` and `CXXFLAGS_FOR_TARGET` for the libraries it builds for its target, CPython's
`OPT`). A `-g` that a build puts ahead of `$CFLAGS` is cancelled by `-g0` at the end of `CFLAGS`
or `CXXFLAGS` (`newsboat`, `aubio`); one in a make variable of its own is cancelled or left out
through that variable (`linuxcnc`'s `EXTRA_DEBUG=-g0` and `ULFLAGS`); one a makefile appends after
them, or one in compile rules that never read them, is dropped at the link by `-Wl,--strip-debug`
in `LDFLAGS` (`frotz`), or left out by a command-line `CFLAGS` that replaces the makefile's own
line (`stfl`, `hfsprogs`, `routino`). `testing/debuginfo.sh` lists what a built
tree still carries; see
[Testing](testing.md#debug-information-in-the-built-tree).

## Canonical build shapes

Each build system has one shape that works on this tree. Start from it and change only what the
project needs; most of the failures in [Build troubleshooting](build-troubleshooting.md) come from
leaving one of these flags out. Of the recipes under `ports/core`, about 580 run a `configure`
script, 600 run CMake, 240 run meson and 57 run `cargo build`.

Every shape below builds a release: optimised, with no debug information, with assertions compiled
out where the build system's release mode does that, and with upstream's SIMD and assembly on. How
the flags and the checks were chosen is in
[Decisions](../01-philosophy/decisions.md#one-release-flag-set-raised-per-port).

### The release flags

From `20_selfhost` on, `script/env/chroot.env` exports these to every `build.sh`:

| Variable | Value |
|---|---|
| `CFLAGS` | `-O2 -pipe -std=gnu11 -fPIC -fno-semantic-interposition -fstack-clash-protection -ffile-prefix-map=/var/cache/kpkg/work=/build` |
| `CXXFLAGS` | the same, without `-std=gnu11` |
| `LDFLAGS` | `-Wl,-O1,--sort-common,--as-needed,-z,now,-z,pack-relative-relocs -Wl,--build-id=sha1` |
| `CMAKE_BUILD_TYPE` | `Release`, the type of a CMake project whose recipe names none |
| `CARGO_PROFILE_RELEASE_DEBUG` | `0`, so no crate's release profile turns debug information back on |
| `GOFLAGS` | `-trimpath -buildvcs=false` |
| `CGO_CFLAGS`, `CGO_CXXFLAGS`, `CGO_LDFLAGS` | copies of `CFLAGS`, `CXXFLAGS` and `LDFLAGS` |

The compiler adds PIE and the stack protector by default (`--enable-default-pie`,
`--enable-default-ssp`). What each option costs a port:

- `--as-needed` records a library only when an object before it on the link line uses it. A
  makefile that names `-lfoo` ahead of its objects, or a library needed only for its constructor,
  needs `-Wl,--no-as-needed`; see
  [Build troubleshooting](build-troubleshooting.md#a-library-dropped-by---as-needed).
- `-z,now` stops musl deferring an unresolved symbol in a plugin opened with `RTLD_LAZY`. A plugin
  that takes symbols from a library loaded after it needs `-Wl,-z,lazy`; see
  [Build troubleshooting](build-troubleshooting.md#a-plugin-that-needs-lazy-binding).
- `-z,pack-relative-relocs` writes relative relocations as `DT_RELR`, which musl 1.2.4 and later
  loads. A binary from this tree cannot run under an older C library.
- `-fno-semantic-interposition` lets GCC inline an exported function into callers in the same
  file. `LD_PRELOAD` then cannot replace that one call; calls from other files still can be.

A recipe adds to these and never replaces them: `export CFLAGS="$CFLAGS -Wno-error"`, never
`export CFLAGS="-O2 …"`, which drops the prefix map, the hardening and the interposition flag at
once. A package's bytes change with every change to them, but `KPKG_STRICT_RECIPE` compares
recipes, not flags: an installed port picks a flag change up only when it is rebuilt.

### Every port

Check each of these against the recipe, whichever build system it uses:

- **Compare with Alpine and T2 SDE.** Alpine's recipe is
  `https://gitlab.alpinelinux.org/alpine/aports/-/raw/master/<repo>/<pkg>/APKBUILD`, with `<repo>`
  one of `main`, `community` and `testing`; T2 SDE's is under
  `https://svn.exactcode.de/t2/trunk/package/<category>/<pkg>/` (`*.conf`, `*.desc`). Look for
  `${CFLAGS/-Os/-O2}` or `-O3`, `-flto`, `--optflags`, `-DNDEBUG`, `--enable-*asm`, `nasm` among
  the build dependencies, and `CMAKE_BUILD_TYPE`.
- **No debug build.** Not `-O0`; not `-g` or `-ggdb` added by the recipe; not `--enable-debug`,
  `--with-debug` or `-DDEBUG`; not CMake's `Debug`, `RelWithDebInfo` or an empty build type; not
  meson's `debug`, `debugoptimized`, or `plain` with no flags; not `cargo build` without
  `--release`; not zig without an optimize mode; not qmake's `CONFIG+=debug` or
  `debug_and_release`. A switch that only makes a debug path available at run time is not a debug
  build: `ocl-icd`'s `--enable-debug` is upstream's default and prints nothing unless
  `OCL_ICD_DEBUG` is set.
- **Every compile and link line takes the exported flags.** A build system that reads none from
  the environment is handed them through its own variable, and the build log's compile and link
  lines are the check, not the recipe: b2's toolset declaration in a `user-config.jam` (`boost`,
  as Alpine does; a b2 argument would split `LDFLAGS` at its commas), qmake's release variables
  through `--qmake-setting` (`python3-pyqt6`, `python3-qscintilla`), a makefile's link command or
  flag variable given on the command line with the makefile's own entries restated after the
  exported flags (`zip`'s `BIND`, `stk`, `ladspa`, `hfsprogs`, `routino`), or the one variable a
  makefile appends to its own (DarkPlaces' `CPUOPTIMIZATIONS` in `xonotic`, which carries
  `LDFLAGS` as well because its release link line repeats it). Where the build system overwrites
  the flags and no variable reaches the compile, a patch puts them first:
  `translatelocally`'s `marian-honour-flags.patch`, because marian sets `CMAKE_CXX_FLAGS`
  outright. Upstream's own level and maths options come after the exported flags and stand, even
  `-Ofast`, `-ffast-math` or `-Os`, where upstream chose them for its code: `goxel`, `hydrogen`,
  `sauerbraten`, `cataclysm-dda`, `retroarch` and `routino` build as upstream intends. A realtime
  or kernel-side part upstream compiles with fixed flags keeps them (`linuxcnc`'s realtime
  components).
- **No machine-specific code.** Never `-march=`, `-mtune=`, `-mcpu=native`, zig's `-Dcpu=native`,
  rustc's `target-cpu`, `GOAMD64` above v1, or a project's own "optimise for this machine" switch,
  such as libsodium's `--enable-opt`. The build machine's CPU is not the one that runs the
  package; the feature level is [`kdos march`](../04-programs/kdos-command.md#kdos-march)'s to
  choose, per machine. Where upstream selects SIMD code at run time, turn that on: gmp's
  `--enable-fat`, stockfish's `ARCH=x86-64-universal`, and the dispatch dav1d, x265,
  libjpeg-turbo and ffmpeg carry. Where upstream selects it only at build time, build one copy per
  level and let the program choose among them as it starts: `john` ships an AVX-512BW, AVX2, XOP,
  AVX and SSE2 build, each compiled with `-DCPU_FALLBACK` to hand over to the next when the
  processor lacks its level, and `satdump` builds its DVB plugin with SSE4.1 and again without,
  and each copy registers its decoders only where the other does not. Watch for a
  default that is above x86-64 v1 or taken from the build machine: numpy's `cpu-baseline` defaults
  to x86-64 v2, so its recipe passes `-Dcpu-baseline=none` and keeps the run-time dispatch, and
  OpenBLAS infers the CPU for its code outside the kernels unless `TARGET` names one. A floor
  upstream's code cannot go below stays, with nothing added above it: marian, in
  `translatelocally`, compiles with `-msse4.1` whatever it is given, so its `BUILD_ARCH` is
  `x86-64` rather than its default `native`, and the package needs SSE4.1.
- **SIMD and assembly on, with the assembler in `depends`.** `nasm` for ffmpeg, x264, x265, dav1d,
  libvpx (`--as=nasm`), libjpeg-turbo (`-DWITH_SIMD=ON`) and libass (`--enable-asm`); openssl never
  configured with `no-asm`. SIMD options above SSE2 stay off, since the baseline is x86-64 v1
  (kodi's `ENABLE_SSE3=OFF` and its siblings), and lame keeps `--enable-nasm=no` as upstream
  advises.
- **`-O3` for a hot port by pattern.** Where upstream's own default is `-O3` and the exported
  `CFLAGS` replaces it, as for zstd, lz4, xxhash, openssl and sqlite in Alpine, raise the level in
  `build.sh`:

  ```bash
  export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"
  ```

  meson needs this form: it puts the exported `CFLAGS` after its own `-O3`, so
  `-Doptimization=3` alone loses to `-O2`.
- **LTO only for the hot libraries.** Link-time optimisation is for code that most of the
  desktop's time is spent in. In this tree that is python, built with PGO and `--with-lto`;
  Pillow's imaging modules; cmake, through its own `CMake_BUILD_LTO`; and the media and drawing
  libraries: ffmpeg, x264, x265, dav1d, libvpx, zstd, lz4, flac, opus, pipewire, pixman, cairo and
  harfbuzz. Nothing else gets it, however large: not Qt, nodejs, perl, R, LibreOffice, WebKit, the
  browsers or the applications. Use the project's own switch where it has one (`--enable-lto` in
  x264, `--enable-lto=auto` in ffmpeg, `-Db_lto=true` in meson,
  `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` in CMake) and `-flto=auto` in both `CFLAGS` and
  `LDFLAGS` otherwise. Each of those thirteen libraries also adds `-frandom-seed=<port>` to `CFLAGS` (and
  `CXXFLAGS` for C++). GCC names each object's `.gnu.lto_*` sections with a random number unless
  that flag fixes it, and an archive keeps those sections, so without it a port that ships a `.a`
  differs on every build. A port that ships a `.a` also adds `-ffat-lto-objects`, or the archive
  holds only bytecode and links only through GCC's plugin; zstd and lz4 do both. Keep the change
  only once the port builds byte-identically twice.
- **The job count is `$KDOS_JOBS`**, never `nproc`: `scons -j"$KDOS_JOBS"`, not
  `scons -j"$(nproc)"`.

### autotools

```bash
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

configure takes the exported flags. Read `./configure --help` for `--disable-debug`,
`--disable-assert` (autoconf's `AC_HEADER_ASSERT`, which defines `NDEBUG`), and `--enable-asm`,
`--enable-simd` or `--enable-sse2`, and pass the release side of each.

### meson

```bash
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
      --buildtype=release -Dtests=disabled -Ddocs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
```

Every meson setup needs `--prefix=/usr --libdir=lib`. meson's default library directory is not on
the runtime linker's search path, and the symptom is a shared library that cannot be loaded at run
time, long after a clean build and install.

Every meson setup also names its buildtype, `--buildtype=release` or `-Dbuildtype=release`.
meson's default is `debug`, which compiles `-g -O0` into every object, and `kpkg` strips nothing,
so the package ships unoptimised code and its DWARF. `testing/preflight.sh` fails a recipe that
runs `meson setup` without naming a buildtype.

The buildtype does not define `NDEBUG`, so a meson port keeps its `assert()` checks, and a
recipe leaves them on. In a library that parses a file, a peer's message or a kernel interface, an
assertion is the last check between bad input and memory corruption, and outside an inner loop it
costs nothing that can be measured. The security- and hardware-facing libraries — p11-kit,
pcsc-lite, ccid, libdrm, libepoxy, libglvnd, libva, libva-intel-driver, libplacebo, wayland,
wlroots, bubblewrap and xdg-dbus-proxy — never turn them off.

`-Db_ndebug=if-release` is for a port whose assertions sit in a hot path it spends its time in,
and only these pass it:

| Port | What `NDEBUG` removes |
|---|---|
| gtk4 | the checks in the roaring bitmaps under `GtkBitset`, walked on every list-view update |
| openh264 | the encoder's per-slice checks; upstream's own release `Makefile` defines `NDEBUG` |
| mesa | `-D b_ndebug=true`, the driver's checks on every draw call |

harfbuzz sets `b_ndebug=if-release` in its own `default_options` and needs nothing from the
recipe. Before adding a port to that table, read what its `meson.build` hangs on the buildtype
rather than on `b_ndebug`: gtk3 and gtk4 take `G_DISABLE_ASSERT` and `G_DISABLE_CAST_CHECKS` from
`optimization`, and glib takes `G_DISABLE_ASSERT` from its own `glib_debug` option, so for gtk3 and
glib `b_ndebug` changes nothing.

meson's release optimisation is `-O3`, but the exported `CFLAGS` follows it on the command
line, so a meson port builds at `-O2` unless its recipe raises `CFLAGS` itself (see
[Every port](#every-port)).

Check option names against the tarball's own `meson_options.txt` or `meson.options`. meson fails at
setup on an unknown option, before a line is compiled, and there is no universal spelling: one
project's disable flag is fatal in the next. meson's built-in options are always valid.
`testing/preflight.sh` checks every `-D` against the option files of every tarball the port
carries, and checks the values of the option types that take a closed set, but only for a port
whose tarball has been fetched, so run `make fetch` (or `ports/fetch <port>`) before trusting that
check.

`-Ddocs=disabled` turns off the HTML and API references. Where a project puts its manual pages
behind their own option (`-Dman=true`, `-Dman-pages=enabled`), that option stays on; see
[Manual pages](#manual-pages).

### cmake

```bash
mkdir build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
make
make DESTDIR=$PKG install
```

The tree's CMake is 4.4.3, which refuses a project that declares a minimum version below 3.5;
`CMAKE_POLICY_VERSION_MINIMUM=3.5` raises that floor without a patch. `-G Ninja` with `ninja` and
`DESTDIR=$PKG ninja install` works equally well and is common in the tree.

Release appends `-O3 -DNDEBUG` after the exported `CFLAGS`, so a CMake port builds at `-O3` with
assertions off. Name the build type in the recipe even though `CMAKE_BUILD_TYPE` is exported: the
command line is what a reader sees. A recipe that sets `CMAKE_C_FLAGS_RELEASE` or
`CMAKE_CXX_FLAGS_RELEASE` itself keeps `-DNDEBUG` in it. A project can put `-UNDEBUG` back behind
its own switch whatever the build type: LLVM's `LLVM_ENABLE_ASSERTIONS` follows the build type and
is off in Release, but `LIBUNWIND_ENABLE_ASSERTIONS` defaults to on, so the `libunwind` recipe
names it off. Look for an `*_ENABLE_ASSERTIONS` option before trusting Release alone. Turn on the
project's SIMD options, such as `WITH_SIMD`, `ENABLE_ASSEMBLY` and `*_ENABLE_ASM`, and leave
`CMAKE_INTERPROCEDURAL_OPTIMIZATION` off except in a port on the LTO list in
[Every port](#every-port).

A misspelt CMake option is a warning, not an error, the opposite of meson. Read the warning about
unused variables in the log rather than trusting the exit status, and take option names from the
project's own `option()` declarations.

### rust

```bash
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
install -Dm755 target/release/<program> $PKG/usr/bin/<program>
```

The `RUSTFLAGS` line is required. The musl target links statically unless told otherwise, and a
crate that binds a system library then fails to link, or carries a private copy of the library that
no update to its port reaches. Preflight fails any recipe that runs `cargo build`, `install`,
`cbuild` or `cinstall` without `-crt-static`. `LIBCLANG_PATH` matters when a crate runs bindgen,
which loads `libclang` at build time and cannot do so from a static binary; `ports/fetch` warns
when it vendors `bindgen` for a recipe that lacks the `RUSTFLAGS` line.

`cargo build` always takes `--release`; `cargo install` builds the release profile by default, and
no recipe passes `--profile dev` or `--debug`. The profile's optimisation level, codegen units and
LTO stay at cargo's defaults: one codegen unit with LTO roughly doubles a Rust build. Where
Alpine's recipe sets them for a hot program, set them in that recipe alone, with
`CARGO_PROFILE_RELEASE_LTO=true` and `CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1`. In a mixed build,
meson's buildtype or corrosion's `CMAKE_BUILD_TYPE` chooses the cargo profile.

### go

```bash
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w" -o <program> ./cmd/<program>
install -Dm755 <program> $PKG/usr/bin/<program>
```

Every `go build` and `go install` passes `-s -w` in its `-ldflags`. Without them the Go linker
writes DWARF and a symbol table, which `kpkg` does not strip. A recipe that stamps a version puts
both in the same string, `-ldflags "-s -w -X main.version=$version"`, because a second `-ldflags`
replaces the first; `GOFLAGS` does not carry them, because a recipe's own `-ldflags` replaces the
one it names. `testing/preflight.sh` fails a `go build` or `go install` whose `-ldflags` lacks
either flag.

`-trimpath` and `-buildvcs=false` come from the exported `GOFLAGS`. A recipe that sets `GOFLAGS`
itself extends it, `GOFLAGS="$GOFLAGS -mod=vendor"`, or loses both. `GOAMD64` stays unset, which
is v1.

Most Go ports build with cgo off. A program that binds a C library sets `CGO_ENABLED=1`, and cgo
then compiles and links its C with the exported `CGO_CFLAGS` and `CGO_LDFLAGS`. One whose
upstream builds through a makefile passes the vendor flag through it:
`make PREFIX=/usr GOFLAGS="$GOFLAGS -mod=vendor"`, then `make PREFIX=/usr DESTDIR=$PKG install`
(`aerc` is the example). A makefile that writes its own `-ldflags` passes `-s -w` only when told:
podman's, buildah's and skopeo's append `EXTRA_LDFLAGS="-s -w"` and aerc's
`GO_EXTRA_LDFLAGS="-s -w"`, and a binary such a makefile builds with no `-ldflags` at all takes
them from `GOFLAGS="$GOFLAGS \"-ldflags=-s -w\""`, which a command-line `-ldflags` replaces.
Preflight does not see inside a makefile, so read its `go build` lines. Build into an output name
that is not also a directory in the source, or `go build` writes the binary inside that directory.

### zig

```bash
zig build -Doptimize=ReleaseSafe -Dcpu=baseline --prefix "$PKG/usr"
```

Name an optimize mode, `-Doptimize=ReleaseFast` or `ReleaseSafe` (or `--release` where the
project's `build.zig` offers it), and always `-Dcpu=baseline`: zig compiles for the build
machine's own CPU unless told otherwise, and the package then stops with an illegal instruction on
an older one.

### qmake

```bash
qmake6 CONFIG+=release QMAKE_CFLAGS_RELEASE="$CFLAGS" QMAKE_CXXFLAGS_RELEASE="$CXXFLAGS" \
       QMAKE_LFLAGS_RELEASE="$LDFLAGS" PREFIX=/usr
make
make INSTALL_ROOT=$PKG install
```

`CONFIG+=release`, never `debug` or `debug_and_release`. Qt's qmake reads no flags from the
environment, so the three flag variables carry them; each replaces the mkspec's release value
(`-O2`, `-Wl,-O1`), which the exported flags already hold, and leaves the base `QMAKE_LFLAGS` a
project's `.pro` adds to alone. A PyQt binding built by `sip-build` takes the same three as
`--qmake-setting "QMAKE_CXXFLAGS_RELEASE = $CXXFLAGS"` and so on.

### SCons, waf and other build tools

SCons ignores the environment unless the project's `SConstruct` imports it: pass the release
switch it defines (`mode=release`, `debug=no`, `optimize=yes`) and hand it `CFLAGS`, `CCFLAGS` or
`LINKFLAGS` where it takes them. waf reads `CFLAGS` from the environment; check that no `--debug`
or `--enable-debug` is passed.

### Python, Perl, Haskell, Node and OCaml modules

- **Python** (setuptools or a PEP 517 backend): a C extension compiles with the interpreter's own
  `-DNDEBUG -O3` followed by the exported `CFLAGS`. Nothing to add; check that the package's
  accelerated build is on, with no `*_NO_EXTENSIONS` set and its Cython build enabled. Where a
  failed compile falls back to pure Python without an error, make the extension required:
  MarkupSafe and wcwidth take `CIBUILDWHEEL=1` for that, PyYAML `PYYAML_FORCE_LIBYAML=1`.
- **Perl**: `Configure` ignores the exported `CFLAGS` and `LDFLAGS`, so the `perl` port passes
  them: `-Doptimize="$CFLAGS"` (as Alpine does), `-Dldflags="$LDFLAGS"` and
  `-Dlddlflags="-shared $LDFLAGS"`. XS modules take perl's recorded settings, so a `perl-*` recipe
  adds nothing.
- **Haskell** (cabal): keep the default `-O1`, pass no `--enable-debug-info`, and `-O2` only where
  upstream asks for it.
- **Node** native modules: node-gyp builds Release by default; pass no `--debug`.
- **OCaml**: build the native `ocamlopt` targets where upstream offers them beside bytecode.

### java

No Ant, Maven or Gradle is a port, so a Java program is compiled with `javac` and packed with
`jar` directly. Each dependency is a recipe source: its `-sources.jar` from Maven Central, or its
repository's tarball when no sources jar is published.

```bash
mkdir -p build/depsrc build/classes
for j in <dep>-<ver>; do
	unzip -q -o "$j-sources.jar" -d build/depsrc
done
rm -rf build/depsrc/META-INF
find src/main/java -name '*.java' | LC_ALL=C sort > build/sources.list
javac -encoding UTF-8 -nowarn -proc:none -implicit:class -d build/classes \
	-sourcepath "build/depsrc:src/main/java" @build/sources.list
cp -r src/main/resources/. build/classes/
( cd build/classes && find . -type f | LC_ALL=C sort | sed 's/.*/"&"/' > ../classes.list )
( cd build/classes && jar --create --file ../<program>.jar --manifest ../manifest.txt \
	--date="$(date -u -d "@$SOURCE_DATE_EPOCH" '+%Y-%m-%dT%H:%M:%SZ')" @../classes.list )
install -Dm644 build/<program>.jar $PKG/usr/share/<program>/<program>.jar
```

The program's own sources are the list; the dependencies sit on the source path, so `javac`
compiles only the classes the program reaches (`-implicit:class`), and an optional integration
that would need a library nobody ships is never compiled. `--date` stamps every jar entry with the
tree's pinned time, and the sorted list fixes their order, so two builds write the same jar. The
manifest names `Main-Class`, and a launcher script in `/usr/bin` runs `java -jar` on it. `digital`,
`josm` and `logisim-evolution` are the examples; each depends on `openjdk`.

### make-only

```bash
export CFLAGS="$CFLAGS -Wno-error"
make PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr install
```

Export the compiler flags rather than passing them as a make argument. A variable on the make
command line beats both the environment and the makefile's own assignment, which is the wrong end
of that precedence for flags: a makefile's own definitions are its *configuration* (architecture
width, installation paths, feature constants), and a makefile that appends with `+=` loses what it
appends, `-fPIC` included. Passing flags as arguments discards those, and the build then fails
somewhere else, on an undeclared constant that reads like a missing header. Check the makefile for
a debug default of its own, such as `CFLAGS = -g -O0`. A plain assignment ignores the environment,
so that one variable is the exception: once you have read that it carries nothing but flags, pass
it on the command line, `make CFLAGS="$CFLAGS"`.

### A graphical application

An application with a window of its own is ported natively the same way as any other program, with
the shape of its build system above. The toolkits it may build on are ports built in `43_toolkits` (`tk`
in `42_graphics`): GTK 3 and 4 (`gtk3`, `gtk4`), `libadwaita`, WebKitGTK (`webkitgtk`, `webkitgtk6`), Qt 5 and 6
(`qt5-qtbase`, `qt6-qtbase` and their modules), KDE Frameworks 6, `wxwidgets`, `fltk` and `tk`.
Each toolkit that has a Wayland backend is built with it as the default and with its X11 backend as
well, so a program that has only an X11 path runs under Xwayland with no change to the recipe. Tk
has one windowing system on Linux, X11, so every Tk window is an Xwayland client. These toolkits are
for applications only; KDOS's own desktop programs link none of them (see
[Writing desktop software](writing-desktop-software.md)).

The recipe does not choose the backend at run time; the session does, from
`fs/etc/profile.d/10-wayland.sh`:

| Variable | Value | Effect |
|---|---|---|
| `QT_QPA_PLATFORM` | `wayland;xcb` | Qt uses Wayland, and falls back to Xwayland only when its Wayland plugin cannot start |
| `GDK_BACKEND` | unset | GDK tries Wayland first by itself, and an X11-only program's own `GDK_BACKEND=x11` is not overridden |
| `GTK_USE_PORTAL` | `1` | GTK's file chooser, print dialog and settings go through the portals, so a GTK application opens the desktop's file chooser |
| `QT_QPA_PLATFORMTHEME` | `kde` | A Qt 6 application reads the `~/.config/kdeglobals` that `kdos theme` writes, and a Qt 5 application the qt5ct files it writes, and both follow the desktop's colours |
| `SDL_VIDEODRIVER`, `CLUTTER_BACKEND` | `wayland` | The same preference for SDL and Clutter programs |

A few habits recur in the tree's application recipes, and a new one should follow them:

- **Pin every optional dependency.** An option left on auto turns a feature on or off according to
  whatever happens to be installed when the port builds. A meson recipe sets each feature option
  explicitly; a CMake recipe names what it wants with `CMAKE_REQUIRE_FIND_PACKAGE_<Name>=ON` and
  what it must not pick up with `CMAKE_DISABLE_FIND_PACKAGE_<Name>=ON` (127 build scripts use one
  or both).
- **KDE applications install into Qt's own paths.** `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` with
  `-D BUILD_TESTING=OFF` is the shape `kate`, `dolphin`, `okular` and 132 other build scripts use.
- **Ship a desktop entry and an icon the launcher can draw.** A graphical program with no entry
  cannot be started from the Start menu; see
  [Desktop entries belong to the port](#desktop-entries-belong-to-the-port). `celluloid`'s recipe
  writes its own entry and rasterises its SVG icon to 48, 64 and 128 pixel PNGs under
  `/usr/share/icons/hicolor`.

These applications are named in `44_apps`, the phase after the toolkits and libraries they need,
each in the list file of its shelf. An application another port depends on is named in
`43_toolkits` instead, so its dependants can build after it: `konsole`, which `dolphin` depends
on, and `vlc`, which `phonon-backend-vlc` links. [The ports catalogue](../06-reference/ports-catalogue.md) lists them by shelf.

## Worked example: frotz

frotz exercises most of the format in one recipe. It renames its source, ships a
desktop entry that opens a file, and defines a MIME type that a shared index has to pick up on the
target.

The metadata is the block shown under [kpkgbuild](#kpkgbuild). It declares the source under the
name the tree expects: the `::` form names the saved file outright. For a first source the
automatic rename would give the same `frotz-2.55.tar.gz`; spelling it out keeps the name fixed
whatever shape the URL takes.

`build.sh` is the make-only shape, with two options chosen rather than defaulted:

```bash
export CFLAGS="$CFLAGS -Wno-error"
export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
make curses PREFIX=/usr SOUND_TYPE=none
make install PREFIX=/usr SOUND_TYPE=none DESTDIR=$PKG
```

The curses interface and no other: the SDL one wants a window server and the dumb one has no screen
model. `SOUND_TYPE=none` keeps an audio stack off every image for the handful of stories that use
sound. frotz 2.55's makefile adds no `-Werror` and no phase adds one, so `-Wno-error` has no effect
on this release; it keeps a `-Werror` in a later upstream release from turning this compiler's
newer warnings into failures without a patch (see
[An upstream `-Werror`](build-troubleshooting.md#an-upstream--werror)). The makefile appends
`-O3 -g` after `CFLAGS`, so the objects carry debug information and `-Wl,--strip-debug` leaves it
out of the linked program. The flags are exported
through `CFLAGS` and `LDFLAGS` and the make variables are upstream's own configuration knobs, as the
[make-only shape](#make-only) describes.

The package carries no story. The desktop entry is `Exec=frotz %f` with `Terminal=true`, the shape
every terminal program here that opens a file uses, so a story is opened from the file manager or
any other program that opens a file by its type. That entry claims two MIME types,
`application/x-zmachine` and `application/x-blorb`, so the port has to define them first;
`shared-mime-info` 2.5.1 has neither:

```bash
install -Dm644 /dev/stdin \
	"$PKG/usr/share/mime/packages/kdos-zmachine.xml" <<'MIMEXML'
…
MIMEXML
```

Installing the package rebuilds the MIME database on the target, which is the one job `build.sh`
cannot do; see [Shared indexes](#shared-indexes).

## Adding a port, end to end

The whole procedure, from an empty directory to a source other people can fetch. Each step links
to the section that explains it.

1. **Find the canonical upstream URL and the latest stable version.** Watch for projects whose
   releases are on a different host from their documentation, and for archives whose top-level
   directory is not `<name>-<version>`. List the archive's contents before writing the recipe
   ([Sources, and what each one becomes](#sources-and-what-each-one-becomes)).
2. **Choose the shelf** from `ports/shelves`, by the rules in
   [Choosing a shelf](#choosing-a-shelf). Check that the name is not taken: one name is one port
   across every shelf and every `src/` area.
3. **Write `kpkgbuild`** in `ports/core/<shelf>/<port>/`, with the banner header, the
   [keys](#keys-the-package-manager-reads), and any [helpers](#recipe-helpers) between `release`
   and `source`. Leave the `sha256 =` lines out for now.
4. **Fetch the source and record its checksum:**
   ```sh
   ports/fetch <port>
   sha256sum ports/core/<shelf>/<port>/<name>-<version>.tar.*
   ```
   `ports/fetch` takes the bare name and finds the port on whatever shelf it sits. With no
   `sha256 =` line, it downloads from upstream and warns `no sha256 for <file> in the recipe`. Add
   the line (`sha256 = <hash>  <file>`) straight away: `kpkg` refuses to extract an unhashed
   source, and nothing else can verify it. For a port with `vendoring =`, the same run generates
   `<name>-vendor-<version>.tar.xz`; hash and record that file too. `make fetch` takes no port name
   and walks all 2,000 ports, so use `ports/fetch <port>` here. When you do want the whole tree,
   `make fetch FETCH_JOBS=8` works on eight ports at once; each port's lines then print together
   when it finishes.
5. **Write `build.sh`** from the [canonical shape](#canonical-build-shapes) for its build system,
   applying any [patches](#patches) before it configures.
6. **Wire it in.** Name the port in the `depends` line of whatever needs it, and name it in the
   list of the phase that installs it, in the file of its shelf. Every package phase from
   `30_foundation` on names every port it installs, a dependency as much as a program nothing
   depends on. [Which phase lists a port](#which-phase-lists-a-port) says which phase and which
   file.
7. **Check the wiring**, in two to three minutes rather than hours into a build:
   ```sh
   testing/preflight.sh
   ```
   It checks, among much else, that the port sits on a listed shelf under a name no other port
   has, that every `depends` name exists, that every phase installs exactly the ports its list
   names, that every source is hashed, that the first source's layout is accounted for, and that
   every meson option exists. The phase check on its own is `python3 testing/phaseclosure.py`,
   which takes a fraction of a second.
8. **Build only that port**, in the phase that lists it, and package the result:
   ```sh
   make build BUILD_ARGS="--phases 41_system,70_image --rebuild <port>"
   ```
   Put the port's own phase where `41_system` stands. Drop `70_image` to build the package without
   making an ISO.
9. **Read the log** under `build/logs/<phase>/` and, when the build fails, look the message up in
   [Build troubleshooting](build-troubleshooting.md).
10. **Publish the source** before pushing the commit (see [Publishing sources](#publishing-sources)).
    Uploading needs a maintainer's token. With one:
    ```sh
    ports/publish <port>
    git commit ports/core/<shelf>/<port> ports/sources.idx script/phases/41_system/packages.d/<shelf>.txt
    ```
    Name in the commit the list file step 6 edited, and the recipe of any port whose `depends`
    line now names the new one.
    Without a token, run `ports/publish --check <port>` to list the hashes the archive lacks, and
    name those ports in your pull request so a maintainer publishes them.

### Choosing a shelf

A shelf holds the ports a reader would look for together, and `ports/shelves` lists all 102 with a
line saying what belongs on each. Apply these rules in order; the first that matches decides.

1. **A `group =` family stays together.** Every port sharing a `group =` key sits on the shelf of
   the family's lead port, so that `ports/update` bumps ports of one directory: `python3-pyside6`
   is on `qt6` with Qt, `python3-qscintilla` on `qt-extra` with `qscintilla`, `stk-assets` on
   `games-action` with `supertuxkart`. Preflight fails a family split over two shelves.
2. **A name-prefix family decides, over the port's domain.** `python3-*` goes to a `python*` shelf
   (rule 7), `perl` and `perl-*` to `perl-cpan`, `qt6-*` to `qt6` and `qt5-*` to `qt5`,
   `libretro-*` and `retroarch*` to `libretro`, `sdl*-*` to `game-libs`, `font-*`, `noto-*`,
   `ttf-*` and `terminus-*` to `fonts`, `libX*`, `xcb-*` and `xorgproto` to `x11`, `gst-*` and
   `gstreamer` to `media-frameworks`, `soapy*` to `sdr-hw` and `gr-*` to `sdr`, `kicad*` to `eda`,
   `fcitx5*` to `input`, `lua54-*` to `lang`, and `texlive*` to `doctools`.
3. **KDE Frameworks go to `kf6`**: any port whose homepage is under `invent.kde.org/frameworks` or
   `api.kde.org/frameworks`, whatever it does. A KDE-hosted library outside the frameworks, such as
   `kirigami-addons`, goes to `qt-extra`.
4. **The kind of data decides for data.** Font files go to `fonts`; icon, cursor and sound themes to
   `themes`; firmware for the host to `boot`, and firmware for an external device to that device's
   shelf (`meshtastic-firmware` on `hamradio`). A data, asset, help or model package, or a plug-in
   that exists for one program, follows that program: `gimp-help` is on `graphics`, `mpv-mpris` on
   `video-players`, and `flare-engine`, an engine for one game, on `games-rpg`.
5. **An application is filed by what it does**, never by its toolkit or desktop project. There is no
   KDE or GNOME shelf: `dolphin` is on `files`, `konsole` on `shells`, `okular` on `documents`, and
   `kpat` and `gnome-mines` on `games-board`.
6. **A library goes to its domain's shelf when it has one**: `libsoup3` to `net-libs`, `librsvg` to
   `graphics-libs`, `libksane` to `printing`, `libkdegames` to `game-libs`. Only a library whose
   job is the toolkit or the desktop platform itself goes to that stack's shelf (`gtk`, `qt-extra`,
   `kde`, `wl`, `x11`), and a Qt or GTK widget library goes to `qt-extra` or `gtk` however generic
   it is. A general-purpose C or C++ library with no domain goes to `devlibs`, even when it has
   one main consumer (`tllist`, `talloc`), and so does a portable SIMD or parallel runtime
   (`highway`, `onetbb`), unless it belongs to one framework (`orc` is on `media-frameworks`). A
   low-level library the base system links goes to `base-libs`.
7. **Python.** Only `python3-*` ports go to the Python shelves, split by domain: `python` (the
   interpreter, build and packaging), `python-libs` (general libraries and file formats),
   `python-net`, `python-sci`, `python-gui`, `python-dev` (Jupyter, language servers, debugging)
   and `python-hw`. An unprefixed Python module goes by its domain (`numpy` is on `sci-libs`,
   `sympy` on `math`), and a Python application by what it does (`calibre` is on `documents`,
   `meson` on `buildtools`).
8. **Other languages.** Every language without a module shelf of its own goes to `lang` with its
   modules: Lua, Ruby, Haskell, Go, Rust, Node, OCaml, Java, Tcl/Tk, Guile, R, Zig and Vala. A
   language that reaches about ten module ports gets its own shelf, named `<language>-<something>`
   and never after the interpreter's port. Vendored crates, Go modules and Hackage packages stay
   inside their port and are not ports.
9. **Tie-breakers.** A client-and-server program that ships a daemon goes to `servers` (`openssh`,
   `mosh`), and a database server to `database`. Terminal emulators go to `shells`. File managers
   and disk-usage tools go to `files`; partitioning, RAID, LVM and block encryption to `disk`;
   filesystem tools and FUSE filesystems to `filesystem`; burning and CD reading to `optical`.
   Sandbox primitives and container networking go to `containers`, remote-desktop protocols and
   VNC to `virt`, and code and binding generators (`bison`, `flex`, `swig`, `bindgen`) to
   `toolchain`.
10. **Still ambiguous:** the shelf of the port's only main consumer, and failing that the shelf a
    reader would open first. Say why in the commit message, never in the recipe.
    [The shelves](../06-reference/ports-catalogue.md#the-shelves) in the ports catalogue lists what
    each shelf holds today, which is the quickest way to see where a port's neighbours are.

A new shelf is a new line in `ports/shelves`, `<id> - <description>`, and a new directory; `make
publish-plan` then replaces the `-` with the shelf's source-archive volume (see [Volumes and
shelves](#publishing-sources)), and preflight fails the line until it has one. Its id is
lowercase letters, digits and `-`; it is never `libs`, because a source-less port hashes `<portdir>/../../libs` and a
shelf by that name would be hashed into those ports; never `core`; and never the name of a port,
which could not be told from a loose port. Preflight and the [pre-push hook](#the-pre-push-hook)
refuse a tree that breaks any of this. A listed shelf must also hold at least one port, which
preflight alone checks.

### Which phase lists a port

A port is installed by the first package phase whose list names it, and every port it depends on
is installed by that phase or an earlier one. A later list may name it again only in its order run
(below), to hold an order, as `40_lang`'s `00-order.txt` names `toybox` and the ports that take
back its names; there it is built again only when its recipe hash has changed. From
`30_foundation` on a list names every port its phase installs and nothing it does not: the
dependency closure of the list, minus what an earlier phase installed, must equal the list.
`testing/phaseclosure.py`, run by preflight, checks it, and names a port pulled in without being
listed, the phase that installs it, and the later phase that names it. It also refuses a port an
earlier phase installs named anywhere but an order run, a name listed twice in one phase, a name
that is not a port on that phase's `PORT_REPO`, and a dependency that only a later phase's
`PORT_REPO` can reach.

So a new port goes in the earliest phase its dependencies allow and its kind admits:

| Phase | A port belongs here when |
|---|---|
| `30_foundation` | It is a build system, an interpreter or a base library that later phases build with |
| `31_compilers` | Its closure reaches one of the large compilers (LLVM, Rust, Go, GHC, Zig, Node, Ruby) |
| `40_lang` | It is a language module, a language, or a build, documentation, debugging or version-control tool, and its closure needs nothing past `31_compilers` |
| `41_system` | Nothing graphical and no toolkit is in its closure: services, networking, storage, command-line programs, codecs, science and hardware libraries |
| `42_graphics` | Its closure reaches Wayland, libX11, Mesa, libdrm, cairo, pango, GStreamer, FFmpeg or PipeWire but no toolkit, and it is not a port nothing depends on that sits on an application shelf |
| `43_toolkits` | Its closure reaches GTK, Qt, wxWidgets, FLTK or Motif, and it is a library (a toolkit, a KDE Framework, or something an application links) or an application another port depends on (`konsole`, `vlc`) |
| `44_apps` | Nothing depends on it, and its closure reaches a toolkit, or it sits on an application shelf and its closure reaches the graphics stack |
| `50_desktop` | It is part of KDOS's desktop, or something only the desktop needs |
| `60_kernel` | It is the kernel or what the kernel build needs |

The application shelves are `3d`, `browsers`, `cad`, `chat`, `editors`, `emulators`, `files`,
`games-action`, `games-board`, `games-rpg`, `games-shooter`, `games-strategy`, `graphics`,
`libretro`, `mail`, `mobile`, `music-players`, `pim`, `studio`, `video-players` and `video-tools`.
A port on any other shelf whose closure reaches the graphics stack but no toolkit stays in
`42_graphics` even when nothing depends on it, as `koreader` on `documents` and `nvtop` on
`sysmon` do.

`20_selfhost` rebuilds the toolchain once inside the chroot and takes no new port. A dependency a
port gains later moves nothing by itself: when it is listed in a later phase than the port, the
check fails until one line moves, the dependency's to the port's phase or earlier, or the port's to
the dependency's phase or later.

`20_selfhost`, `30_foundation`, `31_compilers`, `50_desktop` and `60_kernel` each keep one list,
`script/phases/<phase>/packages.txt`. Its order run is the names ahead of the first shelf banner,
a comment line `# <shelf> — <description>`; after it the ports sit under the banner of their shelf,
or, for a port of KDOS's own, under a `# src-<area> — <description>` banner (`50_desktop`'s
`src-desktop` and `src-daemons`). A new port goes under its shelf's or its area's banner, added in
the same form when the list has none. The
five userland phases, `40_lang` to `44_apps`, split their list into
`script/phases/<phase>/packages.d/`: one file per shelf, `<shelf>.txt`, holding the phase's ports
from that shelf; `src-<area>.txt` for ports of KDOS's own, where the phase installs any
(`src-system.txt` in `41_system`, `42_graphics` and `44_apps`, and `src-art.txt` in `41_system`);
and, in `40_lang` and `41_system`, `00-order.txt`, the order run, which sorts first and holds the runs whose order a comment
pins, such as `toybox` followed by the ports that take back the names it compiles out. The files
are read in byte order as one list, so which file a port is in changes nothing but where a reader
finds it.

The order run is also what `kdosbuild --port-jobs` keeps serial: its ports, and every port the
resolved order puts among them, build one at a time and before any other port of the phase. Pin a
pair there when their order matters for a reason `depends` cannot say, such as two ports that
install the same path, where whoever installs last owns it; a build by level names such a pair when
a commit moves a path the serial order would not have. A new port goes in its shelf's file, created with the same header as its siblings when
the phase has none for that shelf yet. A phase has `packages.txt` or `packages.d/`, never both, and
preflight fails a file in `packages.d/` whose name is not a listed shelf, a `src-` area or
`00-order`, and a port in the file of a shelf or area it is not on.

## Rules a recipe must keep

These conventions apply to every recipe in the tree, including KDOS's own under `src/`. Several are
checked by `testing/preflight.sh`.

| Rule | Why |
|---|---|
| No explanatory comments in `kpkgbuild`: the banner header plus the metadata keys, nothing else | The recipe is data. Reasoning belongs in the commit message or in this book. `build.sh` is a script and carries the comments any script does |
| No source edits with stream editors (`sed -i` and the like) | Use a build flag. Patch only where no flag exists, and then ship a real `.patch` beside the recipe (see [Patches](#patches)), so the change is reviewable and covered by the recipe hash |
| Every optional feature explicit, and every library it needs in `depends` | Many build systems answer a missing library by quietly disabling the feature. Dropping a dependency then gives a build that succeeds and is narrower than its recipe claims, and the library is absent from the host with nothing to say so |
| Every port of the same phase it builds against in `depends`, tools included | A serial build installs a phase's ports in list order, which can hide a missing entry: the library happens to be installed first. `kdosbuild --port-jobs` builds a level's ports side by side against the lower levels only, and the levels come from `depends`, so an undeclared same-phase dependency is absent there and the port fails or loses the feature. See [Building a package phase by level](build-system.md#building-a-package-phase-by-level). `testing/depdrift.py` reads a built tree and names every same-phase library a package links or requires without declaring it; see [Undeclared link dependencies](testing.md#undeclared-link-dependencies) |
| Every library the port links in `depends`, whatever its phase | The [package store](../03-architecture/packaging.md#the-package-store) keys a port on its declared dependencies. After an install `kpkg` reads the package's ELF files and prints `<port> links <owner> without declaring it` for each library owned by a port outside that closure; the store entry then re-checks that library's bytes on every lookup, but a library found and used without being linked (a plugin, a header, a tool) stays invisible to it. Add the owner to `depends` |
| The exported flags extended, never replaced: `CFLAGS="$CFLAGS …"`, `LDFLAGS="$LDFLAGS …"`, `GOFLAGS="$GOFLAGS …"` | A bare assignment drops the reproducibility flags, the hardening and the release options at once, and the package still builds. See [The release flags](#the-release-flags) |
| No `-march`, `-mtune`, `native` CPU or feature level above x86-64 v1 | The build machine is not the one that runs the package; [`kdos march`](../04-programs/kdos-command.md#kdos-march) chooses a feature level per machine. zig needs `-Dcpu=baseline` to keep this |
| Every meson `-D` is an option the port defines | meson stops at setup on an unknown option. Preflight checks each one against the tarball's own option file, and checks the two option types that take a closed set of values |
| A command named in a diagnostic is in single quotes | A backtick inside double quotes is a command substitution, not a name: an `echo` telling somebody to run something runs it instead. Preflight checks the build system's own scripts under `script/` |
| Nothing reaches the network | The build runs with no network. A meson subproject fallback, a CMake download call, or a Python build backend fetching a tool from a package index all fail hours in. See [Build troubleshooting](build-troubleshooting.md#a-build-that-reaches-the-network) |
| Shipped configuration uses only glyphs the console font has | The console font holds 512 glyphs, a kernel limit; a Nerd Font icon renders as a blank cell on `tty1` |

A Nerd Font icon is a private-use codepoint the console font cannot carry, so on `tty1` it appears
as a blank cell in front of every name. Turn icons off where the program has a switch (`yazi`'s
`[icon]`, `starship`'s `format`, `eza --icons=never`), and check the default before writing
anything (`lazygit` 0.65's is already off). Where a program draws them with no way to turn them
off, add it to [known gaps](../06-reference/known-gaps.md). The answer is never a patched console
font.

## Patches

A patch is the last resort after a build flag, and it is a file, never an edit made in place: a
unified diff kept beside the recipe as `<topic>.patch`, where it can be read and reviewed. 436 of
them sit in port directories under `ports/core`. `kpkg` does not apply them. `build.sh` does, from
the unpacked source it starts in, normally before it configures:

```bash
patch -p1 -i "$PORT_SRC/lua-root-usr.patch"
```

Write the diff with paths one directory deep (`a/src/…`, `b/src/…`), as `git diff` or
`diff -ru old new` does, so `-p1` strips that directory and the paths resolve against `$SRC`;
`patch -p1 -i "$PORT_SRC/<file>.patch"` is the form nearly every port uses.

Name a patch `*.patch`. The [recipe hash](../06-reference/glossary.md) covers every file beside the
recipe that no `sha256 =` names, so a `*.diff` rebuilds the port when edited too, but `.patch` is
what the hash takes first and by name, and what a port of KDOS's own is read for. A patch is
tracked by git with the recipe and is not published to the source archive.

A numbered patch series that upstream publishes is a source, not a file of ours. `readline` names
each of its official patches as a later `source =` with its own `sha256 =`; each is copied into
`$SRC` unchanged, and `build.sh` applies them in order. They are diffs from the top of the release
tree, so they take `-p0`:

```bash
for p in "$_pfx"-[0-9][0-9][0-9]; do
	patch -p0 -i "$p"
done
```

`bash` applies its own series the same way.

## What is built from source

Everything the host installs and runs on its own processor is compiled here from source; the
Debian packages in boxes are outside the rule. A recipe that installs a program, a library, a
module or a shared object it did not compile breaks the claim the whole tree makes: the binary
cannot be read, cannot be rebuilt by `kdos rebuild`, and carries whatever its builder put in it.
Five classes of payload are exempt, and each is exempt for a reason the next recipe has to be able
to name.

| Class | What it covers | Why it cannot be compiled here |
|---|---|---|
| Code for another processor | `linux-firmware`, `intel-ucode`, `sof-firmware`; the closed EU kernels `intel-media-driver` compiles in with `ENABLE_KERNELS=ON` and `BUILD_KERNELS=OFF`; the assembled i965 shader kernels `libva-intel-driver` includes from `src/shaders`; the SOF coefficient `.bin` files in `alsa-ucm-conf`; the flasher stubs and flash algorithms inside `espflash`, `probe-rs`, `python3-esptool` and `openfpgaloader`; the riscv64 EDK2 image `qemu` installs from its `pc-bios/` (every other guest firmware it ships is compiled from its `roms/`); the radio firmware images `meshtastic-firmware` and `rnode-firmware` install for flashing; the Perseus FX2 firmware and FPGA bitstreams `libperseus-sdr` compiles in; the guest ROMs and programs the emulators install (`fuse-emulator`'s Spectrum ROMs, `vice`'s Commodore ROMs, `openmsx`'s C-BIOS, `amiberry`'s AROS Kickstart and WHDLoad boot binaries, `dosbox-staging`'s DOS utilities and FreeDOS keyboard drivers), and the replacement console BIOSes compiled in as byte arrays (`libretro-melonds`'s `FreeBIOS.h`; `mgba`'s and `libretro-mgba`'s `hle-bios.c`); `wine-mono` and `wine-gecko`, the Windows .NET runtime and HTML engine Wine installs into a prefix | It runs on a DSP, a GPU, a microcontroller, a radio or a guest (an emulated machine, or a Windows program under Wine), not as a host program. For most of it no source is published; the rest needs a cross toolchain this tree does not carry (an ARM assembler for the GBA BIOS, MinGW and a .NET SDK for Wine's two) |
| Compiled font data | `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`, `font-carlito`, `font-caladea`; the Type 1, OpenType and TFM files in `texlive`'s texmf tree; the faces bundled inside `mupdf`, `matplotlib`, `seqkit`, `kodi`, `koreader`, `qcad`, `freecad`, `solvespace`, `stellarium`, `mupen64plus`, `ppsspp`, `dosbox-x`, `retroarch-assets`, `uosc`, and `vice`'s HTML manual; the Fork Awesome face `qtforkawesome` compiles into its library | Upstream publishes the built face, and the sources compile through a toolchain or a source tree this one does not carry. A face whose upstream build runs on ports is compiled: `ttf-dejavu`, `terminus-ttf`, `noto-emoji`, `font-liberation`, and the X.org bitmap fonts (`font-adobe-75dpi`, `font-cursor-misc`, `font-misc-misc`) from BDF |
| Bootstrap seeds | The `rust` stage-0 `rustc`, `rust-std` and `cargo`; the `go` bootstrap toolchain; `zig`'s `stage1/zig1.wasm`; the upstream musl GHC `ghc` builds with; the Adoptium musl JDK `openjdk` boots from; `ocaml`'s `boot/ocamlc` bytecode image; the Apache Groovy and Commons jars `kodi`'s add-on binding generator runs on | A compiler written in its own language needs a working one first, and Kodi's generator is a Groovy script, which is not a port. Each seed is used only to build, and never ships |
| Data with no other source form | The `tessdata-eng` OCR models for `tesseract`, `whisper-model-base-en`'s speech model, `fluidr3-gm-sf3`'s SoundFont, the `perl-xml-parser` `.enc` encoding maps, the `fcitx5-chinese-addons` pinyin and stroke tables, `john`'s `.chr` files, the RP2350 boot-ROM tails `picotool` embeds from `model/`, recorded audio such as the `speaker-test` samples in `alsa-utils`; trained network weights (`digikam`'s face models, `piper`'s diacritisation models, `stockfish`'s NNUE network, `rnnoise`'s `rnnoise_data.c`, `freedv-gui`'s Opus, RNNoise and RADE weights); map and lookup data (`organicmaps`' world maps and style tables, `sniffnet`'s country and ASN databases, `josm`'s tag2link index, `koreader`'s certifi CA bundle); game and learning content (`gcompris`' word, voice and music archives, `openttd-opengfx`, `openttd-opensfx` and `openttd-openmsx`, `xonotic-data`, `stk-assets`, `devilutionx.mpq`, `retroarch-assets`); the PDF manuals `texlive-doc` ships | The file is the form upstream maintains, or the tool that made it (a trainer, a map generator, nml and grfcodec, a 3D map compiler, each package's own TeX setup) is not something this tree runs; there is nothing earlier to build it from here |
| Built JavaScript | The JavaScript of `libkiwix`'s server skin; the web clients `transmission` (`web/public_html`), `deluge` (its ExtJS library), `kolibri` (its webpack bundles), `pat` (`web/dist`) and `kodi` (`webinterface.default`) ship; `yarn`'s `cli-dist` bundle, which inlines an emscripten build of libzip | Each is the output of a JavaScript toolchain over an npm dependency tree. Rebuilding one takes a node vendor bundle of its own (`cncjs` is the port that carries one) and upstream's bundler, so each ships as upstream released it |

Every exempt payload is still a `source =` line with a `sha256`, so the offline build and the
checksum hold for it exactly as for a tarball of C.

What follows for a recipe:

- **A prebuilt object for the host that fits no class is removed.** When a source tarball carries
  one, use the flag that rebuilds it, or delete it from `$PKG` after the install. `go` deletes the
  race detector's runtime and the BoringCrypto module; `john` deletes the `ztex` bitstreams and
  controller image, which no program it builds can load. Shipping it makes the package contain a
  binary nobody here compiled.
- **A Java library is compiled from its published sources.** Maven Central's `-sources.jar` for
  each dependency is a `source =` line like a tarball, and `javac` compiles it on the source path
  with the program (see [java](#java)). A binary `.jar` is class files somebody else compiled; the
  only class files the tree fetches are `kodi`'s build-time seeds above, which never reach `$PKG`
  (`josm`'s tag2link jar is a JSON index packed as a jar, and only the index is taken from it).
- **A new exemption names its class.** A payload that needs one of the five goes in the table
  above and in the [inventory](../01-philosophy/why-kdos.md#what-is-not-built-from-source) in the
  same change, or the list of exceptions is incomplete and cannot be relied on.
- **A bootstrap seed never reaches `$PKG`.** What ships is what the seed built. A recipe that
  installs the seed ships a compiler this tree did not compile.

## Desktop entries belong to the port

A program that draws in a terminal is an application on this desktop, and almost none of them ship
a `.desktop` file: upstream writes one for a GUI or writes none at all. Write it in `build.sh`,
into `$PKG/usr/share/applications/`. A package owns its entry, so installing the port adds the menu
row and removing the port takes it away; the same file under `fs/` is owned by nothing and outlives
the program it names.

Four entries are under `fs/usr/share/applications/`, all `NoDisplay=true` handlers that exist for
their `MimeType=` line. `kdos-openarchive.desktop` names a script that is itself under `fs/`
(`fs/usr/local/bin/kdos-openarchive`), so entry and program have the same lifetime.
`kdos-peek.desktop`, `kdos-pix.desktop` and `kdos-burn.desktop` name front ends that the `kdos-shell` port installs as
links to `kdos-shell`; they are the exception to this rule, and without `kdos-shell` they would
name programs that are not there.

```bash
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/btop.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=System Monitor
GenericName=Resource Monitor
Comment=Processes, CPU, memory, disks and network
Exec=btop
Icon=speedometer
Terminal=true
Categories=System;Monitor;
Keywords=process;cpu;memory;task;monitor;btop;
EOF
chmod 644 "$PKG/usr/share/applications/btop.desktop"
```

What each line of an entry has to get right, and what goes wrong otherwise:

**`Terminal=true` and a bare `Exec`.** The launcher picks the terminal emulator and supplies the
window's identity (see [`kdos-shell`](../04-programs/kdos-shell.md)). Naming an emulator in `Exec`
pins the entry to that one emulator and defeats `X-KDOS-Term`.

**`X-KDOS-Float=true` and `X-KDOS-Size=COLSxROWS`, when the program wants its own window size.** A
float is an unanchored window at the size the entry asks for, rather than one the session places
among the rest. The size is in cells, and anything smaller than 4x2 is ignored. `kdos app tui`
writes both keys for entries it makes, and a recipe writes them for the same reason (see
[`kdos app tui`](../04-programs/kdos-command.md#kdos-app-tui)).

**Never `X-KDOS-TUI=true`.** That key is `kdos app tui`'s own marker. It means "this command wrote
this file", which is what makes `kdos app tui rm` safe to run. A shipped entry carrying it would be
an application that command could delete.

**`X-KDOS-Term=kdos-term` only where the program draws pictures.** The key names the emulator the
entry needs rather than the one the session runs, and the launcher honours it: `kdos-term` links
the image decoders and speaks sixel and the kitty graphics protocol, so `yazi`'s previews are
pictures rather than a file name. Without the key the session's own terminal is used, which is
lighter. The value is a name, never a program: only `kdos-term` and `foot`, the two emulators this
image ships, are accepted, and any other value falls back to the session's own. An entry is a file
anything can write, and a key that named a program would be a second `Exec` line with none of its
rules.

**An `Icon=` the image can draw.** The shipped atlas is narrower than the artwork.
`src/art/kdos-icons/genatlas.py` takes six contexts (`places`, `devices`, `status`,
`mimetypes`, `actions`, `emblems`) at four sizes (24, 32, 48 and 64). There is no `apps` context
and no `panel` one: `panel/` holds about 2,400 third-party tray marks at each of 16, 22 and 24 px,
and an application's own icon comes from hicolor at run time instead. A name can therefore be
present in `src/art/kdos-icons/art` and still be undrawable: `file-manager` is only in
`panel/`, and `utilities-terminal` is not there at all, though both are what the freedesktop naming
specification suggests. After the atlas, `libkicon` falls back to hicolor's `apps/` PNGs and to
`pixmaps/`. An SVG in either place is never read, because nothing in the session rasterises one.
When a build tree exists, `testing/preflight.sh` resolves every shipped entry's `Icon=` and `Exec=`
by these rules against `build/fs` and names the ones that miss.

**A `Name=` no other visible entry uses.** The Start menu, the launcher and the search all list
entries by name, so two rows both reading `Calendar` are two rows nobody can choose between. The
program filling a role (the file manager, the agenda) keeps the plain name, and every alternative
is qualified: `Files (lf)`, `Files (yazi)`, `Calendar (calcurse)`. An entry with `NoDisplay=true`
is not visible and does not count. When a build tree exists, `testing/preflight.sh` refuses a
collision.

**`MimeType=` only where nothing else claims the type.** When two entries claim one type, the
machine opens those files in whichever entry sorted first, which is not a decision anybody made.
`mimeapps.list` is where a default is chosen.

**`MimeType=` only for a type that exists.** The field names a type in the shared MIME database, and
a name with nothing behind it resolves to nothing and reports that nowhere. The desktop's type
lookup, in `libkxdg`, reads `/usr/share/mime/globs`, which is generated from
`/usr/share/mime/packages`. A port that introduces a type installs its own XML there, and `kpkg`
regenerates the database (see [Shared indexes](#shared-indexes)). `frotz` is the [worked
example](#worked-example-frotz).

**`Keywords=` with the words people will search for.** The menu searches this field, and a program
is found by the words people know it by, which are often not its name.

For the game ports (`nethack`, `frotz`, `bsd-games` and `moon-buggy`), `testing/selftest.sh` reads
the entries out of the `build.sh` heredocs and fails a missing `Terminal=true`, a `Categories=`
with no `Game` token, or an `Exec=` naming a path rather than a command, at the recipe rather than
after a packaging run.

## Manual pages

A port installs every manual page upstream ships or can generate, into
`$PKG/usr/share/man/man<section>/`. `man` reads them in place, and installing the package indexes
them for `apropos` and `whatis` (see [Shared indexes](#shared-indexes)). A page missing from a
package is missing from the machine: nothing else supplies it.

| Upstream | The recipe |
|---|---|
| Installs its pages from `make install` or `meson install` | Nothing, unless a flag turned them off |
| Puts them behind an option | Turns on the manual-page option only (`--enable-man`, `-Dman=true`, `-DENABLE_MAN=ON`), not a full-documentation one that pulls in an HTML toolchain |
| Ships finished pages that its installer skips (most Rust, Go, Zig and Python projects) | `install -Dm644 doc/foo.1 -t "$PKG/usr/share/man/man1"` |
| Has the program write its own page (`foo --generate man`, a `man` subcommand) | Runs the freshly built binary into `$PKG/usr/share/man/man1` |
| Generates them with a tool | Adds the tool to `depends` when it is a port |

The generators that are ports: `scdoc`, `help2man`, `asciidoc` (`a2x`), `asciidoctor`, `xmlto`,
`libxslt` (`xsltproc`) with `docbook-xsl`, `python3-docutils` (`rst2man`), `perl` (`pod2man`),
`texinfo`, `go-md2man`, `lowdown`, `xmltoman` and `python3-sphinx` (`sphinx-build -b man`). A page
that needs a generator that is not a port, such as `ronn`, is not generated.

A build that looks for `asciidoctor` on `$PATH` uses it whenever it is there, so a recipe that
does not name it ships a different package once any other port pulls it into the chroot. Name it
in `depends` and set the documentation switches explicitly. Where upstream has no switch for the
manual pages alone, build the pages' own targets (`newsboat`, `git-lfs`) or run upstream's page
script (`ccache-manual`, a port of its own because ccache is built a phase before asciidoctor):
the package carries the pages and no HTML manual. An HTML manual ships only
where the program opens it itself: `wireshark`'s Help menu reads the HTML form of each page, so
that package carries both forms.

Markdown pages come in two dialects, and each has its converter:

| Source | Converter | The recipe |
|---|---|---|
| A `*.1.md` that upstream's docs `Makefile` feeds to `$(GOMD2MAN)` (the containers stack) | `go-md2man` | Runs upstream's own docs and install targets |
| A page that starts with a pandoc `%` title block and that upstream renders with `pandoc -s -t man` | `lowdown` | `lowdown -s -Tman -o <page> <page>.md`, then installs it |

`lowdown` stands in for `pandoc`. `pandoc` is a port, but a GHC build whose freeze pins 259
Hackage packages, which is too heavy a dependency for a manual page. `lowdown` reads the same `%`
title block into `.TH`, and `-M key=value` supplies what a pandoc invocation passes as
`--variable`: `-M title=YQ -M section=1` for a page with no title block, `-M source=v$version`
where the title block carries an unexpanded `$version` or a version older than the release. Pass
`--out-no-smarty` when the page writes long options outside code spans, or each leading `--`
renders as an en dash. A pandoc-only escape, `\ ` for a non-breaking space, reaches the page as a
literal backslash; `shellcheck` ships a patch that replaces it with a space in its option
headings. Inside a code span the backslash is literal in both renderers, so leave those alone.
Render a new page once and read it with `mandoc -Tlint` before shipping it; a page that only draws
style warnings is fine. `lowdown` is built with `bmake`, because its makefile is BSD make and GNU
make stops at the first `.if`. `bmake` depends on `tzdata` because its install runs its unit
tests, and two of them convert a time in a named zone: without the zoneinfo database they print UTC
and the install fails.

`python3-sphinx` installs Sphinx, and the part of its closure that is not a port, under
`/usr/lib/python3-sphinx` rather than in `site-packages`: `requests` and `urllib3` are ports in
`site-packages` already, and two packages owning one path is a conflict. The closure carries the
default theme, `myst-parser` for Markdown sources and `sphinx-argparse`. The commands in
`/usr/bin` put that prefix on `PYTHONPATH` and run Sphinx, and they are the only way in:
`import sphinx` from a plain `python3` fails, so a build that probes for Sphinx as a module
instead of running `sphinx-build` does not find it.

A Sphinx recipe builds the man builder's output alone. Where a build system turns warnings into
errors behind an option (`SPHINX_WARNINGS_AS_ERRORS` in LLVM), turn it off: the build has no
network, so every intersphinx inventory fails to load and warns. Where a `conf.py` loads an
extension this tree does not carry and the pages do not use, run `sphinx-build` directly with
`-D extensions=<the list without it>`, which replaces the list `conf.py` sets (`khal`, `khard`).
Two cases have no workaround: a `conf.py` that refuses to load without an HTML theme, and a
documentation switch that builds the HTML manual and the pages together.

A version beside another version installs no pages the other one installs, or the two packages
conflict: `openssl3`, `lua54`, the `llvm21` [slot](#a-second-version-beside-the-first) and the cross
toolchains ship none of the native port's pages. `man-pages` supplies the kernel and C library
sections (2, 3, 4, 5, 7) and leaves out every page another port installs.

## Shipping a script the port carries

Some ports install a small script of KDOS's own beside the upstream program, such as a filter for
`aerc`. Where you keep that script matters.

The [recipe hash](../06-reference/glossary.md), which decides whether a port needs rebuilding,
covers `kpkgbuild`, `build.sh`, `postinstall.sh`, every `.patch` and every other file in the port's
directory that no `sha256 =` line names (see [Anatomy of a port](#anatomy-of-a-port)). Every
phase sets `KPKG_STRICT_RECIPE=1`, under which an installed package whose recipe hash has changed
counts as not installed and is rebuilt, so a helper script kept in its own file beside the recipe
reaches the image on the next build after an edit. `testing/preflight.sh`, however, checks only a
script written into `build.sh`.

Write such a script into `build.sh`, in a quoted heredoc whose delimiter is `KDOS_SH`:

```bash
install -d "$PKG/usr/libexec/aerc/filters"
cat > "$PKG/usr/libexec/aerc/filters/kdos-part" <<'KDOS_SH'
#!/bin/sh
...
KDOS_SH
chmod 755 "$PKG/usr/libexec/aerc/filters/kdos-part"
```

The delimiter is what `testing/preflight.sh` looks for. It extracts each `KDOS_SH` body into a file
of its own and parses it with `bash -n` (`bash -n` on the recipe reads a heredoc as one word and
sees nothing inside it), and, when a build tree is present, checks that every program the script
names as the first word of a line, after `if `, or after `set -- ` is on the image. A name inside
a command substitution or a `trap` string is not seen. Quote the delimiter, or `$1` and `$PATH`
expand while the recipe runs rather than while the script does.

## postinstall.sh

`postinstall.sh` is an optional hook that runs on the target machine each time the package is
installed. It travels inside the package as `.POSTINSTALL`, a bash script that begins with the
recipe's keys and helpers as variable assignments and then carries the hook verbatim, so a hook can
read `$name` and `$version`. Twenty-six ports have one:

- `avahi`, `clamav`, `geoclue`, `gnuhealth`, `kolibri`, `libstoragemgmt`, `maddy`, `minidlna`,
  `mosquitto`, `mumble`, `networkmanager-openvpn`, `ngircd`, `nut`, `pcsc-lite`, `polkit`,
  `postgresql`, `prosody`, `radicale`, `tcpdump` and `usbmuxd` create their system accounts.
  `avahi` makes two, `avahi` and `avahi-autoipd`; `mumble`'s is `mumble-server`, `usbmuxd`'s is
  `usbmux`, the account its udev rule starts `usbmuxd` as, and `nut`'s is also in `dialout`.
  `clamav`, `kolibri`, `maddy`, `prosody`, `postgresql` and `radicale` also give their `/var/lib`
  directories to their accounts; `gnuhealth` gives `/var/lib/gnuhealth` and its `attach/`
  directory to its account; `minidlna` gives `/var/lib/minidlna` and `/var/log/minidlna`;
  `mumble` gives `/var/lib/mumble-server` to its account and `mumble-server.ini` to its group; and
  `networkmanager-openvpn` gives its chroot to its account. `gnuhealth`, `maddy`, `mumble`,
  `postgresql` and `radicale` set that directory's mode as well, because `kpkgadd` creates
  every directory 0755.
- `libvirt` creates the `libvirt` group its polkit rule admits, and a `qemu` account in `kvm`.
- `swtpm` gives `/var/lib/swtpm-localca` to `tss` at mode `0750`: libvirt runs `swtpm_setup` as
  `tss`, and `swtpm_localca` refuses a CA directory it cannot write, so without the hook a
  system-mode guest with an emulated TPM never starts.
- `linux` removes the module trees of other kernels. It keeps the running kernel's and those of
  every kernel on the ESP, runs `depmod`, and builds the new kernel's initramfs into the root as
  `/boot/initramfs-kdos.cpio.gz`. Installing into the running system, it then puts both into the
  running [root slot](../06-reference/glossary.md)'s ESP directory with `kdos-bootctl deploy /`;
  `kdos update` deploys the inactive slot itself
  ([A new kernel](../03-architecture/boot-and-init.md#a-new-kernel)).
- `dbus` gives `dbus-daemon-launch-helper` back its group, `messagebus`, and its mode `4110`: the
  package is rolled `root:root`, and the bus can run the helper only through that group, so without
  the hook no `User=root` service is ever activated.
- `ca-certificates` writes `/etc/ssl/cert.pem` with its own `update-ca-certificates`: the Mozilla
  bundle it ships plus the administrator's local roots, which a package-owned file would lose on
  every upgrade.
- `brltty` creates the `brlapi` group its polkit rule admits to BrlAPI, and puts the desktop account
  in it when it first creates it.

Every hook works on `PKG_ROOT`, the root `kpkgadd` is installing into, never on `/`.
`kpkg install --root` and an A/B update both install into a tree that is not the running system,
so a hook that wrote `/etc/passwd` or ran `depmod` on `/` would change the wrong machine and leave
the new root without what it needs. Take the root as `"${PKG_ROOT:-/}"`, prefix it on every path,
and hand it to the tool: `groupadd -R`, `useradd -R`, `depmod -b`. `chown` resolves a name against
the running root's `/etc/passwd`, so read the ids out of `$PKG_ROOT/etc/passwd` and pass them as
numbers, as the `dbus` hook does. A hook runs on every install and reinstall, so each step checks
before it acts.

The one exception is `linux`'s write to the ESP. The ESP is not part of any root: each slot's kernel
lives in its own `EFI/kdos/<slot>/` there. The hook runs `kdos-bootctl deploy /` only when
`PKG_ROOT` is the running system and `/boot/efi` is mounted. For any other root it only builds
`/boot/initramfs-kdos.cpio.gz` into that root, and `kdos update` deploys it once the whole run has
gone in.

Anything a hook writes into the root while the package is installed into the image is baked into
that image and is identical on every machine installed from it; `linux`'s ESP write is the one step
that depends on the machine, and it runs at every kernel update into the running system.
Per-machine state therefore cannot come from here; it is generated on first boot by the init script
that needs it.

Use a hook only for a job that must happen on the target, with the target's own programs, and that
belongs to this one package. Finishing a build belongs in `build.sh`, and rebuilding an index that
other packages also feed belongs to `kpkg` (see [Shared indexes](#shared-indexes)).

## Shared indexes

Some files do nothing until an index built from every package's copy is rebuilt: GSettings
schemas, MIME types, fonts, manual pages and a few more. You do not rebuild these yourself.
`kpkgadd` and `kpkgdel`, `kpkg`'s install and remove commands (see
[Packaging](../03-architecture/packaging.md)), read the manifest of the package they installed or
removed and rebuild each index whose directory it touched, once, from what is then on disk:

| A file under | Rebuilds |
|---|---|
| `/usr/share/glib-2.0/schemas/` | `glib-compile-schemas` |
| `/usr/lib/gio/modules/` | `gio-querymodules` |
| `/usr/lib/gdk-pixbuf-2.0/` | `gdk-pixbuf-query-loaders --update-cache` |
| `/usr/share/mime/packages/` | `update-mime-database` |
| `/usr/share/fonts/`, `/etc/fonts/` | `fc-cache -s`, into `/usr/lib/fontconfig/cache`, not `/var/cache`, which the image and every pack exclude |
| `/usr/share/info/` | the info `dir`, regenerated with `install-info` over every page |
| `/etc/udev/hwdb.d/`, `/usr/lib/udev/hwdb.d/`, `/lib/udev/hwdb.d/` | `udevadm hwdb --update`, into `/etc/udev/hwdb.bin` |
| `/usr/share/man/` | `makewhatis`, the `mandoc.db` that `apropos` and `whatis` search; skipped during the build, where `70_image` writes it once over the finished tree |
| `/usr/share/fonts/` | `mkfontdir`, the `fonts.dir` of every subdirectory holding PCF or BDF faces, which Xwayland's core font path reads |
| `/usr/share/texmf-dist/`, `/usr/share/texmf-local/` | `mktexlsr` over those two trees and `/usr/share/texmf-var`, the `ls-R` files through which every TeX program finds a file |
| `/usr/share/applications/` | `update-desktop-database`, the `mimeinfo.cache` that `kdos-appbox open` and the shell's Open With read for a type no `mimeapps.list` names |

A port therefore installs its schema, loader, MIME XML, font, info page, hwdb file, manual page,
TeX file or desktop entry and does nothing else. A per-port hook would rebuild the index only when that port is installed,
not when the next one adds to it or the last one leaves.

`KPKG_SKIP_INDEX` names indexes to leave alone, space-separated, by the name `kpkg` prints in
`index skipped: <name>`: `schemas`, `gio`, `pixbuf`, `mime`, `fonts`, `info`, `hwdb`, `man`,
`xfonts`, `texmf` and `desktop`. The build's chroot phases set it to `man`; a running system does
not set it, and there every index above is kept current.

A missing tool is skipped: the index is written when the package carrying the tool arrives,
because that package's own files touch a watched directory. fontconfig installs no font, so the
font cache also watches `/etc/fonts/`; without it, a system whose fonts all came before fontconfig
would have no cache. A failing tool is a warning, not a failed install. The build step drops
`usr/share/info/dir` and every `usr/share/fonts/*/fonts.dir` from every package (see
[What kpkg does around build.sh](#what-kpkg-does-around-buildsh)): each is the index, and two
packages each shipping one conflict, as `font-misc-misc` and `font-cursor-misc` would, both
installing into `misc/`. A font directory left with no bitmap face loses its `fonts.dir`, and with
it the directory. kpathsea searches the TeX distribution and local trees through `ls-R` only, so a
file another port adds under either is invisible to TeX until the trigger has run; `texlive` ships
the `ls-R` files it writes at build time, and every later install or removal under those trees
rewrites them in place.

The manual index is the one that is merged rather than rebuilt, and only when nothing was removed.
Two packages in three carry manual pages, and reading every page on the system again for each one
would add seconds to every install. The pages an install places go to `makewhatis -d`. A removal,
an upgrade that orphans a page, or a missing `mandoc.db` rebuilds the whole tree, because a merge
cannot drop an entry for a file that is already gone. The build skips this index altogether and
`70_image` writes it once over the finished tree.

Under `--root`, each tool is handed the root-prefixed directory, and `udevadm` and `fc-cache` get
the root itself. The pixbuf loader cache cannot take a directory, because the tool writes the path
it was compiled with. Under `--root` it therefore runs as the root's own
`gdk-pixbuf-query-loaders`, through `chroot`. That needs root, which `kdos update` has when it
installs into the inactive root slot, and `kpkg` warns rather than skips when it lacks it.

## Vendoring

The build has no network, so a program whose language fetches its dependencies at build time
(Rust crates, Go modules, Python packages, Hackage packages) needs them downloaded in advance. That
is vendoring: `ports/fetch` runs the language's own tool against the port's source and packs what
it downloads into `<name>-vendor-<version>.tar.xz` beside the tarball. `build.sh` unpacks that
bundle and builds offline. Set `vendoring =` in the recipe to ask for it; 159 ports do (71 Rust,
38 Go, 45 Python, 4 Haskell, 1 Node).

A bundle with a `sha256 =` line is an archived source like any other: `ports/fetch` takes it from
the port directory, the cache or the source archive first, and generates it only when none of them
has it. A generated bundle is held to the recipe's hash like a download, and a mismatch (a bundle
resolved by other toolchain versions, or against a registry that has moved) is refused. Generating
needs `cargo`, `go`, `npm`, `pip` and `cabal` at the versions this tree compiles with, so by
default it runs in the `kdos-fetch` container (`ports/Containerfile.fetch`, built with Docker or,
failing that, Podman), whose toolchains are built at the versions the `rust`, `go`, `nodejs`, `ghc`
and `cabal-install` recipes name. `KDOS_FETCH_HOST=1` generates with this machine's own toolchains
instead.

| `vendoring` | Produces | Build then |
|---|---|---|
| `rust` | `vendor/`, `.cargo/config.toml` and `Cargo.lock`, from `cargo vendor` | `cargo build --frozen --offline` |
| `go` | The module `vendor/` tree, from `go mod vendor` | `go build -mod=vendor` |
| `python` | Source distributions only (`pip download --no-binary :all:`), including each one's build requirements, followed recursively | Install from the local directory |
| `haskell` | Hackage source tarballs and their revised `.cabal` files, as a local repository | `cabal v2-install` against that repository alone, or upstream's bootstrap script |
| `node` | `node_modules/` and the `package-lock.json` npm writes, from `npm install --ignore-scripts` plus the recipe's `npmflags` | `npm` with `--offline` and the same `npmflags`; `cncjs` is the example |

A recipe with `pypackages` gets a Python bundle whether or not it sets `vendoring`. A Python fetch
that ends with an empty `vendor/` is a failure, not an empty bundle.

`npmflags` is a recipe helper whose words are added to the `npm install` that writes a node bundle
(`npmflags = --legacy-peer-deps` for `cncjs`, whose upstream resolves with Yarn and holds a peer
pin npm's strict check refuses). The lock file rides in the bundle because an offline `npm prune`
or `npm ci` needs one and cannot write it without the registry, and `build.sh` passes the same
flags to its own offline `npm` calls, or npm resolves the tree differently from the bundle.

### Where the bundle goes

Unpack it where the tool will be standing, not beside the manifest. Cargo finds its configuration
by walking up from the current directory, so a build that invokes it from a subdirectory never
reads a configuration placed next to the manifest, and every crate in the bundle resolves as
*missing* while sitting in the vendor directory.

`vendordir` is the other half: it says where the vendoring tool must run, which is beside the
manifest. The two directories are not always the same place. Five ports set it: `gopls`
(`gopls`), `lnav` (`src/third-party/lnav-rs-ext`), `python3-bcrypt` (`src/_bcrypt`),
`python3-cryptography` (`src/rust`) and `ghc` (`hadrian/bootstrap`).

`vendorsync` names every other Cargo manifest the build runs, space-separated and relative to
`vendordir`. A helper crate outside the workspace, such as an `xtask` that generates a manual page,
resolves against its own lock file, so a bundle made from the top manifest alone lacks its crates
and the offline build fails naming the first one. `ports/fetch` passes each entry to
`cargo vendor --sync`, and the one bundle and its one configuration then serve every manifest.
`oxipng` is the example: `vendorsync = xtask/Cargo.toml`, and `build.sh` runs
`cargo run --frozen --offline --manifest-path xtask/Cargo.toml -- mangen` from the top of the tree.

### The three Python keys

By default a Python bundle vendors everything `requirements.txt` and the package's own metadata
name, plus their build requirements. Three keys narrow that.

`pyrequirements = no` says `requirements.txt` is not the dependency set. The file name is a
convention with no defined meaning, and projects often use it for the *optional* list: `visidata`'s
reaches pandas, scipy and a Fortran compiler, for a program whose actual dependency is a single date
library. With the key set, the source distribution's own metadata is vendored instead, and the
fetch prints that it skipped the file. 31 ports set it.

`pypackages` is an explicit closure: space-separated PyPI names, each optionally pinned as
`name:version` (`pypackages = ruamel.yaml vobject:0.9.8`). It is downloaded without resolving
dependencies, because that key *is* the closure the recipe wants. Letting the tool resolve from
there drags in every dependency that is already a port and builds each one's metadata to find that
out. 31 ports set it.

`pyruntime = no` says the runtime dependencies are all ports already, so only what the *build*
needs is vendored. Without it, downloading a runtime dependency such as `numpy` can drag in that
package's own build chain. It has no effect alongside `pypackages`. 27 ports set it.

What a bundle installs into `site-packages` is named, and installed `--no-deps`. pip skips a
requirement that is already installed in the build root, so a resolving install packages whatever
no earlier port happened to install, and which package owns a module then follows build order;
removing the one that owns it breaks the other with an `ImportError`. A module that more than one
program imports is therefore a `python3-*` port of its own in both `depends` lines, never a member
of either bundle: `wcwidth`, `urwid`, `configobj`, `pytz`, `click-log` and `rich` with its
`markdown-it-py` and `mdurl` are the shared ones that `khal`, `khard`, `vdirsyncer`, `toot`,
`ipython`, `python3-esptool` and `ocrmypdf` would otherwise each carry.

A Python package's declared build backend is part of its version pin. Read the build-system
requirements before picking a version: a version that requires a newer backend than the tree
carries can cost several additional ports.

`python3` marks its `site-packages` as externally managed (PEP 668). The marker does not affect
`pip install --root="$PKG"`, `--prefix` or `--target`, so a port's install into its package needs
nothing. A `pip install` into the build root itself, such as the two backends `vdirsyncer` installs
from its bundle before its own build (`expandvars` and `hatch-fancy-pypi-readme`), is refused
unless it passes `--break-system-packages`.

### The Haskell bundle

`vendoring = haskell` writes a `vendor/` directory that cabal reads as a local `file+noindex`
repository: each dependency's `<pkg>-<ver>.tar.gz` from Hackage, the `<pkg>-<ver>.cabal` revision
in force for it beside the tarball, and `SHA256SUMS` over both. `ports/hackage-vendor` does the
downloading, and it checks every file against a hash from somewhere other than the download
before writing it.

The dependency set comes from the first of these that the port has:

- **`hsplan`**, a bootstrap plan inside the source: `plan-bootstrap-<seed>.json` under
  `hadrian/bootstrap` for `ghc`, `bootstrap/linux-<ghc>.json` for `cabal-install`. A plan pins
  each package's revision and both of its hashes. Those two ports build before any `cabal`
  exists, so their `build.sh` lays the bundle out where upstream's `bootstrap.py` looks for
  prefetched sources, and the script builds every package with its `Setup.hs` alone.
- **`cabal.project.freeze` in the port directory**, a freeze pinned by hand. `pandoc` carries one
  because the solver is free to turn a flag off to reach a solution. An unpinned freeze of
  `pandoc-cli` settles on `-lua -server`, a `pandoc` without `--lua-filter` or the server, and on
  the library's `-embed_data_files`, which leaves pandoc's templates in the build's cabal store and
  fails every DOCX and EPUB conversion on the installed system. Write it with
  `cabal freeze --constraint="<pkg> +<flag>"` beside the manifest, and pass the same flags and
  constraints to `cabal v2-install`, so a freeze that disagrees fails the solve instead of building
  the smaller program. A hand freeze pins the program's own library to its version, so a version
  bump rewrites it before `ports/fetch` runs.
- **`cabal freeze`**, run beside `vendordir`'s manifest by the fetch container's GHC and cabal,
  which are the versions the `ghc` and `cabal-install` recipes name. `shellcheck` takes this
  route: its solve needs no flag pinned.

A freeze's tarballs are checked against the sha256 in Hackage's signed index, and each `.cabal`
is the last revision at or before the freeze's `index-state`. A package the resolving GHC ships
in its global package database is left out. The freeze goes into the bundle, and `build.sh`
puts it back beside the manifest, so the offline solve lands on the set that was downloaded:

```bash
tar xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"
cd "${vendordir:-.}"
cp "$SRC_ROOT/vendor/cabal.project.freeze" .
export CABAL_DIR="$SRC_ROOT/cabal"
mkdir -p "$CABAL_DIR"
printf 'repository hackage.haskell.org\n  url: file+noindex://%s\n' "$SRC_ROOT/vendor" \
    > "$CABAL_DIR/config"
cabal v2-install --installdir="$PKG/usr/bin" --install-method=copy exe:<program>
```

The repository takes Hackage's name because the freeze's `active-repositories` line does; under
any other name cabal warns that no repository provides `hackage.haskell.org`. Its URL is the
bundle and no other repository is configured, so a package missing from the bundle fails the solve
by name instead of reaching for the network. `--offline` is not passed, because cabal counts a
`file+noindex` package as a download and refuses every one of them under it.

### A vendor bundle hashes the same twice

Bundles are packed with the tar flag set packages use (names sorted, the pinned timestamp, owner
and group 0, GNU format) and a compressor of their own, single-threaded `xz -9 -T1`, which every
bundle's `sha256 =` line depends on, because a plain archive records the extraction
time and the builder's identity, so two generations of the same bundle would hash differently and
the recipe's `sha256 =` line could never match a regenerated one.

### A bundle no tool writes

`pdfium` carries a vendor bundle that `ports/fetch` cannot produce, because the dependencies it
holds are gclient checkouts and not a language's packages: Chromium's `//build`,
`third_party/abseil-cpp` and `generate_shim_headers.py`. Gitiles generates its archives on request
and no two downloads of one hash the same, so the bundle is the one in the source archive and its
`sha256 =` line is its identity. Rebuild it by hand on a version bump: take each repository at the
revision the new branch's `DEPS` names (`build_revision`, `abseil_revision`), and the script from
the matching Chromium tag, lay them out under `vendor/` as they sit in a checkout, and pack them
with the flag set above plus `--mode=go-w`. The recipe's `_build_rev` and `_abseil_rev` name those
revisions, and `build.sh` refuses a tarball whose `DEPS` disagrees with them. Record the new
bundle's hash in the recipe and publish it with `ports/publish pdfium`: no other checkout can
produce it.

`bat` carries `bat-assets-<version>.tar.xz`, the inputs to its highlighting sets. bat embeds
`assets/syntaxes.bin`, `themes.bin` and `acknowledgements.bin` with `include_bytes!`. These are
serialized syntect sets compiled from 92 git submodules of Sublime grammars and TextMate themes,
and the release archive carries those submodules empty. The bundle is those submodules at the
tag's pinned commits, cut down to what bat's asset build reads: every `.sublime-syntax`, every
`.tmTheme`, and every `LICENSE*`, `NOTICE*` and `COPYING*` file. The cut is not a guess: sets built
from it are byte-identical to sets built from the full checkout. `build.sh` unpacks the bundle over
the empty directories and applies upstream's `assets/patches`. It builds once, runs
`bat cache --build --blank --acknowledgements --source=assets --target=assets`, and builds again;
the second build recompiles only the bat crate. `--source` stays relative, because the syntax set
records each grammar's path as it was given, and an absolute path would make the set's bytes
depend on the build directory. Rebuild the bundle on a version bump:

```sh
git clone --depth 1 --branch v<version> https://github.com/sharkdp/bat
cd bat && git submodule update --init --depth 1
find $(git submodule status | awk '{print $2}') -name .git -prune -o -type f \
    \( -name '*.sublime-syntax' -o -name '*.tmTheme' -o -iname 'license*' \
       -o -iname 'notice*' -o -iname 'copying*' \) -print | sort > list
tar --sort=name --mtime=@1735689600 --owner=0 --group=0 --numeric-owner --mode=go-w \
    --format=gnu --use-compress-program='xz -9 -T1' -cf bat-assets-<version>.tar.xz -T list
```

As with `pdfium`, record the new hash and run `ports/publish bat` afterwards.

The port installs the three sets and a plain-text `acknowledgements.txt` under
`/usr/share/bat/assets`. `delta` and `presenterm` embed bat's sets too, so both depend on `bat` and
copy these files over their own copies before cargo runs. For `delta` the copies are inside the
vendored `bat` crate, and cargo verifies every vendored file against the crate's
`.cargo-checksum.json`. `build.sh` therefore replaces the three entries with the new files'
hashes, and cargo still checks the rest of the crate. The sets are bincode dumps of syntect's
types and carry no version tag. `delta` refuses to build unless its vendored bat is the version
installed, and a bump of `bat` or `presenterm` checks that both `Cargo.lock` files name the same
syntect.

Five more bundles are made by hand, each packed with the flag set above, recorded by its hash and
published with `ports/publish <port>` on every version bump:

| Bundle | What it is, and how to rebuild it |
|---|---|
| `texlive-tlpdb-<version>.tar.xz` | `tlpkg/texlive.tlpdb` at the `texlive-<year>.0` svn tag, the package database the texmf tarball was cut from, which `texlive` and `texlive-doc` read to select their files. tug.org answers the download with 406 unless the request carries `Accept-Encoding: zstd` |
| `koreader-thirdparty-<version>.tar.xz` | Every `base/thirdparty/*/build/downloads` directory after `make TARGET= KODEBUG= download-all`, run in the `kdos-fetch` container with stand-ins for `meson` and `nasm` on `PATH`, leaving out `thirdparty/fonts`, tesseract's `eng.traineddata` and every `*.lock` file, packed with `--mode=go-w` as well |
| `surfer-vendor-<version>.tar.xz` | `cargo vendor` run by the fetch container's cargo over the tag tree with the `f128` and `instruction-decoder` submodule tarballs unpacked in place; `ports/fetch` vendors from the first source alone, where those path dependencies are missing |
| `libreoffice-vendor-<version>.tar.xz` | Every file LibreOffice's `download.lst` names, at the name and sha256 it gives, fetched from `dev-www.libreoffice.org/src` or `/extern` into `externals/` and packed with `--mode=go-w` as well. `--with-external-tar` points the build at them and `--disable-fetch-external` makes a missing one an error; the `--with-system-*` flags decide which are unpacked |
| `librepcb-vendor-<version>.tar.xz` | `cargo vendor --sync libs/slint/Cargo.toml`, run in `libs/librepcb/rust-core` by the fetch container's cargo, one bundle for both Cargo workspaces; `ports/fetch` unpacks only tarballs, and LibrePCB's source is a zip |

### Archives preflight accepts

`testing/preflight.sh` accepts an archive in a port directory if a `source =` line resolves to it,
if it is the vendor bundle, or if it has its own `sha256 =` line; `kpkg` verifies every `sha256`
entry, including the ones no source names. Any other archive fails preflight when git tracks it. An
untracked, gitignored one is a stale fetch that a version change left behind, and preflight lists
it as safe to delete instead (a copy stays in `ports/.srccache`). Preflight also fails an archive
that is empty or whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or tar, which is
what a mirror's HTML error page saved under a tarball's name looks like, and fails any
recipe-hashed archive that git tracks, because sources belong in the archive and not in git.

### A Rust port's version is pinned by this tree's compiler

Cargo refuses a crate whose declared minimum Rust version is higher than the toolchain, rather than
degrading. The toolchain is the `rust` port (1.98.1). Pin the port to the newest release that
builds, because moving past it means bumping the toolchain and regenerating every vendored Rust
bundle together, which is a project of its own rather than a version bump. The fetch container
needs no edit of its own: `ports/fetch` builds it with the versions the `rust`, `go`, `nodejs`,
`ghc` and `cabal-install` recipes name, so it follows the toolchain recipe.

The declared minimum is not an oracle. It gates the refusal; it says nothing about what the code
uses, so a release declaring an older minimum can still fail on a feature stabilised later. The
only reliable test is compiling.

## A second version beside the first

Some software cannot move to the version the tree ships, and gets an older one installed beside it.
A port that installs such a second version beside the tree's main one is called a slot in this
chapter: `llvm21`, `clang21`, `lld21`, `lua54` and `openssl3` are slots. (It is unrelated to the
A/B root slots of an installed disk.) Neither copy may write a path the other writes, or `kpkg`
reports a conflict and one package shadows the other. There are three ways to keep them apart.

**Versioned names.** `lua54` renames every path it installs: the interpreter is `lua5.4`, the
headers are in `include/lua5.4`, the library is `liblua5.4` and the pkg-config file is
`lua5.4.pc`. This works when the upstream build lets every name be chosen.

**The runtime only.** `openssl3` installs `libssl.so.3` and `libcrypto.so.3` and nothing else: no
headers, no `libssl.so` link, no pkg-config file, no `openssl` program and no manual pages. Ports
build against `openssl`, which is 4.x and whose sonames end in `.4`. The slot exists for code that
cannot: a binary built elsewhere against OpenSSL 3 (an AppImage, a vendor tool, a `.so` a program
loads), which the loader otherwise refuses for want of `libssl.so.3`, and `qt5-qtbase`, whose
QtNetwork does not compile against OpenSSL 4's const-corrected accessors. That port generates the
OpenSSL 3.5 headers itself, from the same release and with the slot's own configuration, and links
`libssl.so.3` and `libcrypto.so.3` by path. Nothing else QtNetwork links may bring OpenSSL 4 into
the process: musl's loader has one symbol namespace, so with both `libcrypto` sonames loaded each
resolves the other's calls. It follows the 3.5 long-term line (`series = 3.5`), so a binary that
needs a symbol added in 3.6 still fails to load. It is built `no-module`, so the legacy provider is inside
`libcrypto.so.3` and the slot never loads OpenSSL 4's `ossl-modules/legacy.so`, and it reads the
same `/etc/ssl/openssl.cnf` and certificate store as `openssl`.

**A private prefix.** `llvm21`, `clang21` and `lld21` install everything under `/usr/lib/llvm21`:
`bin`, `lib`, `include` and `lib/cmake`. LLVM needs this, because its sonames already carry the
version, but its headers (`include/llvm`), its CMake package (`lib/cmake/llvm`) and its tools
(`llvm-config`, `clang`) do not. Nothing under the prefix is on `PATH` or on the linker's search
path, so every consumer that does not ask for the slot still finds the system LLVM. The prefix is
the recipe helper `_prefix`, derived from `version` as `/usr/lib/llvm${version%%.*}`. The slot is
built the way the system LLVM is: `llvm21` and `clang21` as one shared object per component,
`lld21` as static archives, all under the prefix, and the programs and shared libraries find each
other through their `$ORIGIN/../lib` run path. zig links `libclang-cpp.so`, which clang builds in
either shape.

A consumer asks for the slot with build flags and a `depends` line naming the slot ports instead
of `llvm`. `zig` passes `CMAKE_PREFIX_PATH=/usr/lib/llvm21`, which makes its `llvm-config` search
look in the prefix before `PATH`. The zig binary it produces records `/usr/lib/llvm21/lib` as its
run path, because zig adds the directory of every shared library it links to the run path when it
builds for the host.

The LLVM ports take their sources in two ways. From LLVM 22 on, upstream publishes only the whole
`llvm-project-<version>.src.tar.xz`. Each port of the current series (`llvm`, `clang`, `lld`,
`lldb`, `libclc`, `libunwind`, `openmp`, `compiler-rt`) carries that one tarball and configures
its own directory with `cmake -S <dir>`. The runtimes (`libunwind`, `openmp`, `compiler-rt`)
configure `runtimes` and name themselves in `LLVM_ENABLE_RUNTIMES`, which is the build upstream
supports for them: `openmp`'s own directory refuses to configure on its own. `compiler-rt`
installs into clang's resource directory, `/usr/lib/clang/<major>`, where the driver looks for
`libclang_rt.*`, and `openmp` installs `libomp` without its `libgomp.so` alias, which is gcc's.

The 21 slot uses the per-component tarballs that upstream published for 21.x. `lld21` carries
`libunwind-<version>.src.tar.xz` as a third source and names its `include` directory in the C++
flags, because the Mach-O linker includes `mach-o/compact_unwind_encoding.h` and the header must
come from the same release as the linker. The linker's own include path is
`$LLVM_MAIN_SRC_DIR/../libunwind/include`, which does not resolve in a standalone build: there is no
LLVM source tree beside it, and a path through a missing directory fails even when its target
exists. The `libunwind` port installs that header only under its private prefix, which is not on a
default include path, and it is the current series, not 21.

LLVM's `libunwind` is the one current-series port under a private prefix,
`/usr/lib/llvm-libunwind`. `/usr/include/libunwind.h`, `libunwind.so` and the `libunwind*.pc`
files belong to `libunwind-nongnu`, the library that unwinds another process through
`libunwind-ptrace` and that `htop`, GStreamer, `libcamera` and `samba` link. Both own the same file
names, so the LLVM one stays out of every default search path, and no port depends on it.

## A second build of the same source

A port builds once, so a package whose full build needs something that itself depends on the
package is split in two. `glib` builds with introspection off, because `gobject-introspection`
depends on `glib`. `glib-introspection` builds the same glib tarball again with
`-D introspection=enabled`, against the installed `glib` and `gobject-introspection`, and packages
only the GIR and typelib files that build writes: `GLib-2.0`, `GLibUnix-2.0`, `GObject-2.0`,
`GModule-2.0`, `Gio-2.0`, `GioUnix-2.0` and `GIRepository-3.0`, under `/usr/share/gir-1.0` and
`/usr/lib/girepository-1.0`. It installs the rest of the build into a staging directory inside the
source tree and copies those two sets into `$PKG`. Packaging anything else would give two
packages the same path.

The second port carries the tarball under the first port's file name, through
`glib-$version.tar.xz::<url>`, with the same checksum, so the source archive holds one asset and
the cache one file for both paths. Its `version` must equal the first port's: the typelib describes
the library installed beside it. Both recipes carry `group = glib`, so the version checker offers
the two only as one bump; `download.gnome.org` is no forge, and without the key each would be
offered alone. Its meson options are the first port's, except the one being turned on.

A port that builds introspection data from a glib library names both `gobject-introspection` and
`glib-introspection` in `depends`: `g-ir-scanner` comes from the first, and every GIR it writes
includes `GLib-2.0`, `GObject-2.0` or `Gio-2.0` from the second. `libqrtr-glib`, `libmbim`,
`libqmi`, `modemmanager` and `poppler` do.

`python3` is split the same way for Tk. It is built before `tk`, so `_tkinter` is off and the
Tk-only standard library is removed from its package. `python3-tkinter` configures the same
tarball again once `tk` is installed, builds only `_tkinter`, and packages that module with the
`tkinter` package. It carries `python3-$version.tar.xz` with `python3`'s checksum, its `version`
must equal `python3`'s because the module is compiled for that interpreter's ABI, and both recipes
carry `group = python3`.

The arm-none-eabi C++ runtime is split the same way. `gcc-arm-none-eabi` is built with no C
library and installs only the compiler and `libgcc`; `picolibc-arm-none-eabi` is compiled with
that compiler; `libstdcxx-arm-none-eabi` configures the same gcc tarball again with the
compiler's prefix, sysroot and `rmprofile` multilib set, plus `--with-picolibc`, and installs only
what `make install` in its `libstdc++-v3` directory writes: `libstdc++.a`, `libsupc++.a` and
`libstdc++exp.a` for every multilib, and the headers under `/usr/arm-none-eabi/include/c++`. The
top-level `install-target-libstdc++-v3` would install `libgcc` again, and the pretty-printers under
`/usr/share/gcc-<version>` are the host `gcc`'s path, so neither is packaged. It carries
`gcc-arm-none-eabi-$version.tar.xz` with that port's checksum, and both recipes carry
`group = gcc-arm-none-eabi`. `picotool` depends on it: the three RP2350 stubs it embeds are
pico-sdk projects, and the SDK compiles C++ into every one linked against `pico_stdlib`.

Preflight has a related check for ports that build one upstream tarball under two recipes: `perf`
is `tools/perf/` inside the kernel tree and carries its own copy of `linux`'s version and hash, and
preflight fails the tree when the two disagree.

## Checking a recipe change

`kpkg verify` builds a port twice and reports how the two packages differ, on a KDOS system or in
the build chroot. Write the changed recipe as `kpkgbuild.new` beside the current one (and, if the
build changes too, `build.sh.new`; without it the current `build.sh` is used), then:

```sh
kpkg verify <port>            # build with kpkgbuild and with kpkgbuild.new, compare the two
kpkg verify --repro <port>    # build the SAME recipe twice; require byte-identical results
```

Neither build happens in the ports tree: a scratch directory under `/tmp` is filled with symlinks
to everything the port carries, and only the recipe and build script being tested are real copies.
An interrupted verify leaves the tree untouched.

The comparison is over payload, not the file list: a file list shows whether the same paths exist
and says nothing about the same paths with different contents, which is the difference a recipe
change is most likely to make. The comparison is a per-member fingerprint including modes and link
targets, and it prints the first differing lines.

`--repro` is the acceptance test for reproducible packaging, and it is the same code path because
it is the same question asked of two archives.

## Checking for new versions

The version checker reports which ports have an upstream release newer than their pin, the version
the recipe currently names, and can apply the bump for you. It is `kdos-portup` (source in
`src/devtools/kdos-portup`), run through `ports/update`, which compiles it into `ports/.portup` the
first time and whenever a `.c` file under `src/devtools/kdos-portup` is newer than the binary. A
changed header, or a change to a `libk*` library it links, does not trigger that; delete
`ports/.portup` to force a rebuild. It needs network access, `curl` and `git`, and it never runs
version control on your tree.

```sh
make updates                                   # the whole tree, interactively
make updates PORTUP_ARGS="--check curl"        # one port, report only
make updates PORTUP_ARGS=--cve                 # also ask repology which pins are vulnerable
make updates PORTUP_ARGS="--jobs 1 --refresh"  # one check at a time, ignoring the cache
ports/update zlib openssl                      # the same tool, called directly
```

| Option | Does |
|---|---|
| *(none)* | Check every named port, or every port, then review each available bump interactively |
| `--check` | Report only: no prompts, nothing rewritten. Exit 1 when any update exists |
| `--json` | Print the results as JSON instead of the review |
| `--no-fetch` | When a bump is accepted, rewrite the `version =` line only; fetch nothing and leave the `sha256 =` lines alone |
| `--refresh` | Ignore the day-long result cache |
| `--cve` | After the review, ask repology's vulnerability flag about every pinned version, one request a second |
| `--jobs <n>` | Run `n` checks at once, 1 to 32. Default 8 |
| `--selftest` | Replay the recorded responses under `testing/fixtures/portup` and check the tool's own logic, offline |
| `--fixture <dir>` | Answer every request from a recorded corpus instead of the network |

In the review, each offered bump (or [group](#outcomes) of bumps offered together) waits for one
letter: `y` accepts it, `n` skips it, `d` shows the proved URL and the recipe diff, `a` accepts
this and every later one, and `q` stops.

### Accepting a bump

Accepting a bump for one port rewrites its `version =` line, runs `ports/fetch <port>`, and then
rewrites each `sha256 =` line whose file name carries the old version to the new file's hash,
which covers the vendor bundle as well as the source. If the fetch fails, the old version is put back. It then prints two notes: the old tarball
is still in the port directory (delete it when nothing needs it), and `ports/publish <port>`
is the next step, because the recipe names a hash the source archive does not hold yet. Publishing
and committing stay your decisions.

A group bump is accepted for every member or none: each member's `version =` line is rewritten, each
is fetched, and a failed fetch for any member puts every member back. The `sha256 =` lines of a
group bump are not rewritten; record each member's new hashes by hand, as after `--no-fetch`.

Exit codes: 0 means every named port is current (or every bump was applied), 1 means `--check`
found at least one update, and 2 means a fetch failed and putting the old version back failed too,
leaving a recipe that names a version with no archive on disk. The tool prints the port and both
versions; edit the recipe back by hand before building. `make updates` passes 0 and 1 and fails on
2 or anything else.

### Steering the checker from a recipe

Most ports need nothing: the checker works from the recipe's first `source` URL and its
`homepage`. Four helper keys correct it when its answer is wrong for a port.

| Key | Use it when |
|---|---|
| `watch` | Upstream lists its releases somewhere nothing reached from the source URL leads: a forge repository, or a page, listing, JSON index or manifest naming the release files. See [Discovery](#discovery) |
| `series` | The port stays on one line (`21` for `llvm21`, `5.4` for `lua54`), so a release past it is never offered. See [Filtering](#filtering) |
| `devseries` | Upstream marks development releases by number: `odd-minor`, `preview-minor`, or both. See [Filtering](#filtering) |
| `group` | Ports the derived grouping would not join must be bumped together: the pairs in [A second build of the same source](#a-second-build-of-the-same-source), the Qt modules, and the other pairs built from one release (`qca`, `qwt`, `qscintilla`, `mgba`, `supertuxkart`, `webkitgtk`, `texlive`). See [Outcomes](#outcomes) |

### Outcomes

There are three outcomes: *newer*, *current* and *unknown*. *Unknown* is never folded into
*current*, because that would be a confident wrong answer, the one kind this tool must not give.
Each of these is unknown, with a reason that names the newest version upstream when one was seen:

- a listing that could not be reached, or whose tail was cut off by a cap or a failed transfer
  (archive indexes sort ascending, so the dropped entries are the newest);
- a later series directory that could not be listed;
- a tag list naming a later release under another prefix;
- a port whose candidates all failed to resolve, naming upstream's own copy of the newest when its
  site links one;
- a source URL with no version in it, a source that is no URL, and a recipe whose version is not
  written in its source URL (a bundle numbered apart from its sources);
- a pinned commit no release branch is at;
- a SourceForge feed ending at the pin, for a project whose new repository tags later or cannot be
  read, or that repology knows a later release of.

A *current* answer carries a reason too when there is more to it: a series held, or a pinned commit
at a branch tip. A *newer* one may name a GitHub tag with no release, marked low-confidence.

Checks run eight at a time (`--jobs`), each in its own process; a whole-tree run takes minutes
rather than an hour, and a check that dies before reporting leaves its port unknown. Results are
cached for a day in `ports/.update-cache.json`, each stamped with the checker logic that reached
it; an entry from other logic is checked again, so a changed checker never serves its
predecessor's answers. `--refresh` ignores the cache, and a `--fixture` run neither reads nor
writes it.

Ports are grouped so that a release family is offered as one bump. The grouping key is the forge
organisation plus the current version, not a name prefix: a name rule misses the member of a
family whose repository is named differently, while all of them resolve to the same organisation
and version. A `group` key overrides the derived one. The pairs in
[A second build of the same source](#a-second-build-of-the-same-source) set it, and so does a
family released from a host the derivation does not read, such as the Qt modules from
`download.qt.io`.

The reasons behind each outcome are under [How the checker decides](#how-the-checker-decides).

### How the checker decides

A recipe author needs this only when the checker's answer for a port is wrong. The checker asks one
question per port, whether upstream has a release newer than the pin, and answers it in five
steps.

1. **Discover.** Ask upstream what it has released, through the first adapter below that finds
   anything.
2. **Read.** Take a version out of every name the adapter returned, through the recipe's own URL.
3. **Filter.** Drop what is not a later release of this numbering: another scheme, a pre-release, a
   development series.
4. **Compare**, walking from the highest candidate down.
5. **Prove.** Render the candidate through the real recipe parser and request the result. The
   recipe is copied to a temporary directory, the version substituted, and the metadata expanded by
   the same parser the build uses, so a chain of helpers expands exactly as it would in a build and
   no probe ever touches the real tree. A not-found drops that candidate and tries the
   next-highest.

Correctness comes from the last request, not from discovery. A listing can name a version whose
archive lives somewhere the recipe's template does not expect, so the tool moves to the next
candidate rather than report a version whose archive has not answered a request.

#### Discovery

The URL a recipe's first `source` names, with any `filename::` prefix removed (that half is where
the archive lands, not where it comes from), decides which adapters apply. They are tried in this
order:

| Adapter | Applies to | Asks |
|---|---|---|
| Watch | A recipe with a `watch` key | That forge repository's tags, or that page read through the file name, one hop further as the homepage is |
| Registry | `files.pythonhosted.org`, `static.crates.io`, a CPAN `authors/id/` path | PyPI's JSON (withdrawn and empty releases skipped), the crates.io API (yanked versions skipped), MetaCPAN's latest release |
| Forge | GitHub (and `raw.githubusercontent.com`), Codeberg, sr.ht, Bitbucket, GitLab on any host whose URL carries `/-/`, a cgit `/snapshot/` URL | `git ls-remote --tags`. When git cannot answer: GitHub's tag and release feeds, Codeberg's and sr.ht's feeds, GitLab's tags API, cgit's tag page. When nothing answers and the URL names a commit, the branch heads (below) |
| SourceForge | `downloads.sourceforge.net`, `sourceforge.net/projects/…/files/`, a project web host | The project's file feed, narrowed to the path above the first directory named for the version |
| Directory | Anything else | The listing of the archive's directory (or, when a directory in the path is named for the version, of its parent; see below) and the download page one hop from either |
| Homepage | A recipe with a `homepage`, whose source is on no forge or registry and names its version in the file name | The homepage's forge's tags, or the page's links read through the file name, and one hop further |
| Repology | Every port, last | The package-tracking service, rate-limited to one request a second across the whole run and marked low-confidence; it is never upstream itself |

Tags come from git rather than a forge's feed because git names every tag, unauthenticated and
unmetered; a feed carries the newest ten, which a project that tags each of its crates, or maintains
two majors at once, fills with the wrong ones. git runs without the user's own configuration and
over https only, so a `url.*.insteadOf` rewrite to ssh cannot ask for a key from every worker at
once, and with terminal prompts off, so a repository that has moved or gone private answers "no"
instead of asking for credentials.

Git also names the commit behind every tag. A tag on the commit of a lower release, with a release
of other code between the two, is a mistake rather than a release and is dropped: thermald's
`v2.15.10` sits on `v2.5.10`'s commit, and offering it would downgrade 2.5.13 under a higher
number. A tag on the commit of the release just before it stays (sby tags every yosys version,
changed or not), and so does a release sharing its commit with a series tag that follows it
(corrosion's `v0.6` on `v0.6.1`).

A GitHub project that publishes a release for some of its tags and not for others leaves the tags
past its newest release in doubt. They may be engineering drops (intel/media-driver tags `26.2.1`
to `26.2.4` and publishes `26.2.4`, and its `26.3.x` tags are the next quarter's) or releases whose
release object is late: bindgen's `0.73.x` were on crates.io before GitHub had a release for them.
Nothing in the tags tells the two apart, so they stay candidates. When a tag later than the pin
exists, the releases API is asked; if a tag of the pin's numbering between its oldest and newest
release has none, and the pin is no later than the newest, the answer is marked low-confidence and
names the newest tag without a release. The API allows sixty unauthenticated requests an hour and
is asked only for a port that already has a later tag; when it refuses, the tags stand unmarked, so
the verdict is the same either way.

When the recipe's tag prefix names nothing later than the pin, the same list is read again with a
`v` or no prefix. A family there that starts after the recipe's own ends is a change of scheme
(sby's tags include both `yosys-0.47` and `v0.48`), and its newest release makes the answer
unknown, naming it, since the recipe's template cannot fetch it and *current* would be wrong. A
family whose numbers run alongside the recipe's is another project in the same repository
(golang/tools' `v0.50.0` beside `gopls/v0.23.0`) and says nothing.

A listing is not always where the archive is. The directory adapter reads it through the rules
below, each of them general to a kind of host rather than to a port:

- **A stand-in page is followed.** A meta refresh (`curl.se/ca/` sends a browser to
  `/docs/caextract.html`), or an iframe that is the whole of a page with no versioned link of its
  own (`nethack.org/download/`), is replaced by the page it names.
- **An S3 bucket is listed at its root.** `s3.amazonaws.com/<bucket>/<prefix>/` answers 403, and
  the bucket's `?prefix=<prefix>/` lists the keys. A listing S3 marks as cut leaves the answer
  unknown.
- **A page that names none of the port's files leads one hop further.** Links on the same host
  whose last segment names a download (`download.html`, `downloads.html`, `download.php`,
  `download/`, `TestDisk_Download`), releases (`releases.html`), or the current or latest release
  (`mpfr-current/`) are read the same way, those below the page's own directory first, three at
  most. A directory that cannot be read at all (403, 404) is replaced by its parent, which is
  usually the project's page, as netfilter's `files/` sits beside `downloads.html`.

Only a name matched through the recipe's file name counts on any of these pages, because a version
number in running text is not evidence of a release file.

SourceForge's mirrors keep every file a project uploaded after the project leaves, so a file feed
whose newest file is the pin is not proof of *current*: libjpeg-turbo releases on its own site,
while its SourceForge feed ends at `3.0.1`. When the feed ends at the pin, the project's own
SourceForge record is asked. A `moved` status sends the check to the repository it names: tags there
that name the pin and nothing later mean the project still uploads its releases to SourceForge
(procps-ng, psmisc) and the answer is *current*; anything else (a later tag, no tag of the pin, a
repository git cannot list) makes it unknown, naming where it went. The tags are read through `v`,
no prefix and the file's own, and failing those as a word followed by nothing but numbers and
separators (smartmontools' `RELEASE_7_5`). Without a move repology is asked, and a version it calls
upstream's newest that is later than the pin, of the pin's line and no pre-release, makes it unknown
too; a project repology cannot answer for stays current.

A source that names a commit, in a repository that tags no release, has one question left: is the
commit still the tip of the default branch or of a branch named for releases (`master`, `main`,
`stable`, `trunk`, `release…`)? At the tip, the port is current, and says so; anywhere else it is
unknown. A version from any other adapter cannot be rendered into such a URL, so none is asked.

`watch` is for the host no general rule reaches: a release index served as JSON to a script
(`downloads.unidata.ucar.edu/<project>/release_info.json`), a channel manifest
(`static.rust-lang.org/dist/channel-rust-stable.toml`), a browsable mirror of a host that serves
no index (`build.openvpn.net/downloads/releases/`), or a forge repository nothing on the project's
site links. It is tried first and still read through the recipe's own file name. Five ports set it:
`fontconfig`, `netcdf-c`, `openvpn`, `rust` and `udunits`.

A version written into a directory (`gnu/gcc/gcc-15.2.0/`, `ftp/python/3.14.2/`,
`sources/pango/1.57/`, `dist/v8/`, `kernel/v7.x/`) means the archive's own directory never holds a
newer release. The checker lists the parent of the outermost such directory and reads its siblings
through that directory's own name, keeping those at or after the current one.

- **Siblings named for a whole version** (`gcc-16.2.0/`): the three newest are read, so a `3.15.0/`
  holding only `3.15.0a1` is not taken for a 3.15.0 release. A newer one past those three stays a
  candidate unread, for the proof step to settle, and so does one whose files the recipe's archive
  name cannot read (no version in it, only its directory's), since nothing in it can be judged a
  pre-release.
- **Siblings named for a series** (`1.58/`, `v9/`): the three newest are read, and the current
  series as well, since only a series' files say which releases it holds. A later series that could
  not be listed leaves the answer unknown, never current.

A sibling is a link below the listed directory, resolved against the page it was read from: a
link in the page's chrome (a footer's `linkedin.com/company/29561/`) is no later series. A page
whose versioned links all lead elsewhere is not a listing, and its text is not read for siblings
either; launchpad's series page names "Ubuntu RTM 14.09". A walk whose parent names no sibling at
all, not even the current one, reads that page's download links instead (`mpfr.org` links only
`mpfr-current/`).

A walk makes at most eight requests.

#### Reading a version

The recipe's URL says where its version sits: `gcc-15.2.0.tar.xz` is `gcc-` then the version then an
archive suffix, and a tag `llvmorg-21.1.8` is `llvmorg-` then the version. A name is read only when
it has the same text on either side, any archive suffix standing in for the recipe's. A directory
holding every X library, every suckless tool or every GNU pretest then yields this port's versions
and nobody else's. The version may be spelled with `_` or `-` for its dots (`boost_1_89_0`,
`R_2_7_3`), with a mix of them (`ImageMagick-7.1.2-31` for `7.1.2.31`) or with none at all
(`gs10071`, `unzip60`), and reads back dotted. A pin of digits alone may be written in groups
(`2026-08-13` for `20260813`) and reads back with the separators dropped, at the same group widths
only. Only a URL with no version in it at all falls back to extracting every version-shaped run and
keeping those shaped like the pin.

#### Filtering

- **Class.** Dotted numbers of any length are one numbering (binutils 2.45.1 and 2.47 are one
  numbering), with a pre- or post-release marker (`1.4rc5`, `10.2p1`, `1.5.8.pl02`) or a trailing
  commit id set aside. A leading year and a zero-padded part the pin does not pad (`600.0132`
  beside `26.2.4`) are each another. So is any word that is not a pre- or post-release marker, a
  lone letter straight after a digit (`3.6a`, `1.1.1w`), or a word the pin itself carries
  (`1.9.0.jumbo1`): a platform build (`3.8.13-w64`), a patch beside a release (`1.8.1.3.patch`), a
  variant (`5.1.22_dict`) and an archive's own suffix (`56.7z`) are files, not releases.
  Repology's strings are distributions' spellings and must match the pin's exact shape.
- **Pre-release.** `rc`, `alpha`, `beta`, `pre`, `preview`, `dev`, `snapshot`, `wip`, `test`,
  `nightly`, `unstable`, `trunk`, `cr`; and PEP 440's `a1`/`b2`, though never inside a trailing
  commit id (passt's `2025_02_17.a1e48a0`). A port that pins a pre-release follows that line.
- **Pretest.** 90–99 in the third place or later (`1.25.91`, `4.4.0.90`, `26.0.99.902`) where the
  pin has less there, unless the listing or the pin has 80–89 in that place with the same leading
  numbers: a counter walks up through the eighties (`1.0.89`, then `1.0.92`), a pretest jumps
  there. 100 and up is always a counter. A registry's answer is exempt, since the registry marks
  its own pre-releases.
- **Development series.** Declared by the recipe's `devseries` key, space-separated: `odd-minor`
  (an odd second number is a development series, as in GLib and Perl) and `preview-minor` (a
  second number of 90 or more previews the next major: Pango 1.90 is Pango 2).
  `gstreamer.freedesktop.org` is `odd-minor` without asking. `download.gnome.org` is not: libxml2
  2.15 and librsvg 2.63 are stable there, so its odd-minor projects carry the key. 25 ports set it.
- **Series.** A port that stays on one line declares it with the `series` key: a version prefix,
  matched on whole components, so `21` holds `21.1.8` and not `210.1`, and `5.4` holds `5.4.9` and
  not `5.5.0`. A release past it is never a candidate, and a directory walk skips a sibling that
  holds none of the line. The newest such release is remembered: a port with nothing newer in its
  line is *current*, and the answer names what is past it, as in `held to series 21; newest
  upstream 23.1.2`. The slots (`llvm21`, `clang21`, `lld21`, `lua54`, `openssl3`)
  carry it, so do ports held to an upstream major line (`docbook-xml` on 4, `openldap` on 2.6,
  `pngquant` and `zxing-cpp` on 2), and so does a hold a consumer forces (`python3-pydantic-core`, at the one
  version the pydantic vendored in `ocrmypdf` names).

#### Proving

A rendered URL identical to the recipe's own proves nothing (the version is not in it) and is
never requested. A host that answers HEAD 403, 405 or 501 is asked again with a one-byte ranged
GET, since refusing a method is not the same as having no such file. Three misses in a row within a
major newer than the pin's skips the rest of that major: a template that cannot fetch its three
newest releases cannot fetch any of them (`SDL2-<v>.tar.gz` under SDL 3's tags). At most twenty
candidates are requested per port. A proved candidate that is not the newest upstream names comes
with the newest in its line (a series directory written `${version%.*}` turns `0.21.8.2` into a
`0.21.8/` upstream never made), so the review shows what the template cannot fetch.

When every candidate misses, the newest one's file (the name the template gives that version) is
looked for on upstream's own site: the recipe's homepage, and the download pages one hop from it
that the directory adapter would follow. A link to exactly that file, answering a request of its
own, leaves the answer unknown, since the template still cannot fetch it, but carries the file's URL
and names its host: chafa tags on GitHub and uploads to its own site, so a GitHub template reads
`newest 1.18.3 is at hpjansson.org, not at the recipe's URL`, and the recipe's `source` is what
changes. A page that merely mentions the file proves nothing; only the request does.

An HTTP request is tried three times, pausing two and then five seconds, when the answer is one a
busy host gives and a missing file does not: no response at all, 429, or 5xx. A 200 whose body
stopped part-way counts as no response, since a cut listing is missing its newest entries, and so
does a redirect whose next hop never answered. A git tag listing has no status to read and is
tried once more, after two seconds, on any failure. Repology's retries wait for their turn under
its one-a-second limit like any other request, and a 429 from it waits ten seconds first.

## Publishing sources

Upstream archives are not committed. Git carries the recipe, and the archive it names is a release
asset in the `kunaldawn/kdos` repository: numbered pre-releases, the volumes `sources-1`,
`sources-2` and on, hold the files of the shelves `ports/shelves` maps to each, as
`<shelf>--<file>`, and the committed file `ports/sources.idx` says which volume and which asset
hold each hash. That is where every other checkout's `make fetch` looks for
it first (see [Where sources come from](developing.md#where-sources-come-from)). A new or bumped
source therefore has to reach the archive before the commit naming it is pushed, or the commit
builds on the machine that wrote it and nowhere else. The same holds for the index line saying
where it went, which is why `ports/sources.idx` is committed with the recipe. Patches,
configuration files and anything else git tracks are not archived, even when a recipe hashes them.

The `sha256 =` line is a source's identity, and its URL and its asset name are only addresses.
With a hash, the local cache, the archive and upstream are interchangeable, and the nearest is
used; `ports/fetch` keeps no copy that fails its hash, and `ports/publish` uploads none.
Immediately after a version bump, before the new hash is recorded, upstream is the only possible
source: the archive cannot hold a file that has never been hashed here, and another copy of a file
with no recorded hash cannot be verified.

Uploading needs a token with write access to `kunaldawn/kdos`, so publishing is a maintainer's
step. If you are contributing without that access, say in your pull request which ports carry new
or changed sources; `ports/publish --check <port>` shows which of their hashes the archive lacks.

A file git tracks is never archived and never fetched: `ports/fetch` skips every path in the git
index, so a source tarball that is still in the index (as a Git LFS pointer, for instance) is
neither downloaded nor checked by `ports/fetch` or `make fetch-check`. `ports/publish` and the
pre-push hook do treat an LFS pointer as not carried and publish or check the real file.
Preflight fails every recipe-hashed archive git tracks; take such a file out of the index with
`git rm --cached <file>` so the fetch path handles it.

A version bump, end to end:

```sh
ports/update <port>                 # accept the bump: version and sha256 lines are rewritten, the source fetched
make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"   # the phase whose list names it
ports/publish <port>                # upload what the archive lacks and write its lines into ports/sources.idx
                                    # (maintainer token; else --check and say so in the PR)
git commit ports/core/<shelf>/<port> ports/sources.idx   # the recipe and the index; the archive itself is gitignored
git push                            # the pre-push hook checks the layout and that every new hash is archived
```

After `ports/update --no-fetch`, or after a group bump, the `sha256 =` lines still name the old
files. Run `ports/fetch <port>` to bring the new files down from upstream (it warns that each has
no hash), then replace the old lines with the new hashes from `sha256sum`. Until you do, `kpkg`
refuses the unhashed archive and `ports/publish` has nothing to publish.

### `ports/publish`

```text
ports/publish [--dry-run | --check] [--history] [port…]
ports/publish --plan [--dry-run]
ports/publish --describe [--dry-run]
ports/publish --rehome [--dry-run] [port…]
ports/publish --retire [--dry-run]
ports/publish --orphans [--prune=yes-delete]
ports/publish --freeze <kdos-tag> [--dry-run | --check]
ports/publish --release <kdos-tag> [--iso <path>] [--key <path> | --unsigned] [--publish] [--dry-run]
```

`ports/publish` uploads every file a `sha256 =` line names, under all of `ports/core` or only the
named ports, that git does not carry and the archive does not hold yet. A port is named by its
bare name, on whatever shelf it sits, and a named port that does not exist fails the run, with
`--check` as without it. A path git tracks as a Git LFS pointer is not carried: the pointer names
the file and is not the file. The bytes come from the port directory, the cache, or the local LFS
store, and are hashed again immediately before each upload, because an asset whose bytes are not
the hash its index line names would poison every checkout that asks for it.

A file is present when `ports/sources.idx` names its hash and an anonymous `HEAD` on the asset's
download URL (on every part's, for a file in parts) answers 200; the `HEAD` costs no API quota. A
hash the index does not name is missing without asking. A rerun therefore uploads only what is
still missing, and an interrupted run is resumed by running it again. Only a 404 counts as
missing; any other status, or a network failure, stops the run with `archive unreachable`,
because an outage proves nothing about what the archive holds.

**Which volume.** Recipes are read shelf by shelf and port by port, both in C-locale order. A
hash belongs to the shelf of the first port that names it, goes into that shelf's volume, the
number the second field of its `ports/shelves` line gives, and takes its label `<port>/<file>` from
that port. A file only old history names (`--history`) is shelved as `attic` and goes into the
highest volume. A missing volume is created as a pre-release with `make_latest` off, titled
`Sources N: <first shelf> … <last shelf>` after the shelves mapped to it, and a volume found to be a
full release is changed into a pre-release, so the repository's latest release is always a KDOS
system release. A release holds at most 1,000 assets, parts counted one by one. A file its volume
has no room for goes to the lowest-numbered volume with room, or else opens the next volume, and
the run prints a `spilled` line; the index records the volume it went to, so `make fetch` finds it
there, and `--rehome` moves it home once its own volume has room. A file the index names in a
volume but GitHub lacks goes back to the volume and the asset name its line records.

**Which name.** A file is stored as `<shelf>--<file>`, as GitHub will store it: every character
outside `[A-Za-z0-9._-]` becomes `.`, so `libsigc++2-2.12.1.tar.xz` on the `gtk` shelf is
stored as `gtk--libsigc..2-2.12.1.tar.xz`. When that name is taken in the volume, by another
hash's index line (compared ignoring case) or by an asset of other bytes, the next of
`<shelf>--<port>--<file>` and `<shelf>--<port>--<first 12 hex digits of the hash>--<file>` is
tried. The upload carries the label
`<port>/<file>`, which the release page shows in place of the stored name. The index records the
name GitHub returned, and prints a note when it differs from the name asked for. A run that
stops after such an upload and before its index line finds it again by digest and label, since
the name asked for is not in the release, and adopts it.

**Files in parts.** A file larger than 1900 MiB (`KDOS_PART_SIZE`) is cut into parts of that size
under `ports/.srccache/.parts/<hash>/`, never under `/tmp`, and each part is uploaded as
`<asset>.part01` … `<asset>.partNN`. Its index line, carrying every part's hash, is written only
after every part is stored with a verified digest, and the staging directory is removed when the
run ends. A run that stops midway leaves the parts it stored; the next run finds them in the
release listing with the right digests and adopts them without sending them again.

**The index.** An upload is accepted only when GitHub reports its digest as the expected hash.
Its line, `<hash> <tag> <asset> <port>/<file>` with `parts=<N>:<h1>,…,<hN>` after it for a file in
parts, replaces any line for that hash at once, the file is sorted, and the result is checked
against every rule preflight holds the index to before it replaces the index; a run that dies
keeps every line it earned. `ports/publish` refuses to write an index that is not format 2, or
one that already breaks those rules. **Commit `ports/sources.idx`**: `make fetch` and the pre-push
hook read it from the tree, and a file no committed line names cannot be found.

**Release notes.** Every release a run touches has its notes rewritten from the index, and a
volume its title. The notes hold one section per shelf, headed by the shelf's name and its
description from `ports/shelves`, then a table of port, version, file (linked to its asset, or to
each part), size and SHA-256; a file sits under the shelf its asset name starts with, and
`attic--` files under `attic`, last. Past 120,000 characters the tables drop their links and show
16 hex digits of each hash, and past that again they are cut short with a pointer to
`ports/sources.idx`, which is complete.

| Flag | Does |
|---|---|
| `--dry-run` | List what would be uploaded: the target volume, the predicted asset name (resolved against the index and the rest of the plan, not against GitHub), the part count and the size. Makes no network call and needs no token, takes every indexed file as present, and takes the index's count of each volume for its assets, so a spill is predicted as it would happen. With `--plan`, `--rehome` or `--retire`, lists what that mode would do |
| `--check` | Presence only: list what is missing from the archive, as `not in ports/sources.idx` or `indexed in <tag> as <asset> but absent`; exit 1 if anything is |
| `--history` | Add every LFS object under `ports/core` that any ref's history names (`git lfs ls-files --all`), to seed the archive with what old commits' recipes point at. Each is labelled `<port>/<file>` from its old path and goes to its port's shelf today, or, when no port of that name exists today, to the highest volume as `attic--<file>`. Objects the local LFS store and the cache do not hold are counted on one line and skipped without failing the run. Without git-lfs no object can be labelled, so none is uploaded, and the count says to install it |
| `--plan` | Give every shelf `ports/shelves` lists without a volume one, and rewrite those lines; see below. No network, no token |
| `--describe` | Rewrite the title and notes of every release the index names, uploading nothing. Needs the token |
| `--rehome` | Move archived files into the volume and name of the shelf that owns them now, then delete old copies; see below |
| `--retire` | Delete every emptied `src-<shelf>` release of the older layout, and its tag; see below |
| `--orphans` | List index lines no current recipe names; see below |
| `--freeze <tag>` | Write `build/freeze/sources-<tag>.sha256` for the tag's recipes, found one shelf down or directly under `ports/core`, require every hash in it to be archived, and attach it as `sources.sha256` to release `<tag>` on `$KDOS_REPO`, creating a draft release when there is none. With `--dry-run`, only the list is written. See [Cutting a release](developing.md#cutting-a-release) |
| `--release <tag>` | Build and attach the system release of `<tag>`. See [Cutting a release](developing.md#cutting-a-release) |

Exit status is 0 when everything wanted is archived, 1 when something is missing or an upload
failed, and 2 when every name for a file is held by other bytes, or an asset's digest disagrees
with its index line and could not be repaired. `--freeze` also exits 2 when the release already
carries a different `sources.sha256`; replace that asset by hand, and only while the release is
still a draft.

**Volumes and shelves.** The second field of every `ports/shelves` line is the shelf's volume. A
new shelf is written with `-` in that field, or with none, and `ports/publish --plan` (`make
publish-plan`) gives it one, with no network: it counts the files each shelf owns (every recipe
hash whose first port is on it, a file in parts counted once per part), takes the shelves without
a volume in C order, and adds each to the highest volume while that volume's count stays within
`KDOS_VOLUME_FILL` (650), opening the next volume, numbered past any the index names, when it
would not. A shelf larger than the fill gets a volume of its own. A shelf that already has a volume
keeps it, so planning never moves a file. `--plan --dry-run` prints each volume's count, shelves
and title and writes nothing. Preflight fails a shelf whose volume is not a positive integer.

**Moving files between volumes.** Moving a port to another shelf leaves its files where they were
archived, and `make fetch` still finds them through the index. `ports/publish --rehome [port…]`
tidies that. A file is at home when it sits in the volume of a shelf `S` whose port names it, under
a name starting `S--`, so a hash two shelves name is never moved back and forth. A file in a
`src-<shelf>` release of the older layout, one release per shelf, or under a name no current shelf
of its ports gives it, is placed as a new file is, spilling when its volume is full; one no current
recipe names goes from such a release to the highest volume as `attic--<file>`, and one already in
a volume stays where it is. A file that spilled moves home only when its own volume has room.
Each move uploads the file (from the local copy, or downloaded from the archive and verified when
there is none) and rewrites its index line, so a run that stops keeps every move it made and is
resumed by running it again. Then, in a second phase, an asset in a `sources-*` or `src-*` release
is deleted only when its digest is an indexed file's (or one of its parts'), the working index
**and** the pushed one (`@{upstream}:ports/sources.idx`, as this clone last fetched or pushed it)
both place that file somewhere else, and GitHub lists that other place with the right digest. So
the first run after a move uploads and rewrites the index; commit and push it, and a second run
deletes the old copies. Without an upstream branch the second phase deletes nothing. An asset whose
digest no index line names is reported as a `stray` and never deleted.

**Retiring the older layout.** `ports/publish --retire` deletes every `src-<shelf>` release that
holds no asset, then its tag, which deleting a release leaves behind, and any `src-*` tag whose
release is already gone. A `src-*` release that still holds any asset is refused and listed with
its count, and the run exits 1; `--rehome`, before and after the push, is what empties it. It never
touches a `sources-<N>` volume or a `v*` release. `--retire --dry-run` reads the archive with the
token and deletes nothing. A clone that fetched the old tags keeps them until they are deleted
locally (`git tag -l 'src-*' | xargs -r git tag -d`), and a `git push --tags` from it would create
them again on GitHub.

**Orphans.** `ports/publish --orphans` prints, with no network, every index line no current
recipe names, as `<sha256> <tag> <asset> <port>/<file>`, and marks a line `protected` when a
recipe at any `v*` tag names its hash or a `build/freeze/*.sha256` lists it, then counts both.
`--orphans --prune=yes-delete` deletes the assets of every unprotected orphan, all its parts
included, each checked against its index hash first, then removes the lines and rewrites the
notes; any other `--prune` value, and `--prune` without `--orphans`, is refused. A checkout older
than the pruning that still names a pruned file goes upstream for it.

The archive is otherwise append-only. Beyond `--rehome` and `--prune=yes-delete`, and `--retire`
deleting emptied releases, `ports/publish` deletes an asset only when it is an upload GitHub never completed (a `starter`), or when it holds
other bytes at a name the index gives this hash, or was just uploaded by this run and GitHub
reports other bytes: that asset is corrupt, so it is deleted and uploaded once more, and a second
disagreement fails the file. An asset of other bytes at any other name is someone else's, and the
next name is tried instead. GitHub may report a completed asset's digest as null; such an asset is
asked for again and, if it still has none, downloaded and hashed. An unknown digest is never a
reason to delete.

The token is read from `$KDOS_SOURCES_TOKEN`, or from `~/.config/kdos/sources-token`, which is
refused when its group or others can read it. It needs write access to the contents of
`kunaldawn/kdos`. It reaches curl only through a mode-600 header file, never an argument, so no
process list shows it, and it is removed from the environment before any child starts.

GitHub throttles content creation separately from its hourly quota, so at least
`KDOS_PUBLISH_DELAY` seconds pass between creating, changing and deleting calls: eight by default,
which holds a long run under 75 a minute and 480 an hour. A 403 or 429 waits for `retry-after` or
`x-ratelimit-reset`, or 60 seconds for a bare 429 or a 403 naming a secondary rate limit, and
retries. An upload that meets a 5xx or a dropped connection is retried twice; the partial
`starter` asset it may leave is deleted and replaced.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_TOKEN` | `~/.config/kdos/sources-token` | The upload token |
| `KDOS_PUBLISH_DELAY` | `8` | Seconds between creating, changing and deleting calls |
| `KDOS_SOURCES_INDEX` | `ports/sources.idx` | The index read and written |
| `KDOS_RELEASE_CAP` | `1000` | Assets per archive release, parts counted; a file whose volume would pass it spills into another volume. GitHub's limit is 1000 |
| `KDOS_VOLUME_FILL` | `650` | The planned assets per volume that `--plan` packs shelves to |
| `KDOS_PART_SIZE` | `1992294400` (1900 MiB) | A file larger than this is uploaded in parts of this size |
| `KDOS_LFS_STORE` | `<git dir>/lfs/objects` | The LFS store read for bytes and by `--history` |
| `KDOS_REPO` | `kunaldawn/kdos` | The repository whose release `--freeze` and `--release` attach to |
| `KDOS_GITHUB_API`, `KDOS_GITHUB_UPLOADS` | `https://api.github.com`, `https://uploads.github.com` | The two endpoints, replaced to run against a local stand-in |

`KDOS_SOURCES_REPO` and `KDOS_SOURCES_BASE` name the archive as they do for `ports/fetch`; an empty
`KDOS_SOURCES_BASE`, or one naming a local directory, stops every mode but `--plan` and a plain
`--orphans` at once, since there is nothing to publish to.

### The pre-push hook

The pre-push hook stops you pushing a tree whose ports layout is broken, or a recipe whose source
is not in the archive yet. It is `script/hooks/pre-push`, and it is off until you enable it in your
clone:

```sh
git config core.hooksPath script/hooks
```

That setting replaces `.git/hooks` entirely, git-lfs's own hooks included. Preflight reminds you
when the hook is not enabled.

The layout check runs first and needs no network. For each pushed ref's commit that the remotes do
not already hold, it reads the commit's tree, not the working copy, and requires:

- every `kpkgbuild` under `ports/core` at exactly `ports/core/<shelf>/<name>/kpkgbuild`;
- every `<shelf>` listed in that commit's `ports/shelves`;
- every bare port name held by one port across `ports/core` and the `src/<area>/` directories;
- no listed shelf named `libs` or `core`, named after a port, or with an id outside lowercase
  letters, digits and `-`.

A breach refuses the push with `pre-push: the ports layout is broken at <sha>:` and one line per
problem, in the words preflight uses for the same fault. `KDOS_SKIP_LAYOUT_CHECK=1 git push …`
bypasses it.

The archive check follows. For every ref pushed, the hook takes the hashes the pushed commit's
recipes name that the remote side's recipes do not (the remote's current commit for that ref, or
every `refs/remotes/*` tip for a new branch) and drops any whose file git carries at that commit.
It reads `ports/sources.idx` as it stands in the pushed commit: a hash that index does not name is
missing without asking, and the rest are `HEAD`-checked against the archive. Any missing hash
refuses the push, listing each as `port/file (hash)` and naming the command to run,
`ports/publish <ports> && git commit ports/sources.idx`. Deleting a ref passes.

Only a 404 counts as missing. The hook also refuses the push when it cannot reach the archive
(`cannot reach the source archive at <base> (curl exit N)`), when the archive answers any status but
200 or 404 (`source archive unreachable at <base> (HTTP N)`), and when `KDOS_SOURCES_BASE` is empty.
Offline or during an outage, absence cannot be ruled out, and passing silently would let through
exactly the push the hook exists to stop. `KDOS_SKIP_PUBLISH_CHECK=1 git push …` bypasses the
archive check and not the layout check.

## See also

- [How KDOS is built](how-kdos-is-built.md): where ports, shelves and phases sit in the whole build
- [Packaging](../03-architecture/packaging.md): what a package is and how it is verified
- [Build troubleshooting](build-troubleshooting.md): the recurring failures, by symptom
- [The build system](build-system.md): how phases install what you wrote
- [Developing](developing.md): the narrow rebuild loops and where sources come from
- [Testing](testing.md): `preflight.sh` and what it checks about recipes
- [The ports catalogue](../06-reference/ports-catalogue.md): every port, by shelf
- [Decisions](../01-philosophy/decisions.md): why a recipe is two files
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#packages-and-recipes): this recipe format
  beside Debian's, Arch's, Gentoo's and Nix's
- [Why KDOS](../01-philosophy/why-kdos.md#what-is-not-built-from-source): every payload not built
  from source, port by port

<!-- book-nav -->
---

*Part V — Building and developing, chapter 33.* Previous: [32. The build system](build-system.md) · [Contents](../README.md) · Next: [34. Build troubleshooting](build-troubleshooting.md)
