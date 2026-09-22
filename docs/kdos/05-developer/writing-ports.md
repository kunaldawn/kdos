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
├── frotz-2.55.tar.gz          the source, tracked in Git LFS
└── Gnu_in_the_Zoo.zblorb      a second source
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
_story      = Gnu_in_the_Zoo.zblorb
source      = $name-$version.tar.gz::https://gitlab.com/DavidGriffith/frotz/-/archive/$version/frotz-$version.tar.gz
source      = https://ifarchive.org/if-archive/games/zcode/$_story
sha256      = a8c4c4d7…  frotz-2.55.tar.gz
sha256      = d0854f37…  Gnu_in_the_Zoo.zblorb
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
| `version` | yes | | Upstream's version |
| `release` | yes | | Bump to force a rebuild for a reason the recipe hash cannot see |
| `source` | | yes | An upstream URL, or `filename::url` |
| `sha256` | | yes | `<64 hex>  <filename>`, one per source |
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
| `pyrequirements` | `ports/fetch` | `no` — a requirements file is *not* the dependency set here |
| `pyruntime` | `ports/fetch` | `no` — do not vendor a runtime environment |
| `group` | `ports/update` | Override the derived version-check grouping |

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
type installs its own XML there *and* carries a `postinstall.sh` running `update-mime-database
/usr/share/mime`, because the database is compiled on the target and `build.sh` cannot do it.
`frotz` is the worked example below: `shared-mime-info` 1.10 defines no z-machine type, so the port
defines it.

`selftest.sh` reads these entries out of the heredoc. A missing `Terminal=true`, a
`Categories=` with no `Game` token, or an `Exec=` naming a path rather than a command fails at the
recipe rather than after a packaging run.

`Keywords=` is what the menu searches. A row nobody can find by the word they know it by is a
row that is not there.

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

The install-time hook, which becomes a marker inside the package. Seven ports have one: `avahi`,
`dbus`, `frotz`, `glib`, `linux`, `polkit` and `shared-mime-info`.

Reach for it only where the job must happen on the target with target binaries — compiling a
database that ships as source, or registering something in a runtime index. It is not a place to
finish a build.

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

### A Rust port's version is pinned by this tree's compiler

Cargo refuses a crate whose declared minimum Rust version is higher than the toolchain, rather than
degrading. Pin the port to the newest release that builds, because bumping one means bumping the
toolchain, the fetch container and every vendored bundle together — a wave, not a version bump.

The declared minimum is not an oracle. It gates the refusal; it says nothing about what the code
actually uses, so a release declaring an older minimum can still fail on a feature stabilised
later. The only reliable test is compiling.

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
  anything (`lazygit` 0.61's is already off), and where a program draws them with no way to be
  told, name it in [known gaps](../06-reference/known-gaps.md). The answer is never a patched
  console font.

## Worked example: frotz

`ports/core/frotz` exercises most of the format in one recipe. It carries two sources, renames the
first, ships a desktop entry, defines a MIME type, and needs an install-time hook.

The metadata declares both sources and a helper naming the second:

```
name        = frotz
version     = 2.55
release     = 1
_story      = Gnu_in_the_Zoo.zblorb
source      = $name-$version.tar.gz::https://gitlab.com/DavidGriffith/frotz/-/archive/$version/frotz-$version.tar.gz
source      = https://ifarchive.org/if-archive/games/zcode/$_story
sha256      = a8c4c4d7…  frotz-2.55.tar.gz
sha256      = d0854f37…  Gnu_in_the_Zoo.zblorb
description = Z-machine interpreter — Infocom-era interactive fiction in a terminal
homepage    = https://661.org/proj/if/frotz/
depends     = ncurses pkgconf
```

`_story` sits between `release` and `source` because the second `source` line reads it. The `::`
form on the first source is there because GitLab names a generated archive after the tag.

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

The story file is the second source, so it is already in `$SRC` under its own name and the recipe
installs it by that name — plus the MIT notice, which the licence requires to travel with the work:

```bash
install -Dm644 "$_story" "$PKG/usr/share/$name/$_story"
install -Dm644 /dev/stdin "$PKG/usr/share/licenses/$name/$_story.MIT" <<'LICENCE'
…
LICENCE
```

A program that opens nothing is one nobody opens twice: `frotz` with no story prints usage and
exits, so the desktop entry names the story the package carries. Because that entry claims two MIME
types, the port has to define them first — `shared-mime-info` 1.10 has neither:

```bash
install -Dm644 /dev/stdin \
	"$PKG/usr/share/mime/packages/kdos-zmachine.xml" <<'MIMEXML'
…
MIMEXML
```

and `postinstall.sh` runs `update-mime-database /usr/share/mime` on the target, which is the one
job `build.sh` cannot do.

## Adding a port, end to end

1. **Find the canonical upstream URL and the latest stable version.** Watch for projects whose
   releases are on a different host from their documentation, and for archives whose top-level
   directory is not `<name>-<version>` — verify with a listing before writing the recipe.
2. **Write `kpkgbuild`**, with the banner header, the keys, and any helpers between `release` and
   `source`.
3. **Fetch and record the checksum:**
   ```sh
   ports/fetch <port>          # `make fetch` takes no argument and walks all 853
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
```

The version checker asks one question per port: does upstream have a release newer than the pin? It
answers in six steps.

1. List upstream releases — a forge's tag feed, a directory listing, or a package-tracking service;
   first non-empty wins.
2. Extract version candidates from every raw string.
3. Keep the ones whose shape matches the current version's.
4. Compare, walking from the highest match down.
5. Render the candidate through the real recipe parser. The recipe is copied to a temporary
   directory, the version substituted, and the metadata expanded by the same parser the build uses,
   so a helper chain falls out for free and no probe ever touches the real tree.
6. Request the rendered URL. A not-found drops that candidate and tries the next-highest.

Correctness comes from that last request, not from the discovery step. A feed can name a version
whose archive lives somewhere the recipe's template does not expect, and the tool would rather try
the next candidate than report a version it never confirmed the build could fetch.

There are three outcomes, never two. *Unknown* is never folded into *current*, because that would
be a confident wrong answer — the one thing this tool must not give. A listing that could not be
reached, one whose tail was cut off by a cap (archive indexes sort ascending, so the dropped
entries are the newest), and one whose candidates all failed to resolve are each unknown, with a
reason.

Forge tag feeds need no authentication and have no rate limit worth worrying about, unlike the
equivalent programmatic interfaces. The package-tracking service is the fallback of last resort,
rate-limited and marked low-confidence; it is never upstream itself.

The grouping key is the forge organisation plus the current version, not a name prefix. A name rule
misses the member of a release family whose repository is named differently, while all of them
resolve to the same organisation and version and are correctly offered as a bump together. A
`group` key overrides the derived one.

Exit codes: 0 means every named port is current, 1 means `--check` found at least one update, and 2
means a bump was accepted but its archive never made it to disk — the one state this tool exists to keep a
build from inheriting silently.

The tool never runs version control. Accepting a bump rewrites a `version =` line and re-fetches
the archive; committing that stays a human decision.

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
