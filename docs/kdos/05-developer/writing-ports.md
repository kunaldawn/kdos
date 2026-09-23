# Writing ports

A port is the recipe for one package: where its source comes from, what it depends on, and how to
build it. This page covers the format in full, the canonical build shape for each build system,
vendoring for each language, and a worked example from choosing a version to a green preflight.

For what a package *is* and how one is verified, see
[Packaging](../03-architecture/packaging.md).

## Anatomy of a port

```
ports/core/frotz/
├── kpkgbuild                  declarative metadata — parsed, never sourced
├── build.sh                   the build — ordinary bash
├── postinstall.sh             optional install-time hook
├── *.patch                    optional
└── frotz-2.55.tar.gz          the source, tracked in Git LFS
```

`kpkgbuild` has no interpreter line and is never executed. Reading a recipe therefore costs no
shell and cannot run anything.

`build.sh` is a real script, so syntax checking, linting, highlighting and diffing all work on it.
`testing/preflight.sh` syntax-checks every one of them, which is impossible when the build lives
inside a configuration format.

The reasoning behind the split is in [Decisions](../01-philosophy/decisions.md).

## kpkgbuild

Every recipe opens with the KDOS banner header, kept verbatim, and then a block of `key = value`
lines:

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
recipe helper.

| Key | Required | Repeats | Means |
|---|---|---|---|
| `name` | yes | | The package name |
| `version` | yes | | Upstream's version. No hyphen — a package file is `<name>-<version>-<release>.tar.xz`, so a hyphenated version makes that name ambiguous and preflight refuses it. Spell a tag like `1.9.0-Jumbo-1` as `1.9.0.jumbo1` and carry upstream's own form in a helper the `source` line reads |
| `release` | yes | | Bump to force a rebuild for a reason the recipe hash cannot see |
| `source` | | yes | An upstream URL, or `filename::url` |
| `sha256` | | yes | `<64 hex>  <filename>`, one per declared file — every `source`, plus a vendor bundle or anything else beside the recipe the build opens |
| `description` | | | One line. It is read and printed — not a comment |
| `homepage` | | | |
| `depends` | | | One line, space-separated port names — the solver reads the first and stops |
| `vendoring` | | | `rust`, `go` or `python` — see [Vendoring](#vendoring) |
| `pypackages` | | yes | An explicit Python dependency closure to vendor |
| `secdb` | | | The name the security database uses, when it differs from ours |
| `bench` | | | A command `kdos march` times |
| `bench_setup` | | | A command that runs once and is not timed |

`description`, `homepage` and `depends` are keys, not comments — the parser skips a `#` line
entirely, so a fact written as one reaches nothing.

`source` and `sha256` accumulate across lines; `depends` does not. `kp_decl` appends a second
`depends` line into the variable `build.sh` sees, but `kp_depends`, which is what the solver and
`kpkg info` read, takes the first line and stops. Two `depends` lines are a dependency the build
order does not know about.

### Keys other tools read

These are ordinary helpers as far as `kpkg` is concerned — it stores them as variables and passes
them to `build.sh` — but another tool gives each a defined meaning:

| Key | Read by | Means |
|---|---|---|
| `vendordir` | `ports/fetch` | Where the vendoring tool must run, when that is not the top of the tree |
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
each depends on its position and its extension:

| Source | Saved as | Unpacked |
|---|---|---|
| The first, with a recognised archive extension | `<name>-<version>.<ext>`, whatever the URL's basename is | By extension, below |
| A later source, or a bare filename | The URL's basename | By extension, below |
| Anything named `filename::url` | `filename`, first or not | By extension, below |
| A tarball, first | | Into `$SRC`, with `--strip-components=1` |
| A tarball, later | | Into `$SRC_ROOT`, unstripped, beside `$SRC` |
| A data file, a `.zip` or a `.tar.zst` | | Copied into `$SRC` as-is |

The first source is renamed on purpose. A forge that generates an archive named after a tag would
otherwise leave every port holding a file called `2.55.tar.gz`, and the standardised name is what
the tree, the checksum line and the LFS listing all agree on. Use `filename::url` when the
generated name needs to be something else.

Checksums are matched to sources by **basename**, not by position, so reordering the `source` lines
cannot silently pair a hash with the wrong file. A source with no hash in the recipe is a refusal,
not a pass.

Two extension sets, and they are not the same. The standardised name is derived from `.tar.gz`,
`.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz`, `.tar.zst` and `.zip`; what is actually unpacked is
`.tar.gz`, `.tgz`, `.tar.bz2`, `.tbz2`, `.tar.xz`, `.txz` and `.tar`. So a first source that is a
`.zip` or a `.tar.zst` is saved as `<name>-<version>.zip` or `.tar.zst` and copied into `$SRC`
whole; a recipe that wants one unpacks it in `build.sh`.

A release archive carries a submodule's directory empty, because the archive is generated from the
repository without recursing. If upstream's build expects a submodule, add it as a second `source`
extracted where the build looks, or make it a port.

## build.sh

`build.sh` is sourced with the working directory set to the unpacked source. The recipe's keys and
helpers arrive as shell variables — not exports — alongside four paths:

| Variable | Is |
|---|---|
| `$name`, `$version`, `$release` | From the recipe |
| Every helper | From the recipe |
| `$PORT_SRC` | The port's own directory |
| `$SRC` | The unpacked source, and the working directory |
| `$SRC_ROOT` | The parent of `$SRC` |
| `$PKG` | The staging tree — install here |

The minimal autotools recipe is three lines:

```bash
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

## Canonical build shapes

Copy these rather than inventing.

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
setup on an unknown option, before a line is compiled, and there is no universal spelling — one
project's disable flag is fatal in the next. meson's built-in options are always valid.

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

Export the compiler flags; never pass them as a make argument. A variable on the make command line
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

Ten rules, each with a consequence.

Use `Terminal=true` and a bare `Exec`. Naming an emulator in `Exec` pins the entry to that one
emulator and defeats `X-KDOS-Term`. The launcher picks the emulator and supplies the identity — see
[`kdos-shell`](../04-programs/kdos-shell.md).

`X-KDOS-Float=true` and `X-KDOS-Size=COLSxROWS` say how the window should open. A float is an
unanchored window at the size the entry asks for rather than one the session places among the rest.
The size is in cells, and a terminal smaller than 4x2 is refused. `kdos app tui` writes both, and a
recipe writes them for the same reason it writes any other key — see
[the kdos command](../04-programs/kdos-command.md#app).

`X-KDOS-TUI=true` is `kdos app tui`'s own marker and a recipe must not write it. It means "this
command wrote this file", which is what makes `kdos app tui rm` safe. A recipe's entry carrying it
would be a shipped application that verb could delete.

Set `X-KDOS-Term=kdos-term` only where the program draws pictures. The key names the emulator
the entry needs rather than the one the session runs, and the launcher honours it: `kdos-term`
links the decoders and speaks sixel and the kitty protocol, so `yazi`'s previews are pictures
rather than a filename. It is a name and never a program — only an emulator this image ships is
accepted, and an unknown value falls back to the session's own, because an entry is a file anything
can write and a key naming a program would be a second `Exec` line with none of the field-code
rules. Without the key the session's terminal is used, which is lighter.

Check `Icon=` against the shipped atlas, which is narrower than the artwork. `genatlas.py`
takes six contexts — `places`, `devices`, `status`, `mimetypes`, `actions`, `emblems` — at four
sizes: 24, 32, 48 and 64. There is no `apps` context and no `panel` one: `panel/` is 2,344
third-party tray marks, and an application's own icon comes from hicolor at run time instead. A
name can therefore be present in `src/packages/kdos-icons/art` and
still be undrawable: `file-manager` is `panel/`-only and `utilities-terminal` is not there at all,
though both are what the freedesktop naming specification would have you write. After the atlas,
`libkicon` falls back to hicolor's `apps/` PNGs and to `pixmaps/`; an SVG there is never read,
because nothing in the session rasterises one. `testing/preflight.sh` resolves every shipped
entry's `Icon=` and `Exec=` by exactly those rules against `build/fs` and names the ones that miss.

No two visible entries may share a `Name=`. The Start menu, the launcher and the search all
list entries by name, so two rows both reading `Calendar` are two rows nobody can choose between.
The program filling a role — the file manager, the agenda — keeps the plain name, and every
alternative is qualified: `Files (lf)`, `Files (yazi)`, `Calendar (calcurse)`.
`testing/preflight.sh` refuses a collision.

Use `MimeType=` only where nothing else claims the type. Two entries claiming one type is how a
machine opens folders in whichever of them sorted first, which is not a decision anybody made.
`mimeapps.list` is where a default is chosen.

And only where the type exists. `MimeType=` names a type in the shared MIME database, and a
name with nothing behind it resolves to nothing and reports that nowhere. The opener chain keys off
`/usr/share/mime/globs`, which is generated from `/usr/share/mime/packages`. A port introducing a
type installs its own XML there, and `kpkg` regenerates the database — see
[Shared indexes](#shared-indexes). `frotz` is the worked example below: `shared-mime-info` 2.5.1
defines no z-machine type, so the port defines it.

`selftest.sh` reads these entries out of the heredoc. A missing `Terminal=true`, a
`Categories=` with no `Game` token, or an `Exec=` naming a path rather than a command fails at the
recipe rather than after a packaging run.

`Keywords=` is what the menu searches. A row nobody can find by the word they know it by is a
row that is not there.

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
`texinfo`, `go-md2man`, `lowdown` and `python3-sphinx` (`sphinx-build -b man`). A page that needs
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

`lowdown` stands in for `pandoc`: it reads the same `%` title block into `.TH`, and `-M key=value`
supplies what a pandoc invocation passes as `--variable` — `-M title=YQ -M section=1` for a page
with no title block, `-M source=v$version` where the title block carries an unexpanded
`$version` or a version older than the release. Render a new page once and read it with `mandoc -Tlint` before shipping it; a page
that only draws style warnings is fine. `lowdown` is built with `bmake`, because its makefile is
BSD make and GNU make stops at the first `.if`. `bmake` depends on `tzdata` because its install
runs its unit tests, and two of them convert a time in a named zone: without the zoneinfo database
they print UTC and the install fails.

`python3-sphinx` installs Sphinx, and the part of its closure that is not a port, under
`/usr/lib/python3-sphinx` rather than in `site-packages`: other ports vendor `requests` and
`urllib3` into `site-packages`, and two packages owning one path is a conflict. The closure carries
the default theme, `myst-parser` for Markdown sources and `sphinx-argparse`. The commands in
`/usr/bin` put that prefix on `PYTHONPATH` and run Sphinx, and they are the only way in:
`import sphinx` from a plain `python3` fails, so a build that probes for Sphinx as a module
instead of running `sphinx-build` does not find it.

A Sphinx recipe builds the man builder's output alone. Where a build system turns warnings into
errors behind an option (`SPHINX_WARNINGS_AS_ERRORS` in LLVM), turn it off: the build has no
network, so every intersphinx inventory fails to load and warns. Where a `conf.py` loads an
extension this tree does not carry and the pages do not use, run `sphinx-build` directly with
`-D extensions=<the list without it>`, which replaces the list `conf.py` sets (`khal`, `khard`).
Nothing covers a `conf.py` that refuses to load without an HTML theme, or a documentation switch
that builds the HTML manual and the pages together.

A version beside another version installs no pages the other one installs, or the two packages
conflict: `openssl3`, `lua54`, the `llvm21` slot and the cross toolchains ship none of the
native port's pages. `man-pages` supplies the kernel and C library sections (2, 3, 4, 5, 7) and
leaves out every page another port installs.

## Shipping a script the port carries

For a port that names a `source =`, the recipe hash covers `kpkgbuild`, `build.sh`,
`postinstall.sh` and `*.patch` — and nothing else in the port's directory. A helper script kept in
a file beside the recipe is therefore invisible to the hash: the port reports itself current after
every later edit, and the image keeps the copy it already had. Nothing fails; the machine
runs the old script.

(A source-less port is the other case. Its whole directory is hashed, because its own files *are*
its recipe. See [the packaging architecture](../03-architecture/packaging.md).)

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

The install-time hook, which becomes a marker inside the package. Seven ports have one:

- `avahi`, `networkmanager-openvpn`, `pcsc-lite`, `polkit`, `prosody` and `tcpdump` create their
  system accounts.
  `prosody` also gives its data directory to its account, and `networkmanager-openvpn` gives its
  chroot to its account.
- `linux` removes the module trees of other kernels, keeping the running kernel's when the root is
  `/`, and runs `depmod`.

Every hook works on `PKG_ROOT`, the root kpkgadd is installing into, never on `/`. `kpkg install
--root` and an A/B update both install into a tree that is not the running system, so a hook that
wrote `/etc/passwd` or ran `depmod` on `/` would change the wrong machine and leave the new root
without what it needs. Take the root as `"${PKG_ROOT:-/}"`, prefix it on every path, and hand it to
the tool: `groupadd -R`, `useradd -R`, `depmod -b`. `chown` resolves a name against the running
root's `/etc/passwd`, so read the ids out of `$PKG_ROOT/etc/passwd` and pass them as numbers. A hook
runs on every install and reinstall, so each step checks before it acts.

It runs once, while the package is installed into the image, so anything it writes is baked into
that image and is identical on every machine installed from it. Per-machine state therefore cannot
come from here; it is generated on first boot by the init script that needs it.

Reach for it only where the job must happen on the target with target binaries and belongs to
this one package. It is not a place to finish a build, and not a place to rebuild an index that
other packages also feed.

## Shared indexes

Some files do nothing until an index built from every package's copy is rebuilt. `kpkgadd` and
`kpkgdel` read the manifest they acted on and rebuild each index whose directory it touched, once,
from what is then on disk:

| A file under | Rebuilds |
|---|---|
| `/usr/share/glib-2.0/schemas/` | `glib-compile-schemas` |
| `/usr/lib/gio/modules/` | `gio-querymodules` |
| `/usr/lib/gdk-pixbuf-2.0/` | `gdk-pixbuf-query-loaders --update-cache` |
| `/usr/share/mime/packages/` | `update-mime-database` |
| `/usr/share/fonts/` | `fc-cache -s` |
| `/usr/share/info/` | the info `dir`, regenerated with `install-info` over every page |

A port therefore installs its schema, loader, MIME XML, font or info page and does nothing else.
A per-port hook would rebuild the index only when that port is installed, not when the next one
adds to it or the last one leaves.

A missing tool is skipped: the index is written when the package carrying the tool arrives,
because that package's own files touch the same directory. A failing tool is a warning, not a
failed install. `kpkgbuild` drops `usr/share/info/dir` from every package — it is the index, and
two packages each shipping one conflict. Under `--root`, each tool is handed the root-prefixed
directory; the pixbuf loader cache, which only writes the path it was compiled with, is rebuilt
only against `/`.

## Vendoring

`ports/fetch` runs the language's own vendoring tool inside a container and packages the result as
an archive beside the tarball. The build unpacks that archive and builds offline.

| `vendoring` | Produces | Build then |
|---|---|---|
| `rust` | The vendor tree and its configuration | `cargo build --frozen --offline` |
| `go` | The module vendor tree | `go build -mod=vendor` |
| `python` | The wheels or source distributions | Install from the local directory |

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

Each exists for a failure that has a name.

`pyrequirements = no` says a requirements file is not the dependency set. The filename is a
convention with no defined meaning, and projects routinely use it for the *optional* list — which
for one terminal application reached a scientific stack and a Fortran compiler, for a program whose
actual dependency is a single date library. With the key set, the source distribution's own
metadata is vendored, and the fetch says out loud that it skipped the file.

`pypackages` is an explicit closure, downloaded without resolving dependencies, because that key
*is* the closure the recipe wants. Letting the tool resolve from there drags in every dependency
that is already a port and builds each one's metadata to find that out.

`pyruntime = no` says a runtime environment must not be vendored.

A Python package's declared build backend is part of its version pin. Read the build-system
requirements before picking a version: a project that moved to a newer backend can cost several
additional ports.

### A vendor bundle hashes the same twice

Bundles are packed with the same flag set packages use — sorted, a pinned timestamp, fixed
ownership, single-threaded compression — because a plain archive records the extraction time and
the builder's identity, and the checksum beside a bundle would then be a hash of one particular
afternoon.

### A bundle no tool writes

`pdfium` carries a vendor bundle that `ports/fetch` cannot produce, because the dependencies it
holds are gclient checkouts and not a language's packages: Chromium's `//build`,
`third_party/abseil-cpp` and `generate_shim_headers.py`. Gitiles generates its archives on request
and no two downloads of one hash the same, so the bundle is the tree's copy and its `sha256 =`
line is its identity. Rebuild it by hand on a version bump: take each repository at the revision the
new branch's `DEPS` names (`build_revision`, `abseil_revision`), and the script from the matching
Chromium tag, lay them out under `vendor/` as they sit in a checkout, and pack them with the flag
set above plus `--mode=go-w`. The recipe's `_build_rev` and `_abseil_rev` name those revisions,
and `build.sh` refuses a tarball whose `DEPS` disagrees with them.

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
`lldb`, `libclc`, `libunwind`) carries that one tarball and configures its own directory with
`cmake -S <dir>`. The 21 slot uses the per-component tarballs, which were still published for 21.x.

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
`glib-$version.tar.xz::<url>`, with the same checksum, and LFS stores the one object for both
paths. Its `version` must equal the first port's: the typelib describes the library installed
beside it. Both recipes carry `group = glib`, so the version checker offers the two only as one
bump; `download.gnome.org` is no forge, and without the key each would be offered alone. Its meson
options are the first port's, except the one being turned
on.

