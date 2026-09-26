# Writing ports

This page is for anyone adding a piece of software to KDOS or updating one that is already here. A
[port](../06-reference/glossary.md) is the recipe for one host package: where its source comes
from, what it depends on, and how to build it. There are 1,014 of them under `ports/core`, plus 24
of KDOS's own programs under `src/packages` and `src/desktop` written in the same format.

By the end of this page you will be able to write a recipe from scratch, pick the right build shape
for its build system, vendor the dependencies of a Rust, Go, Python or Haskell program so it builds
offline, check the recipe before a long build, keep it current with upstream, and get its source
into the shared archive so every other checkout can build it too (uploading is a maintainer's step;
a contributor checks what is missing and says so in the pull request).

Read these first if you have not:

- [Developing](developing.md): setting up a checkout, `make fetch`, and the fast rebuild loops.
- [Packaging](../03-architecture/packaging.md): what a package *is* and how one is verified.

If you only want the procedure, start at [Adding a port, end to end](#adding-a-port-end-to-end)
and come back to the reference sections when a step sends you here. When a build fails, go to
[Build troubleshooting](build-troubleshooting.md), which is indexed by the message you see.

## Anatomy of a port

A port is a directory. Most hold two files you write and one file `make fetch` downloads:

```
ports/core/frotz/
├── kpkgbuild                  declarative metadata: parsed, never run
├── build.sh                   the build: ordinary bash
├── postinstall.sh             optional install-time hook
├── *.patch                    optional patches, applied by build.sh
└── frotz-2.55.tar.gz          the source: fetched, gitignored, not in git
```

`kpkgbuild` has no interpreter line and is never executed. A tool that reads a recipe therefore
starts no shell and cannot run anything by accident.

`build.sh` is a real script, so syntax checking, linting, highlighting and diffing all work on it.
`testing/preflight.sh` runs `bash -n` on every one, which would be impossible if the build lived
inside a configuration format.

The upstream source is not in git. Its `sha256 =` line in the recipe is its identity, and
`make fetch` puts the file beside the recipe from a local cache, the KDOS source archive, or
upstream (see [Where sources come from](developing.md#where-sources-come-from)). After you add or
change a source, [publish it](#publishing-sources) so other checkouts can fetch it.

The reasoning behind the two-file split is in [Decisions](../01-philosophy/decisions.md).

## kpkgbuild

Every recipe opens with the KDOS banner header, copied verbatim from any other recipe
(`testing/preflight.sh` checks that it is there), and then a block of `key = value` lines:

```
name        = frotz
version     = 2.55
release     = 1
source      = $name-$version.tar.gz::https://gitlab.com/DavidGriffith/frotz/-/archive/$version/frotz-$version.tar.gz
sha256      = a8c4c4d7…  frotz-2.55.tar.gz
description = Z-machine interpreter — Infocom-era interactive fiction in a terminal
homepage    = https://661.org/proj/if/frotz/
depends     = ncurses pkgconf
```

### Keys the package manager reads

`kpkg`'s parser recognises thirteen names. Everything else on a `key = value` line becomes a
recipe helper (see [Recipe helpers](#recipe-helpers)).

| Key | Required | Repeats | Means |
|---|---|---|---|
| `name` | yes | | The package name |
| `version` | yes | | Upstream's version. No hyphen: a package file is `<name>-<version>-<release>.tar.xz`, so a hyphenated version makes that name ambiguous, and preflight refuses it. Spell a tag like `1.9.0-Jumbo-1` as `1.9.0.jumbo1` and carry upstream's own form in a helper the `source` line reads |
| `release` | yes | | Bump to force a rebuild for a reason the recipe hash cannot see |
| `source` | | yes | An upstream URL, or `filename::url` |
| `sha256` | | yes | `<64 hex>  <filename>`, one per declared file: every `source`, plus a vendor bundle or anything else beside the recipe the build opens |
| `description` | | | One line. It is read and printed by `kpkg info`; it is not a comment |
| `homepage` | | | Upstream's home page. The version checker reads it |
| `depends` | | | One line of space-separated port names. The solver reads the first `depends` line and ignores any other |
| `vendoring` | | | `rust`, `go`, `python` or `haskell`, and `node`, which `ports/fetch` supports and no port uses. See [Vendoring](#vendoring) |
| `pypackages` | | yes | An explicit Python dependency closure to vendor |
| `secdb` | | | The name the security database uses, when it differs from ours. `kdos cve` reads it |
| `bench` | | | A command `kdos march` times |
| `bench_setup` | | | A command that runs once before `bench` and is not timed |

`description`, `homepage` and `depends` are keys, not comments. The parser skips a `#` line
entirely, so a fact written as a comment reaches nothing.

`source`, `sha256` and `pypackages` accumulate across lines. `depends` does not: the solver, which
decides build order, and `kpkg info` both take the first `depends` line and stop, so a second line
is a dependency the build order does not know about. Keep every dependency on one line.

`build.sh` does not receive `description`, `homepage` or `depends` as variables. `kpkg info`, the
version checker, `kdos cve` and `kdos march` read `description`, `homepage`, `secdb`, `bench` and
`bench_setup` literally from their first line, without expanding helpers, so write those values
without `$` references.

### Keys other tools read

These are ordinary helpers as far as `kpkg` is concerned — it stores them as variables and passes
them to `build.sh` — but another tool gives each a defined meaning:

| Key | Read by | Means |
|---|---|---|
| `vendordir` | `ports/fetch` | Where the vendoring tool must run, when that is not the top of the tree |
| `hsplan` | `ports/fetch` | A bootstrap plan, relative to `vendordir`, that is the Haskell dependency set — see [The Haskell bundle](#the-haskell-bundle) |
| `vendorsync` | `ports/fetch` | Further Cargo manifests, relative to `vendordir`, whose crates go into the same Rust bundle — see [Where the bundle goes](#where-the-bundle-goes) |
| `pyrequirements` | `ports/fetch` | `no` — a requirements file is *not* the dependency set here |
| `pyruntime` | `ports/fetch` | `no` — do not vendor a runtime environment |
| `group` | `ports/update` | Override the derived version-check grouping |
| `devseries` | `ports/update` | Upstream's development-series convention: `odd-minor`, `preview-minor`, or both — see [Filtering](#filtering) |
| `series` | `ports/update` | The line the port stays on, as a version prefix matched on whole components: `21` for `llvm21`, `5.4` for `lua54` — see [Filtering](#filtering) |
| `watch` | `ports/update` | Where upstream lists its releases when nothing reached from the source URL does: a forge repository, whose tags are read, or a page, listing, JSON index or manifest that names the release files — see [Discovery](#discovery) |

### Recipe helpers

Any key that is not one of `kpkg`'s own is a helper: `_tag`, `_commit`, `_date`, `debrev`, `vrsn`
and the rest. Helpers reach `build.sh` as shell variables.

Declare them between `release` and `source`. That is the only order that works, because a helper
may read `$version` and `source` may read the helper.

Values expand:

```
$var   ${var}   ${var#p}   ${var##p}   ${var%p}   ${var%%p}
${var/a/b}      ${var//a/b}            ${var:off:len}
```

Command substitution is not available. Where upstream's convention needs a transformation, use the
pattern operators: a version with its dots removed is `${version//./}` rather than a pipeline.

### Sources, and what each one becomes

`source` repeats to add more, and the archive format is detected from the name. What happens to
each depends on its position and its extension. First, the name it is saved under:

| Source | Saved as |
|---|---|
| The first URL, with a recognised archive extension | `<name>-<version>.<ext>`, whatever the URL's basename is |
| A later URL | The URL's basename |
| A bare filename, not a URL | That filename, looked for beside the recipe |
| Anything written `filename::url` | `filename`, first or not |

Then, how it is unpacked before `build.sh` runs:

| Source | Unpacked |
|---|---|
| A tarball, first | Into `$SRC`, with `--strip-components=1` |
| A tarball, later | Into `$SRC_ROOT`, unstripped, beside `$SRC` |
| Anything else: a data file, a `.zip`, a `.tar.zst` | Copied into `$SRC` as-is |

The first source is renamed on purpose. A forge that generates an archive named after a tag would
otherwise leave every port holding a file called `2.55.tar.gz`, and the standardised name is what
the port directory, the checksum line and the source archive all agree on. Use `filename::url` when
the saved name needs to be something else, for example when a second port builds the same tarball
(see [A second build of the same source](#a-second-build-of-the-same-source)).

Checksums are matched to sources by **basename**, not by position, so reordering the `source` lines
cannot pair a hash with the wrong file. A source with no hash in the recipe is a refusal, not a
pass: `kpkg` prints `No sha256 for <file> in the recipe` and stops before extracting anything.
While you bring up a new port and do not know the hash yet, `KDOS_ALLOW_UNVERIFIED=1` lets that one
build through with a warning. Preflight fails any recipe that mentions the variable, so it cannot
be committed as a permanent answer.

A source is looked for in the port directory first and then in `kpkg`'s source directory,
`/var/cache/kpkg/sources` (`SOURCE_DIR` in `/etc/kpkg.conf`). That directory is not the source
cache, `ports/.srccache`, which `make fetch` fills on the build host. If it is in neither, the build stops
with `Source not found: <file> (checked <port dir> and <source dir>)`.

Two extension sets, and they are not the same. The standardised name is derived from `.tar.gz`,
`.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst` and `.zip`; what is actually unpacked is
`.tar.gz`, `.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz` and `.tar`. So a first source that is a
`.zip` or a `.tar.zst` is saved as `<name>-<version>.zip` or `.tar.zst` and copied into `$SRC`
whole; a recipe that wants one unpacks it in `build.sh`.

A release archive carries a submodule's directory empty, because the archive is generated from the
repository without recursing. If upstream's build expects a submodule, add it as a second `source`
extracted where the build looks, or make it a port.

## build.sh

`kpkg` runs `build.sh` by sourcing it inside `( set -e )`, with the working directory set to the
unpacked source. There is no `pipefail` and no `set -u`: a failing command stops the build, but a
failure in the middle of a pipeline does not, and an unset variable expands to nothing. Everything
the script prints goes to the port's build log under `build/logs/<phase>/`.

The recipe's keys and helpers arrive as shell variables, not exports, so a child process such as
`make` does not see them unless you export them yourself. Alongside them come five paths and names,
and the settings of `/etc/kpkg.conf`, which `kpkg` sources into the same shell first:

| Variable | Is |
|---|---|
| `$name`, `$version`, `$release` | From the recipe |
| `$source`, `$sha256`, `$vendoring`, `$pypackages`, `$secdb`, `$bench`, `$bench_setup` | From the recipe, when set |
| Every helper | From the recipe |
| `$PKGNAME` | `<name>-<version>-<release>.tar.xz`, the package file being made |
| `$PORT_SRC` | The port's own directory, where patches and vendor bundles are |
| `$SRC` | The unpacked source, and the working directory |
| `$SRC_ROOT` | The parent of `$SRC`, where a second tarball lands |
| `$PKG` | The staging tree: install here, never into `/` |
| `$SOURCE_DIR`, `$WORK_DIR`, `$PACKAGE_DIR`, `$PORT_REPO`, `$PKGDB_DIR` | From `/etc/kpkg.conf`, sourced before `build.sh`: kpkg's source directory, work area, package output, ports tree and package database |

The compiler flags (`CFLAGS`, `CXXFLAGS`, `LDFLAGS`), `MAKEFLAGS=-j12`, `CC=gcc` from phase 3 on, and the
reproducibility settings (`SOURCE_DATE_EPOCH`, `TZ=UTC`, `LC_ALL=C`) come from the phase's
environment file, `script/phaseN.env.sh` (`script/phase4.env.sh` for `04_phase4`,
`script/desktop.env.sh` for `05_desktop`), and are exported. Extend them rather than replace them:
`export CFLAGS="$CFLAGS -Wno-error"`.

The minimal autotools recipe is three lines:

```bash
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

## Canonical build shapes

Each build system has one shape that works on this tree. Start from it and change only what the
project needs; most of the failures in [Build troubleshooting](build-troubleshooting.md) come from
leaving one of these flags out.

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

Check option names against the tarball's own `meson_options.txt` or `meson.options`. meson fails at
setup on an unknown option, before a line is compiled, and there is no universal spelling: one
project's disable flag is fatal in the next. meson's built-in options are always valid.
`testing/preflight.sh` checks every `-D` against the option file, but only for a port whose tarball
has been fetched, so run `make fetch` (or `ports/fetch <port>`) before trusting that check.

`-Ddocs=disabled` turns off the HTML and API references. Where a project puts its manual pages
behind their own option (`-Dman=true`, `-Dman-pages=enabled`), that option stays on — see
[Manual pages](#manual-pages).

### cmake

```bash
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
```

A misspelt CMake option is a warning, not an error — the opposite of meson. Read the warning about
unused variables in the log rather than trusting the exit status, and take option names from the
project's own `option()` declarations.

### autotools

```bash
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

### rust

```bash
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
```

The `RUSTFLAGS` line is required. The musl target links statically unless told otherwise, and a
crate that binds a system library then fails to link, or carries a private copy of the library that
no update to its port reaches. Preflight fails any recipe that runs `cargo build`, `install`,
`cbuild` or `cinstall` without `-crt-static`. `LIBCLANG_PATH` matters when a crate runs bindgen.

### go

```bash
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CGO_ENABLED=1
make PREFIX=/usr GOFLAGS="-mod=vendor"
make PREFIX=/usr DESTDIR=$PKG install
```

### make-only

```bash
export CFLAGS="$CFLAGS -Wno-error"
make PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr install
```

Export the compiler flags rather than passing them as a make argument. A variable on the make command line
beats both the environment and the makefile's own assignment, which is the wrong end of that
precedence for flags: a makefile's own definitions are its *configuration* — architecture width,
installation paths, feature constants. Passing flags as arguments discards those, and the build
then fails somewhere else entirely, on an undeclared constant that reads like a missing header.

## Desktop entries belong to the port

A program that draws in a terminal is an application on this desktop, and almost none of them ship
a `.desktop` file — upstream writes one for a GUI or writes none at all. Write it in `build.sh`,
into `$PKG/usr/share/applications/`. A package owns its entry, so installing the port adds the menu
row and removing the port takes it away; the same file under `fs/` is owned by nothing and outlives
the program it names.

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
among the rest. The size is in cells, and anything smaller than 4x2 is refused. `kdos app tui`
writes both keys for entries it makes, and a recipe writes them for the same reason (see
[`kdos app tui`](../04-programs/kdos-command.md#kdos-app-tui)).

**Never `X-KDOS-TUI=true`.** That key is `kdos app tui`'s own marker. It means "this command wrote
this file", which is what makes `kdos app tui rm` safe to run. A shipped entry carrying it would be
an application that command could delete.

**`X-KDOS-Term=kdos-term` only where the program draws pictures.** The key names the emulator the
entry needs rather than the one the session runs, and the launcher honours it: `kdos-term` links the
image decoders and speaks sixel and the kitty graphics protocol, so `yazi`'s previews are pictures
rather than a filename. Without the key the session's own terminal is used, which is lighter. The
value is a name, never a program: only an emulator this image ships is accepted, and an unknown
value falls back to the session's own. An entry is a file anything can write, and a key that named
a program would be a second `Exec` line with none of its rules.

**An `Icon=` the image can draw.** The shipped atlas is narrower than the artwork.
`src/packages/kdos-icons/genatlas.py` takes six contexts (`places`, `devices`, `status`,
`mimetypes`, `actions`, `emblems`) at four sizes (24, 32, 48 and 64). There is no `apps` context
and no `panel` one: `panel/` holds about 2,400 third-party tray marks at each of 16, 22 and 24 px, and an
application's own icon comes from hicolor at run time instead. So a name can be present in
`src/packages/kdos-icons/art` and still be undrawable: `file-manager` is only in `panel/`, and
`utilities-terminal` is not there at all, though both are what the freedesktop naming
specification suggests. After the atlas, `libkicon` falls back to hicolor's `apps/` PNGs and to
`pixmaps/`. An SVG in either place is never read, because nothing in the session rasterises one.
When a build tree exists, `testing/preflight.sh` resolves every shipped entry's `Icon=` and `Exec=`
by exactly these rules against `build/fs` and names the ones that miss.

**A `Name=` no other visible entry uses.** The Start menu, the launcher and the search all list
entries by name, so two rows both reading `Calendar` are two rows nobody can choose between. The
program filling a role (the file manager, the agenda) keeps the plain name, and every alternative
is qualified: `Files (lf)`, `Files (yazi)`, `Calendar (calcurse)`. An entry with `NoDisplay=true`
is not visible and does not count. When a build tree exists, `testing/preflight.sh` refuses a
collision.

**`MimeType=` only where nothing else claims the type.** When two entries claim one type, the
machine opens those files in whichever entry sorted first, which is not a decision anybody made.
`mimeapps.list` is where a default is chosen.

**`MimeType=` only for a type that exists.** The field names a type in the shared MIME database,
and a name with nothing behind it resolves to nothing and reports that nowhere. The opener chain
reads `/usr/share/mime/globs`, which is generated from `/usr/share/mime/packages`. A port that
introduces a type installs its own XML there, and `kpkg` regenerates the database (see
[Shared indexes](#shared-indexes)). `frotz` is the [worked example](#worked-example-frotz):
`shared-mime-info` 2.5.1 defines no Z-machine type, so the port defines it.

**`Keywords=` with the words people will search for.** The menu searches this field. A row nobody
can find by the word they know the program by might as well not be there.

For the game ports (`nethack`, `frotz`, `bsd-games` and `moon-buggy`), `testing/selftest.sh` reads
the entries out of the `build.sh` heredocs and fails a missing `Terminal=true`, a `Categories=`
with no `Game` token, or an `Exec=` naming a path rather than a command, at the recipe rather than
after a packaging run.

## Manual pages

A port installs every manual page upstream ships or can generate, into
`$PKG/usr/share/man/man<section>/`. `man` reads them in place, and the packaging step indexes them
for `apropos` and `whatis`. A page missing from a package is missing from the machine: nothing
else supplies it.

| Upstream | The recipe |
|---|---|
| Installs its pages from `make install` or `meson install` | Nothing, unless a flag turned them off |
| Puts them behind an option | Turns on the manual-page option only (`--enable-man`, `-Dman=true`, `-DENABLE_MAN=ON`), not a full-documentation one that pulls in an HTML toolchain |
| Ships finished pages that its installer skips — most Rust, Go, Zig and Python projects | `install -Dm644 doc/foo.1 -t "$PKG/usr/share/man/man1"` |
| Has the program write its own page (`foo --generate man`, a `man` subcommand) | Runs the freshly built binary into `$PKG/usr/share/man/man1` |
| Generates them with a tool | Adds the tool to `depends` when it is a port |

The generators that are ports: `scdoc`, `help2man`, `asciidoc` (`a2x`), `asciidoctor`, `xmlto`,
`libxslt` (`xsltproc`) with `docbook-xsl`, `python3-docutils` (`rst2man`), `perl` (`pod2man`),
`texinfo`, `go-md2man`, `lowdown`, `xmltoman` and `python3-sphinx` (`sphinx-build -b man`). A page that needs
a generator that is not a port — `ronn` — is not generated.

A build that looks for `asciidoctor` on `$PATH` uses it whenever it is there, so a recipe that
does not name it ships a different package once any other port pulls it into the chroot. Name it
in `depends` and set the documentation switches explicitly. Where upstream has no switch for the
manual pages alone, build the pages' own targets (`newsboat`, `git-lfs`), run upstream's page
script (`ccache`), or remove the HTML the install adds (`wireshark`): the package carries the pages
and no HTML manual.

Markdown pages come in two dialects, and each has its converter:

| Source | Converter | The recipe |
|---|---|---|
| A `*.1.md` that upstream's docs `Makefile` feeds to `$(GOMD2MAN)` — the containers stack | `go-md2man` | Runs upstream's own docs and install targets |
| A page that starts with a pandoc `%` title block and that upstream renders with `pandoc -s -t man` | `lowdown` | `lowdown -s -Tman -o <page> <page>.md`, then installs it |

`lowdown` stands in for `pandoc`. `pandoc` is a port, but a GHC build whose freeze pins 259 Hackage
packages, too much to put under another port just for its manual page. It reads the same `%` title block into `.TH`, and `-M key=value`
supplies what a pandoc invocation passes as `--variable` — `-M title=YQ -M section=1` for a page
with no title block, `-M source=v$version` where the title block carries an unexpanded
`$version` or a version older than the release. Pass `--out-no-smarty` when the page writes long
options outside code spans, or each leading `--` renders as an en dash. A pandoc-only escape, `\ ` for a
non-breaking space, reaches the page as a literal backslash; `shellcheck` ships a patch that
replaces it with a space in its option headings. Inside a code span the backslash is literal in both
renderers, so leave those alone. Render a new page once and read it with `mandoc -Tlint` before shipping it; a page
that only draws style warnings is fine. `lowdown` is built with `bmake`, because its makefile is
BSD make and GNU make stops at the first `.if`. `bmake` depends on `tzdata` because its install
runs its unit tests, and two of them convert a time in a named zone: without the zoneinfo database
they print UTC and the install fails.

`python3-sphinx` installs Sphinx, and the part of its closure that is not a port, under
`/usr/lib/python3-sphinx` rather than in `site-packages`: `requests` and `urllib3` are ports in
`site-packages` already, and two packages owning one path is a conflict. The closure carries
the default theme, `myst-parser` for Markdown sources and `sphinx-argparse`. The commands in
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
conflict: `openssl3`, `lua54`, the `llvm21` slot and the cross toolchains ship none of the
native port's pages. `man-pages` supplies the kernel and C library sections (2, 3, 4, 5, 7) and
leaves out every page another port installs.

## Shipping a script the port carries

Some ports install a small script of KDOS's own beside the upstream program, such as a filter for
`aerc`. Where you keep that script matters.

The [recipe hash](../06-reference/glossary.md), which decides whether a port needs rebuilding,
covers `kpkgbuild`, `build.sh`, `postinstall.sh` and every file ending in `.patch`, and nothing else
in the directory of a port that names a `source =`. A helper script kept in its own file beside the
recipe is therefore invisible to the hash: after you edit it, the port still reports itself
current, and the image keeps the copy it already had. Nothing fails; the machine runs the old
script. The same applies to a patch named `*.diff`, so name patches `*.patch`.

A port with no `source =` is different: its whole directory is hashed, because its own files *are*
its recipe (see [Packaging](../03-architecture/packaging.md)).

Write such a script into `build.sh` instead, in a quoted heredoc whose delimiter is `KDOS_SH`:

```bash
install -d "$PKG/usr/libexec/aerc/filters"
cat > "$PKG/usr/libexec/aerc/filters/kdos-part" <<'KDOS_SH'
#!/bin/sh
...
KDOS_SH
chmod 755 "$PKG/usr/libexec/aerc/filters/kdos-part"
```

The delimiter is what `testing/preflight.sh` looks for. It extracts each `KDOS_SH` body into a file
of its own and parses it with `bash -n` — `bash -n` on the recipe reads a heredoc as one word and
sees nothing — and, when a build tree is present, checks that every program the script names as the
first word of a line, after `if `, or after `set -- ` is on the image. A name inside a command
substitution or a `trap` string is not seen. Quote the delimiter, or `$1` and `$PATH` expand while
the recipe runs rather than while the script does.

## postinstall.sh

`postinstall.sh` is an optional hook that runs on the target machine each time the package is
installed. It travels inside the package. Thirteen ports have one:

- `avahi`, `geoclue`, `mosquitto`, `networkmanager-openvpn`, `pcsc-lite`, `polkit`, `postgresql`,
  `prosody` and `tcpdump` create their system accounts. `avahi` makes two, `avahi` and `avahi-autoipd`.
  `prosody` and `postgresql` also give their data directories to their accounts, and
  `networkmanager-openvpn` gives its chroot to its account.
- `linux` removes the module trees of other kernels. It keeps the running kernel's and those of
  every kernel on the ESP, runs `depmod`, and builds the new kernel's initramfs into the root as
  `/boot/initramfs-kdos.cpio.gz`. Installing into the running system, it then puts both into that
  slot's ESP directory with `kdos-bootctl deploy /`; `kdos update` deploys the inactive slot itself
  ([a new kernel](../03-architecture/boot-and-init.md#a-new-kernel)).
- `dbus` gives `dbus-daemon-launch-helper` back its group, `messagebus`, and its mode `4110`: the
  package is rolled `root:root`, and the bus can run the helper only through that group, so without
  the hook no `User=root` service is ever activated.
- `ca-certificates` writes `/etc/ssl/cert.pem` with its own `update-ca-certificates`: the Mozilla
  bundle it ships plus the administrator's local roots, which a package-owned file would lose on
  every upgrade.
- `brltty` creates the `brlapi` group its polkit rule admits to BrlAPI, and puts the desktop account
  in it when it first creates it.

Every hook works on `PKG_ROOT`, the root kpkgadd is installing into, never on `/`. `kpkg install
--root` and an A/B update both install into a tree that is not the running system, so a hook that
wrote `/etc/passwd` or ran `depmod` on `/` would change the wrong machine and leave the new root
without what it needs. Take the root as `"${PKG_ROOT:-/}"`, prefix it on every path, and hand it to
the tool: `groupadd -R`, `useradd -R`, `depmod -b`. `chown` resolves a name against the running
root's `/etc/passwd`, so read the ids out of `$PKG_ROOT/etc/passwd` and pass them as numbers. A hook
runs on every install and reinstall, so each step checks before it acts.

The one exception is `linux`'s write to the ESP. The ESP is not part of any root: each slot's kernel
lives in its own `EFI/kdos/<slot>/` there. The hook runs `kdos-bootctl deploy /` only when `PKG_ROOT`
is the running system, whether or not the machine has slots. For any other root it only builds
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
[Packaging](../03-architecture/packaging.md)), read the manifest of the package they installed or removed and rebuild each
index whose directory it touched, once, from what is then on disk:

| A file under | Rebuilds |
|---|---|
| `/usr/share/glib-2.0/schemas/` | `glib-compile-schemas` |
| `/usr/lib/gio/modules/` | `gio-querymodules` |
| `/usr/lib/gdk-pixbuf-2.0/` | `gdk-pixbuf-query-loaders --update-cache` |
| `/usr/share/mime/packages/` | `update-mime-database` |
| `/usr/share/fonts/`, `/etc/fonts/` | `fc-cache -s`, into `/usr/lib/fontconfig/cache` — not `/var/cache`, which the image and every pack exclude |
| `/usr/share/info/` | the info `dir`, regenerated with `install-info` over every page |
| `/etc/udev/hwdb.d/`, `/usr/lib/udev/hwdb.d/`, `/lib/udev/hwdb.d/` | `udevadm hwdb --update`, into `/etc/udev/hwdb.bin` |
| `/usr/share/man/` | `makewhatis`, the `mandoc.db` that `apropos` and `whatis` search |
| `/usr/share/fonts/` | `mkfontdir`, the `fonts.dir` of every subdirectory holding PCF or BDF faces, which Xwayland's core font path reads |

A port therefore installs its schema, loader, MIME XML, font, info page, hwdb file or manual page
and does nothing else. A per-port hook would rebuild the index only when that port is installed,
not when the next one adds to it or the last one leaves.

A missing tool is skipped: the index is written when the package carrying the tool arrives,
because that package's own files touch a watched directory. fontconfig installs no font, so the
font cache also watches `/etc/fonts/`; without it, a system whose fonts all came before fontconfig
would have no cache. A failing tool is a warning, not a failed install. `kpkg`'s build step (`kpkgbuild`, the program,
not the recipe file) drops
`usr/share/info/dir` and every `usr/share/fonts/*/fonts.dir` from every package — each is the
index, and two packages each shipping one conflict: `font-misc-misc` and `font-cursor-misc` both
install into `misc/`. A font directory left with no bitmap face loses its `fonts.dir`, and with it
the directory.

The manual index is the one that is merged rather than rebuilt, and only when nothing was removed.
Two packages in three carry manual pages, and reading every page on the system again for each one
would add seconds to every install. The pages an install places go to `makewhatis -d`. A removal,
an upgrade that orphans a page, or a missing `mandoc.db` rebuilds the whole tree, because a merge
cannot drop an entry for a file that is already gone.

Under `--root`, each tool is handed the root-prefixed directory, and `udevadm` and `fc-cache` get
the root itself. The pixbuf loader cache cannot take a directory, because the tool writes the path
it was compiled with. Under `--root` it therefore runs as the root's own
`gdk-pixbuf-query-loaders`, through `chroot`. That needs root, which `kdos update` has when it
installs into the inactive slot, and kpkg warns rather than skips when it lacks it.

## Vendoring

The build has no network, so a program whose language fetches its dependencies at build time
(Rust crates, Go modules, Python packages, Hackage packages) needs them downloaded in advance. That
is vendoring: `ports/fetch` runs the language's own tool against the port's source and packs what
it downloads into `<name>-vendor-<version>.tar.xz` beside the tarball. `build.sh` unpacks that
bundle and builds offline. Set `vendoring =` in the recipe to ask for it; 123 ports do (60 Rust, 34
Go, 25 Python, 4 Haskell).

A bundle with a `sha256 =` line is an archived source like any other: `ports/fetch` takes it from
the port directory, the cache or the source archive first, and generates it only when none of them
has it. A generated bundle is held to the recipe's hash like a download. Generating needs `cargo`,
`go`, `npm`, `pip` and `cabal` at the versions this tree compiles with, so by default it runs in the
`kdos-fetch` container (`ports/Containerfile.fetch`, built with Docker or Podman, whichever is on
`PATH`); `KDOS_FETCH_HOST=1` generates with this machine's own toolchains instead.

| `vendoring` | Produces | Build then |
|---|---|---|
| `rust` | `vendor/`, `.cargo/config.toml` and `Cargo.lock`, from `cargo vendor` | `cargo build --frozen --offline` |
| `go` | The module `vendor/` tree, from `go mod vendor` | `go build -mod=vendor` |
| `python` | Source distributions only (`pip download --no-binary :all:`), including each one's build requirements | Install from the local directory |
| `haskell` | Hackage source tarballs and their revised `.cabal` files, as a local repository | `cabal v2-install` against that repository alone, or upstream's bootstrap script |
| `node` | `node_modules/`, from `npm install --ignore-scripts` | Supported by `ports/fetch`; no port uses it |

A recipe with `pypackages` gets a Python bundle whether or not it sets `vendoring`.

### Where the bundle goes

Unpack it where the tool will be standing, not beside the manifest. Cargo finds its configuration
by walking up from the current directory, so a build that invokes it from a subdirectory never
reads a configuration placed next to the manifest — and every crate in the bundle resolves as
*missing* while sitting in the vendor directory the whole time.

`vendordir` is the other half: it says where the vendoring tool must run, which is beside the
manifest. The two directories are not always the same place.

`vendorsync` names every other Cargo manifest the build runs, space-separated and relative to
`vendordir`. A helper crate outside the workspace — an `xtask` that generates a manual page —
resolves against its own lock file, so a bundle made from the top manifest alone lacks its crates
and the offline build fails naming the first one. `ports/fetch` passes each entry to
`cargo vendor --sync`, and the one bundle and its one configuration then serve every manifest.
`oxipng` is the example: `vendorsync = xtask/Cargo.toml`, and `build.sh` runs
`cargo run --frozen --offline --manifest-path xtask/Cargo.toml -- mangen` from the top of the tree.

### The three Python keys

By default a Python bundle vendors everything `requirements.txt` and the package's own metadata
name, plus their build requirements. Three keys narrow that.

`pyrequirements = no` says `requirements.txt` is not the dependency set. The filename is a
convention with no defined meaning, and projects often use it for the *optional* list: `visidata`'s
reaches pandas, scipy and a Fortran compiler, for a program whose actual dependency is a single date
library. With the key set, the source distribution's own metadata is vendored instead, and the
fetch prints that it skipped the file. 13 ports set it.

`pypackages` is an explicit closure: space-separated PyPI names, each optionally pinned as
`name:version` (`pypackages = ruamel.yaml vobject:0.9.8`). It is downloaded without resolving
dependencies, because that key *is* the closure the recipe wants. Letting the tool resolve from there drags in every dependency
that is already a port and builds each one's metadata to find that out.

`pyruntime = no` says the runtime dependencies are all ports already, so only what the *build*
needs is vendored. Without it, downloading a runtime dependency such as `numpy` can drag in that
package's own build chain. 11 ports set it.

What a bundle installs into `site-packages` is named, and installed `--no-deps`. pip skips a
requirement that is already installed in the build root, so a resolving install packages whatever
no earlier port happened to install, and which package owns a module then follows build order —
removing the one that owns it breaks the other with an `ImportError`. A module that more than one
program imports is therefore a `python3-*` port of its own in both `depends` lines, never a member
of either bundle: `wcwidth`, `urwid`, `configobj`, `pytz`, `click-log` and `rich` with its
`markdown-it-py` and `mdurl` are the shared ones that `khal`, `khard`, `vdirsyncer`, `toot`,
`ipython`, `python3-esptool` and `ocrmypdf` would otherwise each carry.

A Python package's declared build backend is part of its version pin. Read the build-system
requirements before picking a version: a version that requires a newer backend than the tree carries can
cost several additional ports.

`python3` marks its `site-packages` as externally managed (PEP 668). The marker does not affect
`pip install --root="$PKG"`, `--prefix` or `--target`, so a port's install into its package needs
nothing. A `pip install` into the build root itself — the two backends `vdirsyncer` installs from
its bundle before its own build, `expandvars` and `hatch-fancy-pypi-readme` — is refused unless it passes `--break-system-packages`.

### The Haskell bundle

`vendoring = haskell` writes a `vendor/` directory that cabal reads as a local `file+noindex`
repository: each dependency's `<pkg>-<ver>.tar.gz` from Hackage, the `<pkg>-<ver>.cabal` revision
in force for it beside the tarball, and `SHA256SUMS` over both. `ports/hackage-vendor` does the
downloading, and it checks every file against a hash from somewhere other than the download
before writing it.

The dependency set comes from the first of these that the port has:

- **`hsplan`**, a bootstrap plan inside the source — `plan-bootstrap-<seed>.json` under
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

Bundles are packed with the same flag set packages use — sorted, a pinned timestamp, fixed
ownership, single-threaded compression — because a plain archive records the extraction time and
the builder's identity, and the checksum beside a bundle would then be a hash of one particular
afternoon.

### A bundle no tool writes

`pdfium` carries a vendor bundle that `ports/fetch` cannot produce, because the dependencies it
holds are gclient checkouts and not a language's packages: Chromium's `//build`,
`third_party/abseil-cpp` and `generate_shim_headers.py`. Gitiles generates its archives on request
and no two downloads of one hash the same, so the bundle is the one in the source archive and
its `sha256 =` line is its identity. Rebuild it by hand on a version bump: take each repository at the revision the
new branch's `DEPS` names (`build_revision`, `abseil_revision`), and the script from the matching
Chromium tag, lay them out under `vendor/` as they sit in a checkout, and pack them with the flag
set above plus `--mode=go-w`. The recipe's `_build_rev` and `_abseil_rev` name those revisions,
and `build.sh` refuses a tarball whose `DEPS` disagrees with them. Record the new bundle's hash in
the recipe and publish it with `ports/publish pdfium`: no other checkout can produce it.

`bat` carries `bat-assets-<version>.tar.xz`, the inputs to its highlighting sets. bat embeds
`assets/syntaxes.bin`, `themes.bin` and `acknowledgements.bin` with `include_bytes!`. These are
serialized syntect sets compiled from 92 git submodules of Sublime grammars and TextMate themes,
and the release archive carries those submodules empty. The bundle is those submodules at the
tag's pinned commits, cut down to what bat's asset build reads: every `.sublime-syntax`, every
`.tmTheme`, and every `LICENSE*`, `NOTICE*` and `COPYING*` file. The cut is not a guess. Sets
built from it are byte-identical to sets built from the full checkout. `build.sh` unpacks the
bundle over the empty directories and applies upstream's `assets/patches`. It builds once, runs
`bat cache --build --blank --acknowledgements --source=assets --target=assets`, and builds again.
The second build recompiles only the bat crate. `--source` stays relative, because the syntax set
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

`testing/preflight.sh` accepts an archive in a port directory if a `source =` line resolves to it,
if it is the vendor bundle, or if it has its own `sha256 =` line. `kpkg` verifies every `sha256`
entry, including the ones no source names. Any other archive fails preflight when git tracks it;
an untracked, gitignored one is a stale fetch that a version change left behind, and preflight
lists it as safe to delete instead (a copy stays in `ports/.srccache`). Preflight also fails an
archive that is empty or whose first bytes are none of gzip, bzip2, xz, zstd, lzip, zip or tar,
which is what a mirror's HTML error page saved under a tarball's name looks like, and fails any
recipe-hashed archive that git tracks, because sources belong in the archive and not in git.

### A Rust port's version is pinned by this tree's compiler

Cargo refuses a crate whose declared minimum Rust version is higher than the toolchain, rather than
degrading. Pin the port to the newest release that builds, because bumping one means bumping the
toolchain, the fetch container and every vendored bundle together — a wave, not a version bump.

The declared minimum is not an oracle. It gates the refusal; it says nothing about what the code
actually uses, so a release declaring an older minimum can still fail on a feature stabilised
later. The only reliable test is compiling.

## A second version beside the first

Some software cannot move to the version the tree ships, and gets an older one installed beside it.
Neither copy may write a path the other writes, or `kpkg` reports a conflict and one package
shadows the other. There are three ways to keep them apart.

**Versioned names.** `lua54` renames every path it installs: the interpreter is `lua5.4`, the
headers are in `include/lua5.4`, the library is `liblua5.4` and the pkg-config file is
`lua5.4.pc`. This works when the upstream build lets every name be chosen.

**The runtime only.** `openssl3` installs `libssl.so.3` and `libcrypto.so.3` and nothing else: no
headers, no `libssl.so` link, no pkg-config file, no `openssl` program and no manual pages. Every
port in the tree builds against `openssl`, which is 4.x and whose sonames end in `.4`, so nothing
here links the slot. It exists for a binary built elsewhere against OpenSSL 3 — an AppImage, a
vendor tool, a `.so` a program loads — which the loader otherwise refuses for want of
`libssl.so.3`. It follows the 3.5 long-term line (`series = 3.5`), so a binary that needs a symbol
added in 3.6 still fails to load. It is built `no-module`, so the legacy provider is inside
`libcrypto.so.3` and the slot never loads OpenSSL 4's `ossl-modules/legacy.so`, and it reads the
same `/etc/ssl/openssl.cnf` and certificate store as `openssl`.

**A private prefix.** `llvm21`, `clang21` and `lld21` install everything under `/usr/lib/llvm21`:
`bin`, `lib`, `include` and `lib/cmake`. LLVM needs this, because its sonames already carry the
version, but its headers (`include/llvm`), its CMake package (`lib/cmake/llvm`) and its tools
(`llvm-config`, `clang`) do not. Nothing under the prefix is on `PATH` or on the linker's search
path, so every consumer that does not ask for the slot still finds the system LLVM. The prefix is
the recipe helper `_prefix`, derived from `version`. The slot is built the way the system LLVM is:
`llvm21` and `clang21` as one shared object per component, `lld21` as static archives, all under
the prefix, and the programs and shared libraries find each other through their `$ORIGIN/../lib`
run path. zig links `libclang-cpp.so`, which clang
builds in either shape.

A consumer asks for the slot with build flags and a `depends` line naming the slot ports instead
of `llvm`. `zig` passes `CMAKE_PREFIX_PATH=/usr/lib/llvm21`, which makes its `llvm-config` search
look in the prefix before `PATH`. The zig binary it produces records `/usr/lib/llvm21/lib` as its
run path, because zig adds the directory of every shared library it links to the run path when it
builds for the host.

The LLVM ports take their sources in two ways. From 22 on, upstream publishes only the whole
`llvm-project-<version>.src.tar.xz`. Each port of the current series (`llvm`, `clang`, `lld`,
`lldb`, `libclc`, `libunwind`, `openmp`, `compiler-rt`) carries that one tarball and configures
its own directory with `cmake -S <dir>`. The runtimes (`libunwind`, `openmp`, `compiler-rt`)
configure `runtimes` and name themselves in `LLVM_ENABLE_RUNTIMES`, which is the build upstream
supports for them: `openmp`'s own directory refuses to configure on its own. `compiler-rt`
installs into clang's resource directory, `/usr/lib/clang/<major>`, where the driver looks for
`libclang_rt.*`, and `openmp` installs `libomp` without its `libgomp.so` alias, which is gcc's.

The 21 slot uses the per-component tarballs, which were still published for 21.x. `lld21`
carries `libunwind-<version>.src.tar.xz` as a third source and moves it beside its own tree,
because the Mach-O linker includes `mach-o/compact_unwind_encoding.h` from
`../libunwind/include` relative to the LLVM source tree, and the header must come from the same
release as the linker. The `libunwind` port installs that header only under its private prefix,
which is neither beside the source tree nor on a default include path, and it is the current
series, not 21.

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
the cache one file for both paths. Its `version` must equal the first port's: the typelib describes the library installed
beside it. Both recipes carry `group = glib`, so the version checker offers the two only as one
bump; `download.gnome.org` is no forge, and without the key each would be offered alone. Its meson
options are the first port's, except the one being turned
on.

A port that builds introspection data from a glib library names both `gobject-introspection` and
`glib-introspection` in `depends`: `g-ir-scanner` comes from the first, and every GIR it writes
includes `GLib-2.0`, `GObject-2.0` or `Gio-2.0` from the second. `libqrtr-glib`, `libmbim`,
`libqmi` and `modemmanager` do.

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

## What is built from source

Everything the host installs and runs on its own processor is compiled here from source; the
Debian packages in boxes are outside the rule. A recipe that installs a program, a library, a
module or a shared object it did not compile breaks the claim the whole tree makes: the binary cannot be read, cannot be rebuilt by `kdos rebuild`, and carries whatever its
builder put in it. Four classes of payload are exempt, and each is exempt for a reason the next
recipe has to be able to name.

| Class | What it covers | Why it cannot be compiled here |
|---|---|---|
| Code for another processor | `linux-firmware`, `intel-ucode`, `sof-firmware`; the closed EU kernels `intel-media-driver` compiles in with `ENABLE_KERNELS=ON` and `BUILD_KERNELS=OFF`; the assembled i965 shader kernels `libva-intel-driver` includes from `src/shaders`; the SOF coefficient `.bin` files in `alsa-ucm-conf`; the flasher stubs and flash algorithms inside `espflash`, `probe-rs`, `python3-esptool` and `openfpgaloader`; the riscv64 EDK2 image `qemu` installs from its `pc-bios/` — every other guest firmware it ships is compiled from its `roms/` | It runs on a DSP, a GPU, a microcontroller or a guest, not on the host, and for most of it no source is published |
| Compiled font data | `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`; the faces bundled inside `mupdf`, `matplotlib` and `seqkit` | Upstream publishes the built face, and the sources compile through a toolchain or a source tree this one does not carry. A face whose upstream build runs on ports is compiled: `ttf-dejavu`, `terminus-ttf` and `noto-emoji` |
| Compiler bootstrap seeds | The `rust` stage-0 `rustc`, `rust-std` and `cargo`; the `go` bootstrap toolchain; `zig`'s `stage1/zig1.wasm`; the upstream musl GHC `ghc` builds with | A compiler written in its own language needs a working one first. Each seed is used only to build, and never ships |
| Data with no other source form | The `tesseract` English model, the `perl-xml-parser` `.enc` encoding maps, the JavaScript in `libkiwix`'s skin, the `fcitx5-chinese-addons` pinyin and stroke tables, `john`'s `.chr` files, the RP2350 boot-ROM tails `picotool` embeds from `model/`, and recorded audio such as the `speaker-test` samples in `alsa-utils` | The file is the form upstream maintains; there is nothing earlier to build it from |

Every exempt payload is still a `source =` line with a `sha256`, so the offline build and the
checksum hold for it exactly as for a tarball of C.

What follows for a recipe:

- **A prebuilt object for the host that fits no class is removed.** When a source tarball carries
  one, use the flag that rebuilds it, or delete it from `$PKG` after the install. `go` deletes the
  race detector's runtime and the BoringCrypto module; `john` deletes the `ztex` bitstreams and
  controller image, which no program it builds can load. Shipping it makes the package contain a binary nobody here compiled.
- **A new exemption names its class.** A payload that needs one of the four goes in the table
  above and in the [inventory](../01-philosophy/why-kdos.md#what-is-not-built-from-source) in the
  same change, or the list of exceptions stops being complete and stops being worth reading.
- **A bootstrap seed never reaches `$PKG`.** What ships is what the seed built. A recipe that
  installs the seed ships a compiler this tree did not compile.

## Rules a recipe must keep

These conventions apply to every recipe in the tree, including KDOS's own under `src/`. Several are
checked by `testing/preflight.sh`.

| Rule | Why |
|---|---|
| **No explanatory comments in `kpkgbuild`**: the banner header plus the metadata keys, nothing else | The recipe is data. Reasoning belongs in the commit message or in this book. `build.sh` is a script and carries the comments any script does |
| **No source edits with stream editors** (`sed -i` and the like) | Use a build flag. Patch only where there is genuinely no flag, and then ship a real `.patch` beside the recipe, so the change is reviewable and covered by the recipe hash |
| **Every optional feature explicit, and every library it needs in `depends`** | Many build systems answer a missing library by quietly disabling the feature. Dropping a dependency then gives a build that succeeds and is narrower than its recipe claims, and the library is simply absent from the host |
| **Every meson `-D` is an option the port defines** | meson stops at setup on an unknown option. Preflight checks each one against the tarball's own option file, and checks the two option types that take a closed set of values |
| **A command named in a diagnostic is in single quotes** | A backtick inside double quotes is a command substitution, not a name: an `echo` telling somebody to run something runs it instead. Preflight checks the build system's own scripts under `script/` |
| **Nothing reaches the network** | The build runs with no network. A meson subproject fallback, a CMake download call, or a Python build backend fetching a tool from a package index all fail hours in. See [Build troubleshooting](build-troubleshooting.md#a-build-that-reaches-the-network) |
| **Shipped configuration uses only glyphs the console font has** | The console font holds 512 glyphs, a kernel limit. A Nerd Font icon is a private-use codepoint it cannot carry, so on `tty1` it renders as a blank cell in front of every name. Turn icons off where the program has a switch (`yazi`'s `[icon]`, `starship`'s `format`, `eza --icons=never`), check the default before writing anything (`lazygit` 0.65's is already off), and where a program draws them with no way to turn them off, add it to [known gaps](../06-reference/known-gaps.md). The answer is never a patched console font |

## Worked example: frotz

`ports/core/frotz` exercises most of the format in one recipe. It renames its source, ships a
desktop entry that opens a file, and defines a MIME type that a shared index has to pick up on the
target.

The metadata declares the source under the name the tree expects:

```
name        = frotz
version     = 2.55
release     = 1
source      = $name-$version.tar.gz::https://gitlab.com/DavidGriffith/frotz/-/archive/$version/frotz-$version.tar.gz
sha256      = a8c4c4d7…  frotz-2.55.tar.gz
description = Z-machine interpreter — Infocom-era interactive fiction in a terminal
homepage    = https://661.org/proj/if/frotz/
depends     = ncurses pkgconf
```

The `::` form names the saved file outright. For a first source the automatic rename would give the
same `frotz-2.55.tar.gz`; spelling it out keeps the name fixed even if the URL's shape changes.

`build.sh` is the make-only shape, with two options chosen rather than defaulted:

```bash
export CFLAGS="$CFLAGS -Wno-error"
make curses PREFIX=/usr SOUND_TYPE=none
make install PREFIX=/usr SOUND_TYPE=none DESTDIR=$PKG
```

The curses interface and no other: the SDL one wants a window server and the dumb one has no screen
model. `SOUND_TYPE=none` keeps an audio stack off every image for the handful of stories that use
sound. frotz's makefile adds no `-Werror` and no phase adds one, so `-Wno-error` changes nothing today;
it keeps a `-Werror` in a later upstream release from turning this compiler's newer warnings into
failures without a patch (see [An upstream `-Werror`](build-troubleshooting.md#an-upstream--werror)). The flags are exported through `CFLAGS` and the make variables are
upstream's own configuration knobs, as the [make-only shape](#make-only) describes.

The package carries no story. The desktop entry is `Exec=frotz %f` with `Terminal=true`, the shape
every terminal program here that opens a file uses, so a story is opened from the file manager or
the opener chain. That entry claims two MIME types, so the port has to define them first —
`shared-mime-info` 2.5.1 has neither:

```bash
install -Dm644 /dev/stdin \
	"$PKG/usr/share/mime/packages/kdos-zmachine.xml" <<'MIMEXML'
…
MIMEXML
```

and installing the package rebuilds the MIME database on the target, which is the one job
`build.sh` cannot do — see [Shared indexes](#shared-indexes).

## Adding a port, end to end

The whole procedure, from an empty directory to a source other people can fetch. Each step links
to the section that explains it.

1. **Find the canonical upstream URL and the latest stable version.** Watch for projects whose
   releases are on a different host from their documentation, and for archives whose top-level
   directory is not `<name>-<version>`. List the archive's contents before writing the recipe.
2. **Write `kpkgbuild`** in `ports/core/<port>/`, with the banner header, the
   [keys](#keys-the-package-manager-reads), and any [helpers](#recipe-helpers) between `release`
   and `source`. Leave the `sha256 =` lines out for now.
3. **Fetch the source and record its checksum:**
   ```sh
   ports/fetch <port>
   sha256sum ports/core/<port>/<name>-<version>.tar.*
   ```
   With no `sha256 =` line, `ports/fetch` downloads from upstream and warns
   `no sha256 for <file> in the recipe`. Add the line (`sha256 = <hash>  <file>`) straight away:
   `kpkg` refuses to extract an unhashed source, and nothing else can verify it. For a port with
   `vendoring =`, the same run generates `<name>-vendor-<version>.tar.xz`; hash and record that
   file too. `make fetch` takes no port name and walks all 1,014 ports, so use `ports/fetch <port>`
   here.
4. **Write `build.sh`** from the [canonical shape](#canonical-build-shapes) for its build system.
5. **Wire it in.** Name the port in the `depends` line of whatever needs it. A port something
   depends on is built as part of that dependency's closure and needs no list entry; a port nothing
   depends on goes in the `packages.txt` of the phase it belongs to, usually
   `script/04_phase4/packages.txt`.
6. **Check the wiring**, in about two minutes rather than hours into a build:
   ```sh
   testing/preflight.sh
   ```
   It checks, among much else, that every `depends` name exists, that every list resolves to a
   build order, that every source is hashed, and that every meson option exists.
7. **Build only that port**, and package the result:
   ```sh
   make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"
   ```
   Drop `06_packaging` to build the package without making an ISO.
8. **Read the log** under `build/logs/04_phase4/` and, when the build fails, look the message up in
   [Build troubleshooting](build-troubleshooting.md).
9. **Publish the source** before pushing the commit (see [Publishing sources](#publishing-sources)).
   Uploading needs a maintainer's token. With one:
   ```sh
   ports/publish <port>
   ```
   Without one, run `ports/publish --check <port>` to list the hashes the archive lacks, and name
   those ports in your pull request so a maintainer publishes them.

## Checking a recipe change

`kpkg verify` answers "what does my change actually change in the package?" on a KDOS system or in
the build chroot. Write the changed recipe as `kpkgbuild.new` beside the current one (and, if the
build changes too, `build.sh.new`; without it the current `build.sh` is used), then:

```sh
kpkg verify <port>            # build with kpkgbuild and with kpkgbuild.new, compare the two
kpkg verify --repro <port>    # build the SAME recipe twice; require byte-identical results
```

Neither build happens in the ports tree: a scratch directory is filled with symlinks to everything
the port carries, and only the files being tested are real. An interrupted verify leaves the tree
untouched.

The comparison is over payload, not the file list. A file list answers "are the same files there"
and is silent about the same paths with different contents, which is the error that starts
mattering the moment recipes change. The comparison is a per-member fingerprint including modes and
link targets, and it prints the first differing lines.

`--repro` is the acceptance test for reproducible packaging, and it is the same code path because
it is the same question asked of two archives.

## Checking for new versions

The version checker tells you which ports have a newer upstream release, and can apply the bump
for you. It is `kdos-portup` (source in `src/tools/kdos-portup`), run through `ports/update`, which
compiles it into `ports/.portup` the first time and whenever a `.c` file under
`src/tools/kdos-portup` is newer than the binary. A changed header, or a change to a `libk*`
library it links, does not trigger that; delete `ports/.portup` to force a rebuild. It needs network
access, `curl` and `git`, and it never runs version control on your tree.

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
letter: `y` accepts it, `n` skips it, `d` shows the recipe diff, `a` accepts this and every later
one, and `q` stops.

The rest of this section explains how the checker decides. It asks one question per port: does upstream have a release newer than the pin? It
answers in five steps.

1. **Discover.** Ask upstream what it has released, through the first adapter below that finds
   anything.
2. **Read.** Take a version out of every name the adapter returned, through the recipe's own URL.
3. **Filter.** Drop what is not a later release of this numbering: another scheme, a pre-release, a
   development series.
4. **Compare**, walking from the highest candidate down.
5. **Prove.** Render the candidate through the real recipe parser and request the result. The
   recipe is copied to a temporary directory, the version substituted, and the metadata expanded by
   the same parser the build uses, so a helper chain falls out for free and no probe ever touches
   the real tree. A not-found drops that candidate and tries the next-highest.

Correctness comes from the last request, not from discovery. A listing can name a version whose
archive lives somewhere the recipe's template does not expect, and the tool would rather try the
next candidate than report a version it never confirmed the build could fetch.

### Discovery

The URL a recipe's first `source` names — with any `cachename::` prefix removed, since that half is
where the archive lands and not where it comes from — decides which adapters apply. They are tried
in this order:

| Adapter | Applies to | Asks |
|---|---|---|
| Watch | A recipe with a `watch` key | That forge repository's tags, or that page read through the file name, one hop further as the homepage is |
| Registry | `files.pythonhosted.org`, `static.crates.io`, a CPAN `authors/id/` path | PyPI's JSON (withdrawn and empty releases skipped), the crates.io API (yanked versions skipped), MetaCPAN's latest release |
| Forge | GitHub (and `raw.githubusercontent.com`), Codeberg, sr.ht, Bitbucket, GitLab on any host whose URL carries `/-/`, a cgit `/snapshot/` URL | `git ls-remote --tags`. When git cannot answer: GitHub's tag and release feeds, Codeberg's and sr.ht's feeds, GitLab's tags API, cgit's tag page. When nothing answers and the URL names a commit, the branch heads (below) |
| SourceForge | `downloads.sourceforge.net`, `sourceforge.net/projects/…/files/`, a project web host | The project's file feed, narrowed to the path above the first directory named for the version |
| Directory | Anything else | The listing of the archive's directory — or, when a directory in the path is named for the version, of its parent (below) — and the download page one hop from either |
| Homepage | A recipe with a `homepage`, whose source is on no forge or registry and names its version in the file name | The homepage's forge's tags, or the page's links read through the file name, and one hop further |
| Repology | Every port, last | The package-tracking service, rate-limited to one request a second across the whole run and marked low-confidence; it is never upstream itself |

Tags come from git rather than a forge's feed because git names every tag, unauthenticated and
unmetered; a feed carries the newest ten, which a project that tags each of its crates, or maintains
two majors at once, fills with the wrong ones. git runs without the user's own configuration and
over https only, so a `url.*.insteadOf` rewrite to ssh cannot ask for a key from every worker at
once.

Git also names the commit behind every tag. A tag on the commit of a lower release, with a release
of other code between the two, is a mistake rather than a release and is dropped: thermald's
`v2.15.10` sits on `v2.5.10`'s commit, and offering it would downgrade 2.5.13 under a higher
number. A tag on the commit of the release just before it stays — sby tags every yosys version,
changed or not — and so does a release sharing its commit with a series tag that follows it
(corrosion's `v0.6` on `v0.6.1`).

A GitHub project that publishes a release for some of its tags and not for others leaves the tags
past its newest release in doubt. They may be engineering drops — intel/media-driver tags `26.2.1`
to `26.2.4` and publishes `26.2.4`, and its `26.3.x` tags are the next quarter's — or releases
whose release object is late: bindgen's `0.73.x` were on crates.io before GitHub had a release for
them. Nothing in the tags tells the two apart, so they stay candidates. When a tag later than the
pin exists, the releases API is asked; if a tag of the pin's numbering between its oldest and
newest release has none, and the pin is no later than the newest, the answer is marked
low-confidence and names the newest tag without a release. The API allows sixty requests an hour
and is asked only for a port that already has a later tag; when it refuses, the tags stand
unmarked, so the verdict is the same either way.

When the recipe's tag prefix names nothing later than the pin, the same list is read again with a
`v` or no prefix. A family there that starts after the recipe's own ends is a change of scheme —
sby's tags include both `yosys-0.47` and `v0.48` — and its newest release makes the answer unknown,
naming it, since the recipe's template cannot fetch it and *current* would be wrong. A family whose
numbers run alongside the recipe's is another project in the same repository (golang/tools'
`v0.50.0` beside `gopls/v0.23.0`) and says nothing.

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
  (`mpfr-current/`) are read the same way, those below the page's own directory first — three at
  most. A directory that cannot be read at all (403, 404) is replaced by its parent, which is
  usually the project's page: netfilter's `files/` beside `downloads.html`, musl's `releases/`
  beside a homepage that links the newest tarball.

Only a name read through the file name counts on any of these pages; a page is prose, and prose is
full of numbers.

SourceForge's mirrors keep every file a project uploaded after the project leaves, so a file feed
whose newest file is the pin is not proof of *current*: gnu-efi's releases are on GitHub and
libjpeg-turbo's on its own site, while their SourceForge feeds end at `3.0.18` and `3.0.1`. When the feed ends at the pin, the
project's own SourceForge record is asked. A `moved` status sends the check to the repository it
names: tags there that name the pin and nothing later mean the project still uploads its releases
to SourceForge (procps-ng, psmisc) and the answer is *current*; anything else — a later tag, no
tag of the pin, a repository git cannot list — makes it unknown, naming where it went. The tags
are read through `v`, no prefix and the file's own, and failing those as a word followed by
nothing but numbers and separators (smartmontools' `RELEASE_7_5`). Without a move repology is
asked, and a version it calls upstream's newest that is later than the pin, of the pin's line and
no pre-release, makes it unknown too; a project repology cannot answer for stays current.

A source that names a commit, in a repository that tags no release, has one question left: is the
commit still the tip of the default branch or of a branch named for releases (`master`, `main`,
`stable`, `trunk`, `release…`)? At the tip, the port is current, and says so; anywhere else it is
unknown. A version from any other adapter cannot be rendered into such a URL, so none is asked.

`watch` is for the host no general rule reaches: a release index served as JSON to a script
(`downloads.unidata.ucar.edu/<project>/release_info.json`), a channel manifest
(`static.rust-lang.org/dist/channel-rust-stable.toml`), a browsable mirror of a host that serves
no index (`build.openvpn.net/downloads/releases/`), or a forge repository nothing on the project's
site links. It is tried first and still read through the recipe's own file name.

A version written into a directory — `gnu/gcc/gcc-15.2.0/`, `ftp/python/3.14.2/`,
`sources/pango/1.57/`, `dist/v8/`, `kernel/v7.x/` — means the archive's own directory never holds a
newer release. The checker lists the parent of the outermost such directory and reads its siblings
through that directory's own name, keeping those at or after the current one.

- **Siblings named for a whole version** (`gcc-16.2.0/`): the three newest are read, so a `3.15.0/`
  holding only `3.15.0a1` is not taken for a 3.15.0 release. A newer one past those three stays a
  candidate unread, for the proof step to settle, and so does one whose files the recipe's archive
  name cannot read — no version in it, only its directory's — since nothing in it can be judged a
  pre-release.
- **Siblings named for a series** (`1.58/`, `v9/`): the three newest are read, and the current
  series as well, since only a series' files say which releases it holds. A later series that could
  not be listed leaves the answer unknown, never current.

A sibling is a link below the listed directory, resolved against the page it was read from: a
link in the page's chrome (a footer's `linkedin.com/company/29561/`) is no later series. A page
whose versioned links all lead elsewhere is not a listing, and its text is not read for siblings
either — launchpad's series page names "Ubuntu RTM 14.09". A walk whose parent names no sibling at
all, not even the current one, reads that page's download links instead (`mpfr.org` links only
`mpfr-current/`).

A walk makes at most eight requests.

### Reading a version

The recipe's URL says where its version sits: `gcc-15.2.0.tar.xz` is `gcc-` then the version then an
archive suffix, and a tag `llvmorg-21.1.8` is `llvmorg-` then the version. A name is read only when
it has the same text on either side — any archive suffix standing in for the recipe's. A directory
holding every X library, every suckless tool or every GNU pretest then yields this port's versions
and nobody else's. The version may be spelled with `_` or `-` for its dots (`boost_1_89_0`,
`R_2_7_3`), with a mix of them (`ImageMagick-7.1.2-31` for `7.1.2.31`) or with none at all
(`gs10071`, `unzip60`), and reads back dotted. A pin of digits alone may be written in groups
(`2026-08-13` for `20260813`) and reads back with the separators dropped, at the same
group widths only. Only a URL with no version in it at all falls back to extracting every
version-shaped run and keeping those shaped like the pin.

### Filtering

- **Class.** Dotted numbers of any length are one numbering — binutils 2.45.1 and 2.47 are one numbering —
  with a pre- or post-release marker (`1.4rc5`, `10.2p1`, `1.5.8.pl02`) or a trailing commit id set
  aside. A leading year and a zero-padded part the pin does not pad (`600.0132` beside `26.2.4`) are
  each another. So is any word that is not a pre- or post-release marker, a lone letter straight
  after a digit (`3.6a`, `1.1.1w`), or a word the pin itself carries (`1.9.0.jumbo1`): a platform
  build (`3.8.13-w64`), a patch beside a release (`1.8.1.3.patch`), a variant (`5.1.22_dict`) and
  an archive's own suffix (`56.7z`) are files, not releases. Repology's strings are distributions'
  spellings and must match the pin's exact shape.
- **Pre-release.** `rc`, `alpha`, `beta`, `pre`, `preview`, `dev`, `snapshot`, `wip`, `test`,
  `nightly`, `unstable`, `trunk`, `cr`; and PEP 440's `a1`/`b2`, though never inside a trailing
  commit id (passt's `2025_02_17.a1e48a0`). A port that pins a pre-release follows that line.
- **Pretest.** 90–99 in the third place or later — `1.25.91`, `4.4.0.90`, `26.0.99.902` — where the
  pin has less there, unless the listing or the pin has 80–89 in that place with the same leading
  numbers: a counter walks up through the eighties (`1.0.89`, then `1.0.92`), a pretest jumps
  there. 100 and up is always a counter. A registry's answer is exempt, since the registry marks
  its own pre-releases.
- **Development series.** Declared by the recipe's `devseries` key, space-separated: `odd-minor`
  (an odd second number is a development series — GLib, Perl) and `preview-minor` (a second number
  of 90 or more previews the next major — Pango 1.90 is Pango 2). `gstreamer.freedesktop.org` is
  `odd-minor` without asking. `download.gnome.org` is not: libxml2 2.15 and librsvg 2.63 are stable
  there, so its odd-minor projects carry the key.
- **Series.** A port that stays on one line declares it with the `series` key: a version prefix,
  matched on whole components, so `21` holds `21.1.8` and not `210.1`, and `5.4` holds `5.4.9` and
  not `5.5.0`. A release past it is never a candidate, and a directory walk skips a sibling that
  holds none of the line. The newest such release is remembered: a port with nothing newer in its
  line is *current*, and the answer names what is past it — `held to series 21; newest upstream
  23.1.2`. The slots (`llvm21`, `clang21`, `lld21`, `lua54`, `openssl3`, `docbook-xml`) carry it,
  so do ports held to an upstream major line (`openldap` on 2.6, `pngquant` and `zxing-cpp` on 2),
  and so does a hold a consumer forces (`python3-pydantic-core`, at the one version the pydantic
  vendored in `ocrmypdf` names).

### Proving

A rendered URL identical to the recipe's own proves nothing — the version is not in it — and is
never requested. A host that answers HEAD 403, 405 or 501 is asked again with a one-byte ranged
GET, since refusing a method is not the same as having no such file. Three misses in a row within a
major newer than the pin's skips the rest of that major: a template that cannot fetch its three
newest releases cannot fetch any of them (`SDL2-<v>.tar.gz` under SDL 3's tags). At most twenty
candidates are requested per port. A proved candidate that is not the newest upstream names comes
with the newest in its line — a series directory written `${version%.*}` turns `0.21.8.2` into a
`0.21.8/` upstream never made — so the review shows what the template cannot fetch.

When every candidate misses, the newest one's file — the name the template gives that version — is
looked for on upstream's own site: the recipe's homepage, and the download pages one hop from it
that the directory adapter would follow. A link to exactly that file, answering a request of its
own, leaves the answer unknown, since the template still cannot fetch it, but carries the file's URL
and names its host: chafa tags on GitHub and uploads to its own site, so a GitHub template reads
`newest 1.18.3 is at hpjansson.org, not at the recipe's URL`, and the recipe's `source` is what
changes. A page that merely mentions the file proves nothing; only the request does.

An HTTP request is tried three times, pausing two and then five seconds, when the answer is one a
busy host gives and a missing file does not: no response at all, 429, or 5xx. A 200 whose body
stopped part-way counts as no response, since a cut listing is missing its newest entries, and so
does a redirect whose next hop never answered. A git tag
listing has no status to read and is tried once more, after two seconds, on any failure. Repology's
retries wait for their turn under its one-a-second limit like any other request, and a 429 from it
waits ten seconds first.

### Outcomes

There are three outcomes, never two. *Unknown* is never folded into *current*, because that would
be a confident wrong answer — the one thing this tool must not give. A listing that could not be
reached, one whose tail was cut off by a cap or a failed transfer (archive indexes sort ascending,
so the dropped entries are the newest), a later series directory that could not be listed, a tag
list naming a later release under another prefix, one whose candidates all failed to resolve — naming upstream's own copy of the newest when its
site links one — and
a source URL with no version in it are each unknown, with a reason that names the newest version
upstream when one was seen. So are a source that is no URL, a recipe whose version is not written
in its source URL (a bundle numbered apart from its sources), a pinned commit no release branch is
at, and a SourceForge feed ending at the pin of a project whose new repository tags later or cannot
be read, or that repology knows a later release of. A *current* answer carries a reason too when there is more to it: a series held,
or a pinned commit at a branch tip. A *newer* one names a GitHub tag with no release, marked
low-confidence.

Checks run eight at a time (`--jobs`), each in its own process; a whole-tree run takes minutes rather
than an hour, and a check that dies before reporting leaves its port unknown. Results are cached for
a day in `ports/.update-cache.json`, each stamped with the checker logic that reached it; an entry
from other logic is checked again, so a changed checker never serves its predecessor's answers.
`--refresh` ignores the cache.

The grouping key is the forge organisation plus the current version, not a name prefix. A name rule
misses the member of a release family whose repository is named differently, while all of them
resolve to the same organisation and version and are correctly offered as a bump together. A
`group` key overrides the derived one.

Exit codes: 0 means every named port is current, 1 means `--check` found at least one update, and 2
means a bump was accepted but its archive never made it to disk — the one state this tool exists to keep a
build from inheriting silently.

The tool never runs version control on the tree. Accepting a bump for one port rewrites its
`version =` line, runs `ports/fetch <port>`, and then rewrites each `sha256 =` line whose file name
carries the old version to the new file's hash. If the fetch fails, the old version is put back.
It then prints two notes: the old tarball is still in the port directory (delete it when you no
longer want it), and `ports/publish <port>` is the next step, because the recipe now names a hash
the source archive does not hold. Publishing and committing stay your decisions.

A group bump is accepted for every member or none: each member's `version =` line is rewritten, each
is fetched, and a failed fetch for any member puts every member back. The `sha256 =` lines of a
group bump are not rewritten; record each member's new hashes by hand, as after `--no-fetch`.

## Publishing sources

Upstream archives are not committed. Git carries the recipe, and the archive it names is a release
asset in the `kunaldawn/kdos` repository, named by its own sha256 and stored under the
release `sha256-<first two hex digits>`. That is where every other checkout's `make fetch` looks
for it first (see [Where sources come from](developing.md#where-sources-come-from)). A new or bumped
source therefore has to reach the archive before the commit naming it is pushed, or the commit
builds on the machine that wrote it and nowhere else. Patches, configuration files and anything
else git tracks are not archived, even when a recipe hashes them.

Uploading needs a token with write access to `kunaldawn/kdos`, so publishing is a
maintainer's step. If you are contributing without that access, say in your pull request which
ports carry new or changed sources; `ports/publish --check <port>` shows which of their hashes the
archive lacks.

A file git tracks is never archived and never fetched: `ports/fetch` skips every path in the git
index, so a source tarball that is still in the index (as a Git LFS pointer, for instance) is
neither downloaded nor checked by `ports/fetch` or `make fetch-check`. `ports/publish` and the
pre-push hook do treat an LFS pointer as not carried and publish or check the real file.
Preflight fails every recipe-hashed archive git tracks; take such a file out of the index with
`git rm --cached <file>` so the fetch path handles it.

A version bump, end to end:

```sh
ports/update <port>                 # accept the bump: version and sha256 lines are rewritten, the source fetched
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"
ports/publish <port>                # upload what the archive lacks (maintainer token; else --check and say so in the PR)
git commit ports/core/<port>        # the recipe; the archive itself is gitignored
git push                            # the pre-push hook confirms every new hash is archived
```

After `ports/update --no-fetch`, or after a group bump, the `sha256 =` lines still name the old
files. Run `ports/fetch <port>` to bring the new files down from upstream (it warns that each has no
hash), then replace the old lines with the new hashes from `sha256sum`. Until you do, `kpkg`
refuses the unhashed archive and `ports/publish` has nothing to publish.

### `ports/publish`

```
ports/publish [--dry-run] [--check] [--history] [--freeze <kdos-tag>] [port…]
```

`ports/publish` uploads every file a `sha256 =` line names, under all of `ports/core` or only the
named ports, that git does not carry and the archive does not hold yet. A path git tracks as a Git LFS pointer is not carried: the pointer names the file
and is not the file. The bytes come from the port directory, the cache, or the local LFS store, and
are hashed again immediately before upload, because an asset whose bytes do not match its name
would poison every checkout that asks for it.

Presence is an anonymous `HEAD` on the asset's download URL, which costs no API quota, so a rerun
uploads only what is still missing and an interrupted run is resumed by running it again. Only a
404 counts as missing; any other status, or a network failure, stops the run with
`archive unreachable`, because an outage proves nothing about what the archive holds. Only a
missing asset reaches the API: the shard's release is looked up and created when absent, and the
upload is accepted only when GitHub reports its digest as the expected hash.

| Flag | Does |
|---|---|
| `--dry-run` | List what would be uploaded, with sizes and totals, and the shard releases it would touch. Needs no token and makes no API call |
| `--check` | Presence only: list what is missing from the archive; exit 1 if anything is |
| `--history` | Add every LFS object under `ports/core` that any ref's history names (`git lfs ls-files --all`, or every object in the store when git-lfs is absent), to seed the archive with what old commits' recipes point at. Objects at other paths are never wanted, since no recipe asks the archive for them. Only objects the local LFS store or the cache holds are wanted; the rest — old versions and paths this clone never downloaded, which `ports/fetch` cannot supply — are counted on one line and skipped without failing the run |
| `--freeze <tag>` | Write `build/freeze/sources-<tag>.sha256` for the tag's recipes, require every hash in it to be archived, and attach it as `sources.sha256` to release `<tag>` on `$KDOS_REPO`, creating a draft release when there is none. See [Cutting a release](developing.md#cutting-a-release) |

Exit status is 0 when everything wanted is archived, 1 when something is missing or an upload
failed, and 2 when an asset's digest disagrees with its name and could not be repaired. `--freeze` also
exits 2 when the release already carries a different `sources.sha256`; replace that asset by hand,
and only while the release is still a draft.

The archive is append-only. The one deletion `ports/publish` makes is of an asset whose name is a
hash and whose digest is a different hash, or an upload GitHub never completed: that asset is
corrupt by definition and holds the name, so it is deleted and uploaded once more. A second
disagreement exits 2. GitHub may report a completed asset's digest as null; such an asset is asked
for again and, if it still has none, downloaded and hashed. An unknown digest is never a reason to
delete.

The token is read from `$KDOS_SOURCES_TOKEN`, or from `~/.config/kdos/sources-token`, which is
refused when its group or others can read it. It needs write access to the contents of
`kunaldawn/kdos`. It reaches curl only through a mode-600 header file, never an argument, so
no process list shows it, and it is removed from the environment before any child starts.

GitHub throttles content creation separately from its hourly quota, so at least
`KDOS_PUBLISH_DELAY` seconds pass between creating calls — eight by default, which holds a long run
under 75 a minute and 480 an hour. A 403 or 429 waits for `retry-after` or `x-ratelimit-reset`, or
60 seconds for a bare 429 or a 403 naming a secondary rate limit, and retries. An upload that meets
a 5xx or a dropped connection is retried twice; the partial `starter` asset it may leave is
deleted and replaced.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_SOURCES_TOKEN` | `~/.config/kdos/sources-token` | The upload token |
| `KDOS_PUBLISH_DELAY` | `8` | Seconds between creating calls |
| `KDOS_LFS_STORE` | `<git dir>/lfs/objects` | The LFS store read for bytes and by `--history` |
| `KDOS_REPO` | `kunaldawn/kdos` | The repository whose release `--freeze` attaches to |
| `KDOS_GITHUB_API`, `KDOS_GITHUB_UPLOADS` | `https://api.github.com`, `https://uploads.github.com` | The two endpoints, replaced to run against a local stand-in |

`KDOS_SOURCES_REPO` and `KDOS_SOURCES_BASE` name the archive as they do for `ports/fetch`; an empty
`KDOS_SOURCES_BASE` stops `ports/publish` at once, since there is nothing to publish to.

### The pre-push hook

The pre-push hook stops you pushing a recipe whose source is not in the archive yet. It is
`script/hooks/pre-push`, and it is off until you enable it in your clone:

```sh
git config core.hooksPath script/hooks
```

That setting replaces `.git/hooks` entirely, git-lfs's own hooks included. Preflight reminds you
when the hook is not enabled. For every ref pushed,
the hook takes the hashes the pushed commit's recipes name that the remote side's recipes do not —
the remote's current commit for that ref, or every `refs/remotes/*` tip for a new branch — drops any
whose file git carries at that commit, and `HEAD`-checks the rest against the archive. Any missing
hash refuses the push, listing each as `port/file (hash)` and naming the `ports/publish <ports>`
command to run. Deleting a ref passes.

Only a 404 counts as missing. The hook also refuses the push when it cannot reach the archive
(`cannot reach the source archive at <base> (curl exit N)`), when the archive answers any status but
200 or 404 (`source archive unreachable at <base> (HTTP N)`), and when `KDOS_SOURCES_BASE` is empty.
Offline or during an outage, absence cannot be ruled out, and passing silently would let through
exactly the push the hook exists to stop. `KDOS_SKIP_PUBLISH_CHECK=1 git push …` bypasses the
check.

### What an archive is

- **Nothing is published that does not match its recipe.** The `sha256 =` line verifies the bytes,
  and `preflight.sh` checks that every recipe has one. `ports/fetch` keeps no copy that fails its
  hash, and `ports/publish` uploads none.
- **The hash is the identity; the URL is advisory.** With a hash, the cache, the archive and
  upstream are interchangeable, and the nearest is used. Without one — immediately after a version
  bump — upstream is the only source, because the archive cannot hold a file that has never existed
  here, and trusting another copy of an unverifiable file would be trusting the wrong thing
  entirely.

For a single-port bump, the version checker records the new checksums, for the source and the vendor
bundle both, in the same operation as the version, so the recipe is not left naming a file nothing
verifies. A group bump or a `--no-fetch` bump leaves that step to you.

## See also

- [Packaging](../03-architecture/packaging.md) — what a package is and how it is verified
- [Build troubleshooting](build-troubleshooting.md) — the recurring failures, by symptom
- [The build system](build-system.md) — how phases install what you wrote
- [Developing](developing.md) — the narrow rebuild loops
- [Testing](testing.md) — `preflight.sh` and what it checks about recipes
- [Decisions](../01-philosophy/decisions.md) — why a recipe is two files
- [Why KDOS](../01-philosophy/why-kdos.md#what-is-not-built-from-source) — every payload not built from source, port by port
