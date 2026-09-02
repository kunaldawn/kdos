# Writing ports

The recipe format in full, the canonical build shapes, vendoring for each language, and adding a
port from choosing a version to a green preflight. For what a port *is* and how packages are
built from one, see [Packaging](../03-architecture/packaging.md).

## A port is two files

```
ports/core/foo/
├── kpkgbuild          declarative metadata — PARSED, never sourced
├── build.sh           the build — ordinary bash
├── postinstall.sh     optional install-time hook
├── *.patch            optional
└── foo-1.2.3.tar.gz   the source, a release asset, gitignored
```

**`kpkgbuild` has no interpreter line and is never executed.** Reading a recipe therefore costs no
shell and cannot run anything.

**`build.sh` is a real script**, so syntax checking, linting, highlighting and diffing all work on
it — and `testing/preflight.sh` syntax-checks every one of them, which is impossible when the build
lives inside a config format.

The argument for the split is in [Decisions](../01-philosophy/decisions.md).

## kpkgbuild

```
# (banner header — keep verbatim)
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ...

name        = foo
version     = 1.2.3
release     = 1
source      = https://upstream.example/$name-$version.tar.gz
sha256      = 9a93b2b7…  foo-1.2.3.tar.gz
description = <one line>
homepage    = <URL>
depends     = zlib openssl
```

### Every key