A port that builds introspection data from a glib library names both `gobject-introspection` and
`glib-introspection` in `depends`: `g-ir-scanner` comes from the first, and every GIR it writes
includes `GLib-2.0`, `GObject-2.0` or `Gio-2.0` from the second. `libqrtr-glib`, `libmbim`,
`libqmi` and `modemmanager` do.

## What is built from source

Everything the host installs and runs on its own processor is compiled here from source; the
Debian packages in boxes are outside the rule. A recipe that installs a program, a library, a
module or a shared object it did not compile breaks the claim the whole tree makes: the binary cannot be read, cannot be rebuilt by `kdos rebuild`, and carries whatever its
builder put in it. Four classes of payload are exempt, and each is exempt for a reason the next
recipe has to be able to name.

| Class | What it covers | Why it cannot be compiled here |
|---|---|---|
| Code for another processor | `linux-firmware`, `intel-ucode`, `sof-firmware`; the closed EU kernels `intel-media-driver` compiles in with `ENABLE_KERNELS=ON` and `BUILD_KERNELS=OFF`; the SOF coefficient `.bin` files in `alsa-ucm-conf`; the flasher stubs and flash algorithms inside `espflash`, `probe-rs`, `python3-esptool` and `openfpgaloader`; the guest firmware images `qemu` installs from its `pc-bios/` | It runs on a DSP, a GPU, a microcontroller or a guest, not on the host, and for most of it no source is published |
| Compiled font data | `noto-fonts`, `noto-fonts-extra`, `noto-cjk`, `nerd-fonts-symbols`; the faces bundled inside `mupdf`, `matplotlib` and `seqkit` | Upstream publishes the built face, and the sources compile through a toolchain or a source tree this one does not carry. A face whose upstream build runs on ports is compiled: `ttf-dejavu`, `terminus-ttf` and `noto-emoji` |
| Compiler bootstrap seeds | The `rust` stage-0 `rustc`, `rust-std` and `cargo`; the `go` bootstrap toolchain; `zig`'s `stage1/zig1.wasm` | A compiler written in its own language needs a working one first. Each seed is used only to build, and never ships |
| Data with no other source form | The `tesseract` English model, the `perl-xml-parser` `.enc` encoding maps, the JavaScript in `libkiwix`'s skin, the `fcitx5-chinese-addons` pinyin and stroke tables, `john`'s `.chr` files, and recorded audio such as the `speaker-test` samples in `alsa-utils` | The file is the form upstream maintains; there is nothing earlier to build it from |

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