| Key | Required | Means |
|---|---|---|
| `name` | yes | The package name |
| `version` | yes | Upstream's version |
| `release` | yes | Bump to force a rebuild for a reason the recipe hash cannot see |
| `source` | | An upstream URL. **Repeats** for additional sources |
| `sha256` | | `<hash>  <filename>`, one per source. **Repeats** |
| `description` | | One line. It is *read* and printed — not a comment |
| `homepage` | | |
| `depends` | | Space-separated port names |
| `vendoring` | | `rust`, `go` or `python` — see [Vendoring](#vendoring) |
| `vendordir` | | Where the vendoring tool must **run**, when that is not the top of the tree |
| `pypackages` | | An explicit Python dependency closure to vendor |
| `pyrequirements` | | `no` — a requirements file is **not** the dependency set here |
| `pyruntime` | | `no` — do not vendor a runtime environment |
| `secdb` | | The name the security database uses, when it differs from ours |
| `bench` | | A command `kdos march` times |
| `bench_setup` | | A command that runs once and is **not** timed |
| `group` | | Override the derived version-check grouping |

`description`, `homepage` and `depends` are **keys, not comments**. An older comment form is still
read as a fallback.

### Recipe helpers

**Any key that is not one of ours is a helper** — `_tag`, `_commit`, `_date`, `debrev`, `vrsn` and
the rest.

**Declare them between `release` and `source`.** That is the only order that works: a helper may
read `$version`, and `source` may read the helper.

Values expand:

```
$var   ${var}   ${var#p}   ${var##p}   ${var%p}   ${var%%p}
${var/a/b}      ${var//a/b}            ${var:off:len}
```

**Command substitution is not available.** Where upstream's convention needs a transformation, use
the pattern operators — a version with its dots removed is `${version//./}` rather than a pipeline.

### Sources

`source` repeats to add more. The extension is detected automatically, and
**`--strip-components=1` is passed for the first source only**.

**A non-archive source is not unpacked** — it stays in the port directory and the build reads it
from there. That is how a port carries a data file or a patch fetched from elsewhere.

**A release archive carries a submodule's directory empty.** If upstream's build expects a
submodule, a release tarball will not have it: add it as a second `source` extracted where the
build looks, or make it a port.

## build.sh

The working directory is the unpacked source. Available as shell **variables** — not exports:

| Variable | Is |
|---|---|
| `$name`, `$version`, `$release` | From the recipe |
| Every helper | From the recipe |
| `$PORT_SRC` | The port's own directory |
| `$SRC` | The unpacked source, and the working directory |
| `$SRC_ROOT` | The parent of `$SRC` |
| `$PKG` | The staging tree — install here |

```bash
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
```

## Canonical shapes

Copy these rather than inventing.

### meson

```bash
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
      --buildtype=release -Dtests=disabled -Ddocs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
```

**Every meson setup needs `--prefix=/usr --libdir=lib`.** The default library directory is not on
the runtime linker's search path, and the symptom is a shared library that cannot be loaded.

**Check the option names against the tarball's own option file.** meson **fails at setup** on an
unknown option, before a line is compiled, and there is no universal spelling — one project's
disable flag is fatal in the next. The built-in options are always valid.

### cmake

```bash
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF
```

**A misspelt option is a warning, not an error** — the opposite of meson. Read the warning about
unused variables in the log rather than trusting the exit status, and take option names from the
project's own option declarations.

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

### make-only

```bash
export CFLAGS="$CFLAGS -Wno-error"
make PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr install
```

**Export the compiler flags; never pass them as a make argument.** A variable on the make command
line beats both the environment *and* the makefile's own assignment — which is the wrong end of
that precedence for flags, because a makefile's own definitions are its **configuration**. Passing
them as arguments discards those, and the build then fails somewhere else entirely, on an
undeclared constant that reads as a missing header.

## A desktop entry belongs to the port, not to `fs/`

A program that draws in a terminal is an application on this desktop, and almost none of them ship
a `.desktop` file — upstream writes one for a GUI or writes none at all. **Write it in `build.sh`,
into `$PKG/usr/share/applications/`.** A package owns its entry, so installing the port adds the
menu row and removing the port takes it away; the same file under `fs/` is owned by nothing and
outlives the program it names.

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

Four rules, each with a consequence:

- **`Terminal=true` and a bare `Exec`.** Naming an emulator in `Exec` pins the entry to one
  desktop: `foot` is a Wayland client and cannot run on the console. The launcher picks the
  emulator and supplies the identity — see [`kdos-shell`](../04-programs/kdos-shell.md).
- **Check `Icon=` against the shipped atlas**, `src/packages/kdos-icons/art`. The set is
  Papirus-derived and does not carry the freedesktop names you would guess: there is `file-manager`
  but no `system-file-manager`, `help-contents` but no `help-browser`. A name that misses still
  draws — the glyph tier is underneath — so this is polish, not correctness.
- **`MimeType=` only where nothing else claims the type.** Two entries claiming one type is how a
  machine opens folders in whichever of them sorted first, which is not a decision anybody made.
  `mimeapps.list` is where a default is chosen.
- **`Keywords=` is what the menu searches.** A row nobody can find by the word they know it by is
  a row that is not there.

## postinstall.sh

The install-time hook, becoming a marker inside the package. Six ports have one.

Reach for it only where the job **must** happen on the target with target binaries — compiling a
database that ships as source, or registering something in a runtime index. It is not a place to
finish a build.

## Vendoring

`ports/fetch` runs the language's own vendoring tool inside a container and packages the result as
an archive beside the tarball. The build unpacks it and builds offline.

| `vendoring` | Produces | Build then |
|---|---|---|
| `rust` | The vendor tree and its configuration | `cargo build --frozen --offline` |
| `go` | The module vendor tree | `go build -mod=vendor` |
| `python` | The wheels or sdists | Install from the local directory |

### Where the bundle goes

**Unpack it where the tool will be standing, not beside the manifest.** The package manager finds
its configuration by walking up from the **current directory**, so a build that invokes it from a
subdirectory never reads a configuration next to the manifest — and every crate in the bundle
resolves as *missing* while sitting in the vendor directory the whole time.

**`vendordir` is the other half**: it says where the vendoring tool must **run**, which is beside
the manifest. The two directories are not always the same place.

### Python has three extra keys, and they exist for real failures

- **`pyrequirements = no` says a requirements file is not the dependency set.** The name is a
  convention with no defined meaning, and projects routinely use it for the *optional* list — which
  for one terminal application reached a scientific stack and a Fortran compiler, for a program
  whose actual dependency list is a single date library. With the key set, the source
  distribution's own metadata is vendored, and the fetch **says** it skipped the file rather than
  going quiet.
- **`pypackages`** is an explicit closure, downloaded **without** resolving dependencies — because
  that key *is* the closure the recipe wants. Letting the tool resolve from there drags in every
  dependency that is already a port and builds each one's metadata to find that out.
- **`pyruntime = no`** where a runtime environment must not be vendored.

**A Python package's declared build backend is part of its version pin.** Read the build-system
requirements before picking a version: a project that moved to a newer backend can cost several
additional ports.

### A vendor bundle is an artefact and hashes the same twice

It is packed with the same flag set packages use — sorted, a pinned timestamp, fixed ownership,
single-threaded compression — because a plain archive records the extraction time and the builder's
identity, and the checksum beside a bundle would then be a hash of one particular afternoon.

### A Rust port's version is pinned by this tree's compiler

The package manager **refuses** a crate whose declared minimum version is higher, rather than
degrading. Pin the port to the newest release that builds and say so in the recipe, because bumping
one means bumping the toolchain, the fetch container and every vendored bundle together — a wave,
not a version bump.

**The declared minimum is not an oracle.** It gates the refusal; it says nothing about what the
code actually uses. A release declaring an older minimum can still fail on a feature stabilised
later. The only reliable test is compiling.

## Rules a recipe must keep

- **No rationale comments.** The banner header plus the metadata keys. Reasoning belongs in a
  commit message or in this documentation.
- **No source edits with stream editors.** Use build flags. Patch only when there is genuinely no
  flag, and then ship a real patch file beside the recipe.
- **A library nobody links is a library the host does not have.** Several build systems answer a
  missing dependency by **disabling the feature** rather than failing, so the options must be
  explicit rather than automatic and the `depends` line is load-bearing. Dropping one produces a
  build that succeeds and is silently narrower than its recipe claims.
- **Every `-D` must be an option the port defines.** `testing/preflight.sh` checks meson options
  against the tarball's own option file, and checks the two option types with a closed value set.
- **Quote a command a diagnostic names with single quotes.** A backtick inside double quotes is a
  command, not a name — an echo telling somebody to run something will **run it**.
- **Nothing may reach the network.** A subproject fallback, a download call, or a build backend
  resolving a system tool from a package index are all the same bug. See
  [Build troubleshooting](build-troubleshooting.md).

## Adding a port end to end

1. **Find the canonical upstream URL and the latest stable version.** Watch for projects whose
   releases are on a different host from their documentation, and for archives whose top-level
   directory is not `<name>-<version>` — verify with a listing before writing the recipe.
2. **Write `kpkgbuild`**, with the banner header, the keys, and any helpers between `release` and
   `source`.
3. **Fetch and record the checksum**:
   ```sh
   make fetch
   ```
4. **Write `build.sh`** from the canonical shape for its build system.
5. **Wire it in**: add it to the `depends` of whatever needs it, and to the `packages.txt` of the
   phase it belongs in.
6. **Check the wiring**, in seconds rather than at the end of a build:
   ```sh
   testing/preflight.sh
   ```
7. **Build just that port**:
   ```sh
   make build BUILD_ARGS="--phases 04_phase4 --rebuild foo"
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

**It compares payload, not the file list.** A file list answers "are the same files there" and is
silent about the same paths with different contents — which is the error that starts mattering the
moment recipes change. The comparison is a per-member fingerprint including modes and link targets,
and it prints the first differing lines.

`--repro` is the acceptance test for reproducible packaging, and it is the same code path because
it is the same question asked of two archives.

## Checking for new versions

```sh
make updates                                   # the whole tree
make updates PORTUP_ARGS="--check curl"        # one port, non-interactive
make updates PORTUP_ARGS=--cve                 # cross-check vulnerabilities online
```

The version checker asks one question per port: *does upstream have a release newer than the pin?*

**Six steps**, and the constraint every step above serves:

1. List upstream releases — a forge's tag feed, a directory listing, or a package-tracking service,
   first non-empty wins.
2. Extract version candidates from every raw string.
3. Keep the ones whose **shape** matches the current version's.
4. Compare, walking from the highest match down.
5. **Render the candidate through the real recipe parser** — the recipe is copied to a temporary
   directory, the version substituted, and the metadata expanded by the same parser the build uses.
   So a helper chain falls out for free and no probe ever touches the real tree.
6. **Request the rendered URL.** A not-found drops that candidate and tries the next-highest.

**Correctness comes from that last request, not from the discovery step.** A feed can name a
version whose archive lives somewhere the recipe's template does not expect, and the tool would
rather try the next candidate than report a version it never confirmed the build could fetch.

**Three outcomes, never two.** *Unknown* is never folded into *current* — that would be a confident
wrong answer, the one thing this must not give. A listing that could not be reached, one whose tail
was cut off by a cap (archive indexes sort ascending, so the dropped entries are the newest), and
one whose candidates all failed to resolve are each **unknown, with a reason**.

**Forge tag feeds need no authentication** and have no rate limit worth worrying about, unlike the
equivalent programmatic interfaces. The package-tracking service is the fallback of last resort,
rate-limited and marked low-confidence — it is never upstream itself.

**The grouping key is the forge organisation plus the current version**, not a name prefix. A name
rule misses the member of a release family whose repository is named differently, while all of them
resolve to the same organisation and version and are correctly offered as a bump together. A
`group` key overrides the derived one.

**Exit codes**: 0 means every named port is current, 1 means at least one has an update, 2 means a
bump was accepted but its archive never made it to disk — the one state this tool exists to prevent
a build from inheriting silently.

The tool **never runs version control**. Accepting a bump rewrites a `version =` line and re-fetches
the archive; committing that is still a human decision.

## Publishing sources

```sh
make publish-sources
```

Archives are **release assets, sharded by first letter**, with git holding only the checksums that
identify them. Two guarantees:

- **Append-only.** An asset is never deleted and never replaced, because replacing one silently
  changes what an old commit builds. The publisher skips one that is already there rather than
  overwriting.
- **The hash is the identity; the URL is advisory.** With a hash, our archive and upstream are
  interchangeable and ours is tried first. **Without one — you have just bumped the version —
  upstream is the only source**, because our archive cannot hold an archive that has never existed,
  and reaching for our own release for an unverifiable file would be trusting the wrong thing
  entirely.

The version tool records the new checksum in the same operation as the version, for the archive
**and** the vendor bundle, so the tree is never left with an archive nothing verifies.

## See also

- [Packaging](../03-architecture/packaging.md) — what a package is and how it is verified
- [Build troubleshooting](build-troubleshooting.md) — the recurring failures, by symptom
- [The build system](build-system.md) — how phases install what you wrote
- [Developing](developing.md) — the narrow rebuild loops
- [Testing](testing.md) — `preflight.sh` and what it checks about recipes