- **No rationale comments in `kpkgbuild`.** The banner header plus the metadata keys. Reasoning
  belongs in a commit message or in this book. `build.sh`, being a script, carries the comments a
  script carries.
- **No source edits with stream editors.** Use build flags. Patch only where there is genuinely no
  flag, and then ship a real `.patch` beside the recipe.
- **A library nobody links is a library the host does not have.** Several build systems answer a
  missing dependency by disabling the feature rather than failing, so options must be explicit
  rather than automatic and the `depends` line is load-bearing. Dropping one produces a build that
  succeeds and is silently narrower than its recipe claims.
- **Every `-D` must be an option the port defines.** `testing/preflight.sh` checks meson options
  against the tarball's own option file, and validates the two option types with a closed value
  set.
- **Quote a command a diagnostic names with single quotes.** A backtick inside double quotes is a
  command, not a name — an echo telling somebody to run something will run it.
- **Nothing may reach the network.** A meson subproject fallback, a CMake download call, or a
  Python build backend resolving a system tool from a package index are all the same bug. See
  [Build troubleshooting](build-troubleshooting.md#a-build-that-reaches-the-network).
- **A port's shipped configuration draws nothing outside the console font's set.** The font holds
  512 glyphs — a kernel limit, not a choice — and a Nerd Font icon is a private-use codepoint it
  cannot carry, so on `tty1` it renders as a blank cell: a name arrives with a hole punched in
  front of it and the listing reads as broken. Turn them off where the program has a switch
  (`yazi`'s `[icon]`, `starship`'s `format`, `eza --icons=never`), check the default before writing
  anything (`lazygit` 0.65's is already off), and where a program draws them with no way to be
  told, name it in [known gaps](../06-reference/known-gaps.md). The answer is never a patched
  console font.

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

The `::` form on the source is there because GitLab names a generated archive after the tag.

`build.sh` is the make-only shape, with two options chosen rather than defaulted:

```bash
export CFLAGS="$CFLAGS -Wno-error"
make curses PREFIX=/usr SOUND_TYPE=none
make install PREFIX=/usr SOUND_TYPE=none DESTDIR=$PKG
```

The curses interface and no other: the SDL one wants a window server and the dumb one has no screen
model. `SOUND_TYPE=none` keeps an audio stack off every image for the handful of stories that use
sound. `-Wno-error` is the answer where upstream's warnings meet this tree's `-Werror` and a patch
is not needed.

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

1. **Find the canonical upstream URL and the latest stable version.** Watch for projects whose
   releases are on a different host from their documentation, and for archives whose top-level
   directory is not `<name>-<version>` — verify with a listing before writing the recipe.
2. **Write `kpkgbuild`**, with the banner header, the keys, and any helpers between `release` and
   `source`.
3. **Fetch and record the checksum:**
   ```sh
   ports/fetch <port>          # `make fetch` takes no argument and walks all 879
   ```
4. **Write `build.sh`** from the canonical shape for its build system.
5. **Wire it in.** Add it to the `depends` of whatever needs it, and to the `packages.txt` of the
   phase it belongs in.
6. **Check the wiring**, in seconds rather than at the end of a build:
   ```sh
   testing/preflight.sh
   ```
7. **Build only that port:**
   ```sh
   make build BUILD_ARGS="--phases 04_phase4 --rebuild frotz"
   ```
8. **Read the log** at `build/logs/04_phase4/`, and consult
   [Build troubleshooting](build-troubleshooting.md) when it fails.

## Checking a recipe change

```sh
kpkg verify <port>            # build with the current recipe and with the .new one beside it
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

```sh
make updates                                   # the whole tree
make updates PORTUP_ARGS="--check curl"        # one port, non-interactive
make updates PORTUP_ARGS=--cve                 # cross-check vulnerabilities online
make updates PORTUP_ARGS="--jobs 1 --refresh"  # one check at a time, ignoring the cache
```

The version checker asks one question per port: does upstream have a release newer than the pin? It
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
sby's tags went from `yosys-0.47` to `v0.48` — and its newest release makes the answer unknown,
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
whose newest file is the pin is not proof of *current*: gnu-efi went to GitHub and libjpeg-turbo
to its own site with their feeds ending at `3.0.18` and `3.0.1`. When the feed ends at the pin, the
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

- **Class.** Dotted numbers of any length are one numbering — binutils went from 2.45.1 to 2.47 —
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
  23.1.2`. The slots (`llvm21`, `lua54`, `openssl3`, `docbook-xml`) carry it, and so does a
  hold a consumer forces (`python3-pydantic-core`, at the one version the pydantic vendored in
  `ocrmypdf` names).

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

The tool never runs version control on the tree. Accepting a bump rewrites a `version =` line and
re-fetches the archive; committing that stays a human decision.

## Committing sources

An archive `ports/fetch` downloaded is committed with the recipe that names it. The tarballs are in
the tree, tracked through Git LFS by the three `ports/core/**` patterns in `.gitattributes`:
`*.tar.*`, `*.tgz` and `*.zip`.

`ports/fetch` tries a mirror before upstream. `KDOS_SOURCES_BASE` names an append-only archive of
every tarball this tree has ever used, sharded by the filename's first character, so
`curl-8.21.0.tar.xz` is always under `sources-c` and a five-year-old checkout finds the exact
archive its recipe was written against. Set the variable empty to fetch from upstream alone.

```sh
ports/fetch <port>               # downloads and vendors, into ports/core/<port>/
git add ports/core/<port>
git lfs ls-files | grep <port>   # the archive must appear here
```

The archive must show in `git lfs ls-files`. One staged before `git lfs install` has run is an
ordinary blob and stays one until the history is rewritten — and an archive over 100 MB is then a
push github.com refuses, which is where the mistake first surfaces. Identical archives under
different ports share one entry, because LFS lists an object once however many paths point at it.

Two rules the tree keeps about what an archive is:

- **Nothing is committed that does not match its recipe.** The `sha256 =` line verifies the bytes,
  and `preflight.sh` checks that every recipe has one. An archive whose hash does not match its
  recipe fails the build at the port that unpacks it, hours in.
- **The hash is the identity; the URL is advisory.** With a hash, the local copy and upstream are
  interchangeable, and the local one is used. Without one — immediately after a version bump —
  upstream is the only source, because the tree cannot hold an archive that has never existed, and
  trusting a local file for an unverifiable one would be trusting the wrong thing entirely.

The version tool records the new checksum in the same operation as the version, for the archive and
the vendor bundle both, so the tree is never left with an archive nothing verifies.

## See also

- [Packaging](../03-architecture/packaging.md) — what a package is and how it is verified
- [Build troubleshooting](build-troubleshooting.md) — the recurring failures, by symptom
- [The build system](build-system.md) — how phases install what you wrote
- [Developing](developing.md) — the narrow rebuild loops
- [Testing](testing.md) — `preflight.sh` and what it checks about recipes
- [Decisions](../01-philosophy/decisions.md) — why a recipe is two files
- [Why KDOS](../01-philosophy/why-kdos.md#what-is-not-built-from-source) — every payload not built from source, port by port
