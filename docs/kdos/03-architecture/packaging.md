# Packaging

This chapter describes how software on the KDOS host is organised, fetched, built, installed,
verified and updated. It is written for administrators who want to know what `kpkg` does to their
machine, and for contributors who are about to add or change a port and want to understand the
machinery around the recipe. It assumes the overview in
[Architecture overview](overview.md); the recipe format itself, key by key, is in
[Writing ports](../05-developer/writing-ports.md), and the end-to-end story of a build, from a
fresh clone to a bootable image, is in [How KDOS is built](../05-developer/how-kdos-is-built.md).
How this packaging compares with other distributions' is in
[How KDOS differs](../01-philosophy/how-kdos-differs.md#packages-and-recipes).

It starts with the ports tree and how its recipes are grouped into build phases, then follows a
source archive from the network to the port directory, through the package manager `kpkg`, into an
installed package. It then covers what a build checks, how the build decides what to rebuild, how
packages are made reproducible, the signed binary host, binary deltas, updating a running machine,
and vulnerability tracking.

The natively ported graphical applications, Chromium and LibreOffice among them, are ports like any
other and are packaged this way. The applications in the box catalogue are not: they ship as
read-only images run in a sandbox, a separate system described in
[Packs and boxes](packs-and-boxes.md).

## The ports tree

A **port** is a directory that describes how to build one piece of software. A **recipe** is the
pair of files that make up that description, and a **package** is what building a port produces:
an archive that `kpkg` installs and records. Every program on the KDOS host, the compiler and the
kernel included, is built from a port.

The build that turns ports into a system runs in numbered **phases**, from `00_cross` to
`70_image`. From `20_selfhost` onwards each runs inside the build
[chroot](../06-reference/glossary.md): the directory tree holding the system built so far, which
the build enters as its root. The phases are introduced under
[Phases, package lists and shelves](#phases-package-lists-and-shelves), and run as described in
[The build system](../05-developer/build-system.md#phases).

### What a port holds

A port holds two files:

| File | What it is |
|---|---|
| `kpkgbuild` | Declarative metadata: name, version, release, sources, hashes, dependencies. Parsed, never run as a script |
| `build.sh` | The build. Ordinary bash, run with the working directory set to the unpacked source |

A port may also carry a `postinstall.sh` hook, `.patch` files, and small files committed beside
the recipe: upstream patch-level files such as `bash53-001`, data files such as the IANA registries,
and local files such as the kernel configuration `linux/kdos.config`. The upstream archives and
vendor bundles a recipe names sit beside it once `make fetch` has run, but git does not track them;
see [Where sources come from](#where-sources-come-from).

The split into two files is deliberate. Because the metadata is parsed rather than executed,
reading a recipe runs no shell, so querying a thousand recipes is as cheap and as safe as reading a
thousand configuration files. Because the build is a real script, syntax checking, `shellcheck`,
highlighting and diffing all work on it, and no parser has to tell a heredoc in the build apart
from the metadata's own syntax. [Decisions](../01-philosophy/decisions.md) gives the reasoning in
full.

A recipe's `depends =` line is the only dependency list. It names every port that must be
installed before this one builds, and serves as the runtime dependency list as well; there is no
separate list of build-only dependencies.

### Repositories and shelves

There are five port repositories, all in the same format, searched in this order:

| Repository (inside the build chroot) | In the tree | Recipes | What it holds |
|---|---|---|---|
| `/ports/core` | `ports/core/` | 1,999 | Upstream software, filed on 102 shelves |
| `/kdos/src/system` | `src/system/` | 5 | KDOS's own `kdos` command and tools, the packer, the box runtime and its init, and the installer |
| `/kdos/src/art` | `src/art/` | 6 | KDOS's own theme generator, icon, cursor and GTK themes, boot splash and demo |
| `/kdos/src/desktop` | `src/desktop/` | 8 | KDOS's own compositor, panel, terminal, lock screen, resource monitor, box socket, recorder and portal |
| `/kdos/src/daemons` | `src/daemons/` | 5 | KDOS's own root daemons |

That is 2,023 recipes in all (counted as directories holding a `kpkgbuild`).

A repository holds a port either directly, as `<repo>/<name>/`, or one level down, as
`<repo>/<shelf>/<name>/`. The `src/` areas hold theirs directly. `ports/core` holds none directly:
every upstream port sits on a **shelf**, a directory named for its subject, such as
`ports/core/wl/wlroots/`, `ports/core/fonts/noto-fonts/` or
`ports/core/python-net/python3-requests/`. The shelves are a closed list, the file `ports/shelves`,
with one line per shelf giving its id and what belongs on it. [Writing
ports](../05-developer/writing-ports.md) gives the rules for choosing one.

A shelf is only where a recipe is filed. A port's identity is its bare name: `depends =` lines,
package lists, the package database, the binary host and the source index all name `wlroots`, never
`wl/wlroots`, and there is no recipe key for the shelf, so moving a port to another shelf changes
nothing but its path (and the `.gitignore` line of the few ports ignored by path, which spells the
shelf). So a name must be unique across the whole tree: `kpkg` looks a name up in
each repository, first as `<repo>/<name>/` and then on every shelf, and a name found at two paths
inside one repository is an error that names both, never a choice between them. `kpkg`,
`kdos update`, `kdos cve` and the version checker all stop on it. A port nested below its shelf is
an error too. Across repositories the first in the search order wins a name. `testing/preflight.sh`
refuses a tree in which any port sits anywhere but `ports/core/<shelf>/<name>/` or
`src/<area>/<name>/`, a shelf is missing from `ports/shelves`, or a name is filed twice, and the
pre-push hook makes the same checks of `ports/core` at the tip of every ref it pushes.

`PORT_REPO` lists the repositories `kpkg` may resolve against, in order. It holds at most eight;
an entry past the eighth is dropped with a warning. Shelves are never listed on it: `kpkg` finds
them itself. Its default, from `/etc/kpkg.conf`, is `/ports/core` alone, and `20_selfhost`,
`30_foundation`, `31_compilers` and `70_image` use that default (`00_cross` and `10_bootstrap` do not use `PORT_REPO`: their
step scripts read recipes through `script/lib/port.sh`, which finds a name one shelf down in
`ports/core` itself). The package-building phases from `40_lang` to `60_kernel` widen it in their
environment files, `script/phases/<phase>/phase.env`:

| Phases | `PORT_REPO` |
|---|---|
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

Building KDOS's own programs is therefore not a special case anywhere in the build system: they
are ports like any other, found on a longer search path.

Beside `ports/core`, the `ports/` directory holds the shelf list `shelves` and the tools that
operate on the tree rather than recipes: `fetch` and `publish` for sources, `srclib.sh` (the
library both share), `update` for upstream version checks, `hackage-vendor` for Haskell vendor
bundles, `Containerfile.fetch` for the container that generates vendor bundles, and the source
index `sources.idx`. Each of these tools names a port by its bare name and finds it on any shelf.

### The recipes under src/

The four `src/` areas that hold recipes hold KDOS's own software. Each directory there is a port
whose recipe names no `source =`: the code lives in the port directory itself, and `build.sh`
compiles it from `$PORT_SRC` (the port directory; see
[What a build verifies](#what-a-build-verifies)), usually together with some of the shared
libraries under `src/libs` (see [The C libraries](../05-developer/c-libraries.md)). Two of those
recipes carry an empty `source =` line, which means the same thing.

Every port there sits exactly two levels below `src/`, at `src/<area>/<name>/`. That depth is load
bearing: a recipe finds the libraries at `$PORT_SRC/../../libs`, and `kdos-installer` compiles a
file of its sibling `kdos-appbox`. The areas divide the programs by what they are:

| Area | Holds |
|---|---|
| `src/desktop` | Programs that draw the session or serve it over Wayland or D-Bus |
| `src/daemons` | Root daemons whose client is the desktop account |
| `src/system` | The package manager, the `kdos` command and its services, packs and boxes, and the installer |
| `src/art` | Themes, pictures and the programs that generate them |
| `src/libs` | The shared `libk*` C libraries; no recipes |
| `src/devtools` | The build orchestrator `kdosbuild` and the version checker `kdos-portup` behind `ports/update`; no recipes. They run on the build machine and are compiled on demand |

Two directories under `src/system` are special:

- `kdos-kpkg` is the package manager's own source and has no recipe. The bootstrap step
  `script/phases/10_bootstrap/120_kpkg.sh` cross-compiles it, because nothing can read a recipe
  before `kpkg` exists. It is therefore not an installed package and is not in the package
  database.
- `kdos-installer` has a recipe but is built by the bootstrap step `130_kinstall.sh`, from the
  same sources, rather than from a package list.

A source-less port is hashed differently from an upstream one when the build decides whether it is
current: its whole directory, and all of `src/libs`, count as its recipe. The two source-less ports
in `ports/core`, `containers-common` and `musl-ldd`, hash their whole directory but not
`src/libs`. See
[`E:` — the recipe hash](#e--the-recipe-hash).

### Phases, package lists and shelves

The ports tree says how to build each piece of software; it does not say which pieces make up the
system or in what order they are built. That is the job of the **phase package lists**, one per
package-building phase directory under `script/phases/`. A list is either one file,
`packages.txt`, or a directory of files, `packages.d/`, whose `*.txt` files are read in byte order
as one list. A phase has one or the other, never both.

| Phase | List | Names | Installs |
|---|---|---|---|
| `20_selfhost` | `packages.txt` | 8 | 14 |
| `30_foundation` | `packages.txt` | 125 | 115 |
| `31_compilers` | `packages.txt` | 22 | 22 |
| `40_lang` | `packages.d/`, 14 files | 195 | 167 |
| `41_system` | `packages.d/`, 94 files | 967 | 967 |
| `42_graphics` | `packages.d/`, 54 files | 186 | 186 |
| `43_toolkits` | `packages.d/`, 46 files | 242 | 242 |
| `44_apps` | `packages.d/`, 55 files | 280 | 280 |
| `50_desktop` | `packages.txt` | 22 | 22 |
| `60_kernel` | `packages.txt` | 2 | 2 |

"Names" counts the port names a list writes; "Installs" counts the packages the phase installs
that no earlier phase did. Between them the lists name 2,013 distinct ports and install 2,017:
1,994 of the 1,999 in `ports/core`, and every recipe under `src/` except `kdos-installer`, which
the bootstrap builds by name. The 5 `ports/core` recipes nothing reaches (`helix`,
`icon-naming-utils`, `musl-locales`, `perl-xml-simple` and `setconf`) are built only on request.

Where a list names more ports than it installs, the extra names are ports an earlier phase already
installed, named again so that one whose recipe changed is rebuilt at that point in the order. A
name may be written again only in the list's *order run*: `packages.d/00-order.txt`, or the names
a `packages.txt` gives ahead of its first shelf heading. `30_foundation`'s order run is the build
tools recipes use without naming them in `depends =` (`make`, `pkgconf`, the autotools, `cmake`,
`meson`, `ninja`, `python3`, `perl`, `bash` and `toybox`), led by the toolchain `20_selfhost`
built (`musl`, `gcc`, `binutils`) and followed by the rest of `20_selfhost`'s ports, which
reinstall after toybox to take back the names it leaves out. `40_lang` opens with `toybox` and
those owners again (see [toybox and the tools it overlaps](#toybox-and-the-tools-it-overlaps)).

**From `30_foundation` on, a list names exactly the ports its phase installs.** Each port's
`depends =` still pulls in its dependencies, but every dependency the phase needs is either
installed by an earlier phase or named in the list itself. So a phase means what it says: a port
cannot drift into an earlier phase because something there started to depend on it, and a list
cannot reach forward into a later one. `testing/phaseclosure.py`, which preflight runs, checks this
for every phase and names each port that breaks it, with the phase that installs it and the phase
that names it. It also refuses a name that is not a port on the phase's `PORT_REPO`, a name listed
twice, and a dependency that only a later phase's `PORT_REPO` can resolve.

The phases split the system by what a port's dependency closure reaches, not by subject:

| Phase | Holds |
|---|---|
| `20_selfhost` | The C library, the compilers and the tools their builds run, rebuilt inside the chroot |
| `30_foundation` | Build systems, the Perl and Python interpreters, and the base libraries every later recipe builds with |
| `31_compilers` | LLVM and clang with their runtimes, Rust, Go, GHC, Zig, Node.js and Ruby, and the ports whose closure reaches one of them, such as `cargo-c`, `bindgen` and `pandoc`. They take most of the hours before `40_lang`, so `30_foundation` is a restore point in front of them |
| `40_lang` | Language modules and developer tools whose closure needs nothing past `31_compilers` |
| `41_system` | Everything whose closure reaches no graphics and no toolkit: services, networking, storage, the command line, codecs, and the science and hardware libraries |
| `42_graphics` | Ports whose closure reaches Wayland, X11, Mesa, cairo, pango, GStreamer, FFmpeg or PipeWire but no toolkit, other than ports nothing depends on that sit on an application shelf |
| `43_toolkits` | GTK, Qt, KDE Frameworks, wxWidgets, FLTK and Motif, and every library over them that something else depends on |
| `44_apps` | Ports nothing depends on whose closure reaches a toolkit, and those on an application shelf whose closure reaches the graphics stack. The application shelves are listed in [Which phase lists a port](../05-developer/writing-ports.md#which-phase-lists-a-port) |
| `50_desktop` | KDOS's own desktop and daemons, with wlroots, the input method and the portal backends |
| `60_kernel` | The kernel, `linux`, and `dwarves` for its BTF |

A list is a plain file: one port name per line, with `#` comments. After the file's banner, which
holds the KDOS name, the phase directory and its title, such as "41_system: System", comments do two
jobs. The first is grouping, and **the lists are grouped by shelf**. In a `packages.d/` directory
each file is one shelf's ports and is named after it, `<shelf>.txt`, such as
`41_system/packages.d/network.txt`, or one `src/` area's, `src-<area>.txt`; a file's banner repeats
the shelf's line from `ports/shelves`. In a single `packages.txt` a heading between two rules,
`# <shelf> — <description>`, opens each shelf's ports, in the order `ports/shelves` lists them,
and after the shelves a `# src-<area> — <description>` heading opens the ports of KDOS's own from
that `src/` area, as `50_desktop`'s list does for `src-desktop` and `src-daemons`; the two shortest lists, `20_selfhost`'s and `60_kernel`'s, have no headings. The grouping exists
for the reader, and the build ignores it. Preflight refuses a `packages.d/` file that names a port
filed on another shelf, and `testing/phaseclosure.py` refuses a shelf's ports naming one an earlier
phase installs.

The second job is stating the constraint that pins a port's position, such as the block after
`toybox` described in [toybox and the tools it overlaps](#toybox-and-the-tools-it-overlaps). A run
of ports whose order a comment pins cannot be split across shelf files, so in a `packages.d/`
directory it lives in `00-order.txt`, which sorts ahead of every shelf file and installs first, in
the order written. `40_lang` keeps the toybox run there; `41_system` keeps `coreutils` ahead of
the text games that install with GNU `install`, and each Python module straight after the program
whose vendor bundle carries a copy of it, so that the port, installed last, owns the files.

The toolkits and the natively ported graphical applications build last among the userland phases,
in `43_toolkits` and `44_apps`. These toolkits serve applications only; the desktop's own
programs, which `50_desktop` builds, link none of them (see
[Principles](../01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).

Order within a list matters. The orchestrator hands the whole list to `kpkgdepends`, which walks
each name in turn and emits its dependencies before it, so a name written earlier is installed
earlier unless a dependency says otherwise. Each port in the resulting order is then installed
with `kpkg install`, which skips a package whose recipe has not changed; see
[Deciding what to rebuild](#deciding-what-to-rebuild). How phases are discovered, entered and
snapshotted is in [The build system](../05-developer/build-system.md#phases), and
[The ports catalogue](../06-reference/ports-catalogue.md) lists every port by shelf, with the
phase that installs it.

### The group key

A recipe may also carry a `group =` key, and that is a different kind of group. It is read only by
the upstream version checker, `ports/update`, never by `kpkg` or the build. The checker offers
related version bumps together: two ports in the same group are offered as one bump, and only when
every member has the same newer version available upstream. Anything short of that is not offered
as a group: each member that has a newer version is then reviewed on its own, like a port with no
group, and keeping a set of ports that must move in step together is left to the reviewer.

Most ports need no key. Without one, the checker derives a group from the first `source =` URL: two
ports whose sources come from the same organisation on GitHub, Codeberg, sr.ht or a GitLab
instance, at the same version, fall into one group. The key overrides that derivation. In
`ports/core`, 60 recipes carry it, naming twelve groups between them. Most
are pairs taken from one upstream release: `glib` and `glib-introspection` carry
`group = glib`, `python3` and `python3-tkinter` carry `group = python3`, `webkitgtk` and
`webkitgtk6` carry `group = webkitgtk`, and the same holds for `gcc-arm-none-eabi`, `mgba`, `qca`,
`qscintilla`, `qwt`, `supertuxkart` and `texlive`. The other two are families released together
from a host the derivation does not read: the Qt modules and PySide, fetched from `download.qt.io`,
carry `group = qt6` (29 recipes) or `group = qt5` (11).
[Writing ports](../05-developer/writing-ports.md) covers the key and the review it feeds.

A `group =` family is not a shelf either, but it lives on one: every member of a group is filed on
the same shelf, so a bump offered as one group touches one directory. Preflight refuses a group
whose members sit on different shelves.

## Where sources come from

Every upstream file a recipe uses is named by a `sha256 =` line in its `kpkgbuild`. That hash is
the file's identity: any copy that hashes to it is the right file, wherever it came from. Git
carries the recipes and a few small hashed files committed beside them, such as bash's patch-level
files; every other hashed file, the upstream archives and the vendor bundles, is fetched.

`make fetch`, which runs `ports/fetch`, puts every hashed file that git does not track into its port
directory and is the only step that uses the network. `make build` reads nothing but port
directories, so a tree fetched once builds offline indefinitely. For each file, `ports/fetch` stops
at the first copy that verifies, looking in this order:

1. the port directory itself;
2. the local cache, `ports/.srccache/sha256-XX/<hash>` (`XX` is the hash's first two hex digits),
   hard-linked into the port directory;
3. the KDOS source archive,
   `https://github.com/kunaldawn/kdos/releases/download/sources-NNN/<hash>`, where
   `ports/sources.idx` gives `NNN` for each hash; a hash the index does not name skips this step;
4. the recipe's own `source =` URL upstream;
5. for a port's own vendor bundle only, generating it again.

A **vendor bundle** is a port's language dependencies (Go modules, Rust crates, Python or Haskell
packages) packed as `<name>-vendor-<version>.tar.xz`; 164 ports carry one. It has a `sha256 =`
line but no `source =` URL, so when no copy exists anywhere it is generated rather than downloaded,
inside the `kdos-fetch` container that `ports/Containerfile.fetch` describes.

Whatever verifies is entered into the cache, so switching branches downloads nothing twice.

The source archive is content-addressed and append-only. It is a series of numbered GitHub
releases, `sources-001`, `sources-002` and onwards, filled in order up to 1,000 assets each (the
limit GitHub places on one release), each asset named by its bare hash; nothing is ever replaced
or removed. The committed index `ports/sources.idx` has one line per file,
`<hash> <NNN> <port>/<file>`, and each release's notes list what it holds. A checkout years old
therefore finds the exact bytes it was written against even after the upstream host has gone. The
index names 1,678 files: 1,000 in `sources-001` and 678 in `sources-002`. The current `ports/core`
recipes name 2,487 distinct hashed files. 39 of them are small files git tracks beside their
recipes, and the other 2,448, about 38.6 GiB, are fetched. 1,186 of those are in the archive; the
other 1,262 are not, so `make fetch` takes them from upstream. The remaining 492 files the index
names are ones no current recipe names. Stored by hash, a file several
ports use is one asset; the LLVM monorepo tarball, for example, is shared by eight ports.

The commands and settings:

| Command or variable | Does |
|---|---|
| `make fetch` | Fetch and verify everything; generate missing vendor bundles in the `kdos-fetch` container |
| `make fetch-check` | Offline (`ports/fetch --check`): list each archived source that is missing or corrupt, download nothing, exit 1 if any is |
| `ports/fetch [port…]` | Fetch only the named ports |
| `ports/fetch --tree <dir> [port…]` | Fetch for another checkout's `ports/core`; never generates a vendor bundle |
| `KDOS_SOURCES_BASE=` (empty) | Skip the archive and go straight to upstream |
| `KDOS_SOURCES_REPO` | The archive repository (default `kunaldawn/kdos`) |
| `KDOS_SOURCES_INDEX` | The index to read (default `ports/sources.idx`) |
| `KDOS_SRCCACHE` | Move the cache, for example to share one between checkouts |
| `KDOS_FETCH_HOST=1` | Do everything in one pass on this host, generating bundles with its own toolchains |

An empty file or a hash mismatch is always refused, and a corrupt cache entry is deleted. A file
whose recipe carries no hash yet, as after a `version =` bump, can only come from upstream, and a
vendor bundle with no hash is generated.

The contributor's side of this is adding a new source to the archive: `ports/publish` uploads it
and writes its index line, and an opt-in pre-push hook (`script/hooks/pre-push`, enabled with
`git config core.hooksPath script/hooks`) refuses a push whose recipes name a hash that the pushed
`ports/sources.idx` does not. Both are described in
[Writing ports](../05-developer/writing-ports.md#publishing-sources); the fetch flow, step by step,
is in [Developing](../05-developer/developing.md#where-sources-come-from).

## kpkg

`kpkg` is the package manager: one C program that builds ports, installs and removes packages,
resolves dependencies, signs and verifies binary packages, and makes deltas. It is one binary
answering to five names. The bootstrap phase installs it as `/usr/bin/kpkg` with the other four
names as symbolic links, and what it does depends on the name it was run as:

| Name | Does |
|---|---|
| `kpkg` | The front end, with the commands below |
| `kpkgadd` | Install a prebuilt package file: `kpkgadd [-f] [--overwrite] [--root <path>] <file>` |
| `kpkgbuild` | Build the port in the current directory into a package without installing it |
| `kpkgdel` | Remove packages: `kpkgdel [--root <path>] <package>…` |
| `kpkgdepends` | Print the resolved install order, and nothing else |

The `kpkgbuild` command shares its name with the recipe file it reads; this chapter says "the
`kpkgbuild` file" where the file is meant.

The front end's commands:

| Command | Does |
|---|---|
| `install <pkg>…` (`i`) | Build and install packages with their dependencies |
| `remove <pkg>…` (`r`) | Remove packages |
| `list [--json]` (`l`) | List installed packages |
| `info [--json] <pkg>` | For an installed package, its version, release and file count; for one that is not installed, the recipe's description and `depends =` list |
| `meta <pkg or portdir>` | Print the recipe's metadata as shell assignments, which `ports/fetch` reads |
| `verify <pkg>` | Build the current recipe and a candidate written beside it as `kpkgbuild.new` (with an optional `build.sh.new`), then compare the two packages |
| `verify --repro <pkg>` | Build the same recipe twice; the two packages must be byte-identical |
| `keygen <name>` | Make an Ed25519 signing key pair |
| `index <dir> [--sign <key>]` | Write `PACKAGES` for a directory of packages, optionally signing it and every package |
| `verify-index <dir>` | Check `PACKAGES` against the trusted keys |
| `verify-pkg <file>` | Check `<file>.sig` against the trusted keys |
| `delta <old> <new> [-o <file>]` | Write the difference between two packages |
| `apply-delta <old> <delta> -o <new>` | Rebuild a package from a delta |
| `binhost <dir> <pkg> [--dry-run] [--insecure]` | Install a prebuilt package if it matches this machine exactly |
| `help` | Print usage |

Options and environment:

| Option or variable | Effect |
|---|---|
| `--root <path>`, `KPKG_ROOT` | Operate on another root directory, such as an installer's target |
| `--keep-cache` | Keep each built package in `PACKAGE_DIR` after installing it; by default it is deleted |
| `-f`, `--force` | Rebuild the named packages, though not their dependencies, and skip the file-conflict scan for them |
| `--overwrite`, `KPKG_OVERWRITE=1` | Let a package take a path another package owns; ownership moves with the file |
| `KPKG_CONF` | Configuration file (default `/etc/kpkg.conf`) |
| `KPKG_KEYRING` | Trusted-key directory (default `/etc/kdos/keys`) |
| `KPKG_REQUIRE_SIG=1` | Refuse any package without a valid signature |
| `KPKG_STRICT_RECIPE=1` | Rebuild an installed package whose recipe hash differs; without it, every installed package is skipped. See [Deciding what to rebuild](#deciding-what-to-rebuild) |
| `KDOS_ALLOW_UNVERIFIED=1` | Extract a source the recipe gives no hash for; see [What a build verifies](#what-a-build-verifies) |

An unknown option is refused rather than read as a package name.

`/etc/kpkg.conf` sets `PORT_REPO` (default `/ports/core`), `SOURCE_DIR`
(`/var/cache/kpkg/sources`), `PACKAGE_DIR` (`/var/cache/kpkg/packages`), `WORK_DIR`
(`/var/cache/kpkg/work`) and `PKGDB_DIR` (`/var/lib/kpkg/db`). Every line has the form
`NAME="${NAME:-default}"`, and `kpkg` reads it as key and value without running a shell. An
environment variable of the same name always wins over the file, which is how each phase's
environment file points `kpkg` at its own repositories.

The solver is a depth-first walk of `depends =` lines: each port's dependencies are emitted before
the port itself, a port is visited once, and a dependency cycle terminates rather than recursing
without end. A package that is installed and current is dropped from the order before any build
starts.

`kpkgdepends` writes one bare, space-separated line to standard output and nothing else, because
the build orchestrator parses it. The orchestrator checks every token against the pattern
`^[A-Za-z0-9][A-Za-z0-9._+-]*$` and treats empty output as an error, and diagnostics from anything
running inside the build chroot go to a log, so stray output fails loudly instead of being
installed as a package.

`kpkg` links three of this tree's libraries, `libkbase`, `libkpkg` and `libksig`, and nothing else
but the C library, so it is cross-compiled early and exists on every tree from the first bootable
image onwards.

## Packages

A package is a compressed tar archive plus a database entry.

| | |
|---|---|
| File name | `<name>-<version>-<release>.tar.xz`, parsed from the right because a name may contain hyphens |
| Database entry | `/var/lib/kpkg/db/<name>`: the version and release on the first line, then the manifest |
| Manifest | Every path the package owns, `./`-prefixed, directories with a trailing slash |
| Recipe hash | `/var/lib/kpkg/db/.recipe/<name>`, one line; see [Deciding what to rebuild](#deciding-what-to-rebuild) |
| Install hook | `.POSTINSTALL` at the root of the archive, when the port has a `postinstall.sh` |

The file name is the only metadata a package carries. The install hook is a standalone bash
script: a shebang, the recipe's metadata, then `postinstall.sh` byte for byte. `kpkgadd` lifts it
out before placing any file, so it is never installed and never in the manifest, and runs it after
the install; a hook that fails produces a warning, not a failed install.

`kpkgadd` extracts into a staging directory on the target filesystem, so placing a file is a
rename. A file that cannot be placed aborts the install before any database entry is written,
because an entry written past a failure would claim a complete install of a package that is half
on disk. Writing to the root is permitted whenever the root is writable, which is what lets a
build install into a sysroot (a directory standing in for a target's root filesystem) it owns
without being root. `kpkgadd` creates every directory 0755, whatever mode it was packaged with:
the package is rolled root:root, and a mode that leaned on a daemon's group would lock that
daemon out. A hook that needs a directory's mode sets it.

### Who owns a file

A file conflict is between *packages*. A path that exists but that no installed package claims is
**adopted**, not refused. This is what makes the bootstrap work: the earliest build phases install
a toolchain by hand, leaving files no database entry owns, and the bootstrap then rebuilds those
packages with `kpkg`.

A path that another package does own is a conflict. Where the userland overlaps, as when
toybox (the compact userland) ships a name that a full GNU tool also provides, whoever comes last
in dependency order wins, through the explicit overwrite flag. The build sets `KPKG_OVERWRITE=1`
for every package it installs. That flag does one thing besides allowing the write: the path
changes hands in the database and is removed from the previous owner's manifest. Without that,
removing the older package would delete a file the newer one installed.

Ownership is compared on the canonical path. The root's `bin`, `sbin`, `lib` and `lib64` are
links into `/usr`, so a package built with `--exec-prefix=` records `./bin/free` for the same file
another records as `./usr/bin/free`. Compared as strings the two never collide: the scan passes,
the file is claimed twice, and removing or upgrading either package deletes the other's file.
`kpkg` reads which top-level names are such links from the root it installs into, and folds each
path through them before comparing.

### toybox and the tools it overlaps

toybox is built without every applet whose name another port on the image installs, so each name
has one owner and one implementation. The GNU tools the bootstrap cannot do without, `sed`, `find`,
`xargs`, `awk`, `expr` and `ln`, are the exception: toybox keeps them, the GNU ports come after it
in dependency order and take them over, and upgrading toybox alone puts its applets back until
those ports are reinstalled.

Dropping one more applet from toybox without bumping the release of the port that owns that name
leaves the name missing after a toybox upgrade. A toybox installed after the owning port holds the
name in its manifest, its upgrade removes the name, and `kpkg` skips the owner because its recipe
hash is current, so nothing puts the name back. A contributor who removes an applet therefore
bumps the owning port's `release` in the same change. `toybox` and the owners that
[`20_selfhost` and `30_foundation`](../05-developer/build-system.md#phases) install before it are
named again at the head of `40_lang`, in `packages.d/00-order.txt`, with toybox first and the
owners straight after it. That is the first phase after the long compiler phase, so a build that
continues from `40_lang` reinstalls them too, and their names return before a later build step runs
them: bzip2's `make`, for one, runs `cmp`.

### Upgrades and removals

An upgrade removes orphans. A file present in the old version and absent from the new one is
removed rather than left on disk owned by nothing, unless another package claims it, in which case
it is that package's file and stays. "Absent" is judged on the canonical path, so `./bin/x` in the
old version and `./usr/bin/x` in the new are one file.

A removal walks the manifest in reverse, so a directory is reached only after everything inside it;
a directory that still holds another package's files survives. A file another installed package
claims is left in place. `kpkgdel` does not check reverse dependencies: `kpkgdel bash` removes
bash. A name that is not installed is reported and skipped, and the rest of the command line is
still processed.

An install or removal ends by rebuilding the shared indexes its manifest fed, from everything then
on disk: the GSettings schemas, the GIO module and pixbuf loader caches, the MIME database, the
desktop entries' `mimeinfo.cache`, the font cache and the X core fonts' `fonts.dir`, the info
directory, the TeX `ls-R` files and the udev hardware database. The
manual index is merged instead: an install that only adds pages adds them to `mandoc.db`, and the
index is rebuilt from the whole tree only when a page was removed (by a removal, or as an upgrade's
orphan) or when there is no `mandoc.db` yet. A tool that is not installed yet is skipped, and a
tool that fails produces a warning, because the package is already on disk. No package owns these
files, so no package ships them; `kpkgbuild` deletes the info `dir` file and each `fonts.dir` from
the staged tree for that reason. The list is in
[Writing ports](../05-developer/writing-ports.md#shared-indexes).

### Files kpkg keeps to itself

The system Python's `site-packages` belongs to `kpkg`. `python3` ships the PEP 668
`EXTERNALLY-MANAGED` marker, so `pip install` outside a virtual environment is refused rather than
allowed to replace a port's files, which the next upgrade or removal of that port would delete or
conflict with. To install Python software that is not a port, create an environment with
`python3 -m venv DIR`; `python3-pip` ships the pip wheel that `ensurepip` installs into it.

`/etc/localtime` is kept out of every package for the same reason: a file the machine's owner
changes cannot also be a file an upgrade rewrites. A machine whose installed `tzdata` manifest
still lists it has the link removed as an orphan on the next upgrade; see
[Known gaps](../06-reference/known-gaps.md#a-tzdata-upgrade-can-reset-the-time-zone-to-utc-once).

## What a build verifies

The `kpkgbuild` command runs in the port directory. It parses the `kpkgbuild` file, checks every
declared hash, unpacks the sources under `WORK_DIR`, sources `build.sh` in a subshell under
`set -e`, and rolls whatever the build staged into a package in `PACKAGE_DIR`. The build sees these
shell variables:

| Variable | Value |
|---|---|
| `PKGNAME` | `<name>-<version>-<release>.tar.xz` |
| `PORT_SRC` | The port directory |
| `SRC_ROOT` | `$WORK_DIR/<name>` |
| `SRC` | `$WORK_DIR/<name>/<name>-<version>`, the working directory when `build.sh` starts |
| `PKG` | `$WORK_DIR/<name>/pkg`, where the build installs its files |

`name`, `version`, `release` and every recipe-local helper key are defined too, because a recipe is
never sourced and cannot define them itself. The first source is unpacked into `$SRC` with its
top-level directory stripped; later tarballs are unpacked unstripped beside it in `$SRC_ROOT`, and
any other file is copied into `$SRC`. A `.zip` or `.tar.zst` is copied, not unpacked, and the
recipe unpacks it itself. Before rolling the package, `kpkgbuild` deletes every libtool `.la` file,
because each names build-time paths that do not exist on the target.

Before it touches the work directory, `kpkgbuild` hashes every file a `sha256 =` entry names
that is present beside the recipe or in `kpkg`'s source directory (`SOURCE_DIR`, default
`/var/cache/kpkg/sources`), and refuses on any mismatch.

That is wider than the `source =` list on purpose, because of vendor bundles (see [Where sources
come from](#where-sources-come-from)). 164 ports carry a vendor bundle, most of them Go, Rust,
Python or Haskell programs, with pdfium among the rest, and each unpacks it in `build.sh` itself. A
vendor bundle is declared with a hash but named by no `source =` line, so a check that walked
`source =` alone would compile those ports from bytes nothing had looked at.

A declared file that is in neither place is skipped rather than refused. A source the build needs
is caught when extraction cannot find it, and failing on a declared file the build never opens
would refuse a port over a hash that cannot affect it.

On top of that, no source is unpacked before its bytes match. A source the recipe names with no
`sha256 =` for it is a hard failure, not a warning. `KDOS_ALLOW_UNVERIFIED=1` is the escape hatch
for bringing up a new port before its hash is known, and `testing/preflight.sh` checks that no
recipe in the tree needs it.

The first `source =` entry is looked for under the standard name `<name>-<version>.<ext>` (for
example `zlib-1.3.2.tar.gz`), which is the name `ports/fetch` saves it under; later entries keep
their URL's basename, and `file::url` names a file explicitly.

## Deciding what to rebuild

The build must not recompile 2,023 ports on every run, and must not skip one whose recipe changed.
Two hashes decide, and they are the same two the binary host uses.

### `E:` — the recipe hash

The recipe hash is SHA-256 over the `kpkgbuild` file, `build.sh`, `postinstall.sh` and every
`.patch` file, sorted by name, each contributing its name, its length *and* its bytes. The length
stops two files from hashing the same as one by moving the boundary between them. The result is an
exact statement of what a package was built from.

For a port that names a `source =`, nothing else in the directory is hashed: a tarball or a vendor
bundle is covered by its own `sha256 =` line, which `kpkgbuild` checks before it builds anything
(see [What a build verifies](#what-a-build-verifies)). A file beside the recipe that no `sha256 =`
names is in neither this hash nor that check.

Such a file still reaches the package when `build.sh` reads it from `$PORT_SRC`. `linux` appends
`kdos.config` to the kernel configuration and copies in its panic-screen logo this way, and `doxx`
and `epy` install a `.desktop` file. Editing one changes nothing the recipe hash sees, so the change
reaches the next build only when the same change bumps the port's `release`.

A port with no `source =` is different, and this is what keeps the rule true for this tree's own
code. Such a port builds out of its own directory: nothing names those files and no checksum covers
them. If only the four recipe files were hashed, editing a `.c` file would change nothing the build
can see. The port would report as installed and current, and the tree would keep the binary it
already had. The symptom would never be a build error; it would be a shipped program behaving like
an older one.

So a source-less port hashes its whole directory, sorted at every level. A port under `src/`
hashes all of `src/libs` with it: the library directory is found at `../../libs` from the port,
which from `src/<area>/<name>/` is `src/libs`. Each `build.sh` names which libraries it compiles,
and working that out would need a shell parser inside the package manager, so every library is
included. The two source-less ports in `ports/core`, `containers-common` and `musl-ldd`, compile
none of those libraries. From `ports/core/<shelf>/<name>/`, `../../libs` is `ports/core/libs`,
which does not exist, so their hash covers their own directory alone. This is why no shelf may be
named `libs`: a shelf by that name would be hashed whole into both recipes, and preflight and the
pre-push hook refuse it.

The cost is that editing one library rebuilds every port of KDOS's own, not only the ones that use
it. Upstream ports' hashes are unaffected.

### `B:` — the build-config hash

The build-config hash is SHA-256 over the architecture, the C library (always musl), the target
triple (`KDOS_TARGET`, or `native`), the compiler and its version, and the `CFLAGS`, `CXXFLAGS` and
`LDFLAGS`. Two machines with the same
`B:` produce comparable binaries; two with different `B:` do not, whatever the recipe says.

Both hashes include their field names, because a hash over values alone collides the moment
two fields swap.

### The three states

With `KPKG_STRICT_RECIPE=1`, which `script/env/common.env` sets for every build phase, `kpkg`
compares an installed package's recorded recipe hash with the port as it stands. Without it, as in
an interactive `kpkg install`, an installed package is skipped whatever its recipe says. With the
check on:

| State | Result |
|---|---|
| Hashes match | Skip |
| Hashes differ | Rebuild |
| No recorded hash, or a corrupt one | Skip |

The third row makes the check safe on a tree that has packages without a record: absent reads as
*unknown*, never as *changed*, so no package is rebuilt merely for lacking a record. A record that
is not exactly 64 lowercase hex digits is treated the same way; reading it as a mismatch would
rebuild that one package on every run, with nothing saying why.

The hash is recorded in `/var/lib/kpkg/db/.recipe/<name>` after a successful install, never
before. A record written ahead of a build that then fails would claim a recipe is installed that
is not. It is a separate file rather than a field of the database entry, so the entry keeps the
fixed shape given under [Packages](#packages): the version and release on the first line, then the
manifest.

The dependency solver applies the check, not the install loop. An installed and current package is
dropped before the loop runs, so a check placed later would reach only packages named on the
command line and miss every *dependency* whose recipe changed.

## Reproducible packages

A package built twice from the same tree is byte-identical. That is a property of one function, the
archive roller inside `kpkg`, rather than of 2,023 recipes, which is why `kpkg` rolls the archive
itself instead of letting each `build.sh` do it.

Each setting removes one source of difference between two builds:

| Setting | Without it |
|---|---|
| `--sort=name` | Directory order is filesystem order, which is not stable even between two copies of the same tree |
| `--mtime=@$SOURCE_DATE_EPOCH` | Every file carries the second it was installed; with the variable unset, `kpkg` uses 0 |
| `--owner=0 --group=0 --numeric-owner` | The builder's user id, and its *name* as text in the header |
| `--format=gnu` | Extended (pax) headers carry access and change times, which are wall clock; plain ustar cannot hold a path over 255 bytes, which some ports have |
| `--use-compress-program=xz -9 -T1` | Multi-threaded compression is not deterministic, and `XZ_OPT` in the environment can silently enable it |
| `umask(022)` before the build | A file created without an explicit mode takes the builder's umask: the one source of drift that is not in the archive call |

The other half is five settings in `script/env/common.env`, which every phase's environment sources:

| Line | Purpose |
|---|---|
| `SOURCE_DATE_EPOCH=1735689600` | A pinned epoch, not the current date and not derived from git: the build container mounts only `build`, `src`, `fs`, `script` and `ports`, not the repository's `.git` |
| `TZ=UTC` | Dates formatted during the build do not depend on the builder's zone |
| `LC_ALL=C` | Sorting and formatting do not depend on the builder's locale |
| `-ffile-prefix-map=/var/cache/kpkg/work=/build`, on both `CFLAGS` and `CXXFLAGS` | Rewrites the build directory out of `__FILE__` and debug paths |
| `-Wl,--build-id=sha1` | The build identifier is a function of the contents, not random |

`ports/fetch` rolls vendor bundles with the same tar and xz settings, so regenerating a bundle
reproduces the file its `sha256 =` names.

Reproducibility is what makes a signed binary host meaningful, what lets a delta rebuild a package
that still verifies against the *original* signature, and what lets a rebuild be checked against
what it was built from (`kpkg verify --repro`).

## The binary host

A **binary host** (binhost) is a directory of prebuilt packages with a signed index. It is optional:
KDOS builds everything from source, and a binhost only saves compiling. It is a path, not a URL: a
USB stick, an NFS mount or any other directory.

```sh
kpkg keygen builder                      # once, on the machine that builds
kpkg index /repo --sign builder.key      # PACKAGES + PACKAGES.sig + a sidecar per package
cp builder.pub /etc/kdos/keys/           # on every machine that should trust it
kpkg binhost /repo zlib                  # install it, or say why it will not
```

The index, `PACKAGES`, is a flat text file in the shape Alpine uses: single-character keys, one
stanza per package, a blank line between stanzas. It parses in a few dozen lines of C and reads
well in a pager. A header stanza (`K:kdos-index-1`, then the indexing machine's `A:` and `B:`)
opens the file. Each package stanza carries:

| Key | Value |
|---|---|
| `P:` `V:` `R:` | Name, version, release |
| `A:` | Architecture |
| `F:` | The package file name |
| `S:` `C:` | Its size and SHA-256 |
| `B:` | The build-config hash of the machine that wrote the index |
| `E:` | The recipe hash, taken from the ports tree on that machine; empty for a package whose port that tree does not have |
| `T:` | The recipe's description |

Three equality tests decide whether a prebuilt package is usable, and there is no "close enough":
the architecture, the build-config hash `B:`, and the recipe hash `E:`. Anything else builds from
source. `kpkg binhost` says which test failed, and its exit status says what happened:

| Exit | Meaning |
|---|---|
| 0 | The prebuilt package was used |
| 1 | No match, so build it from source; also a usage error, or a matching package whose install failed |
| 2 | Refused: no index, no trusted key, or verification failed |

`make build KDOS_MAKE_BINHOST=1` writes such a directory at `build/binhost/`: every `kpkg install`
of the build keeps the package it made, and the image phase, `70_image`, copies them there and
indexes them with a key kept under `build/binhost-key/`. That phase runs with the default
`PORT_REPO`, `/ports/core` alone, so the index finds no recipe for a package built from `src/` and
gives it an empty `E:`. Such a package matches no client and always builds from source there.

Gentoo solves the same problem by matching USE flags between the builder and the client. KDOS has
no USE flags, so the whole question reduces to these equality tests;
[How KDOS differs](../01-philosophy/how-kdos-differs.md#updates) sets this against other
distributions' update models.

### Signing

Signatures are Ed25519, through Monocypher, a vendored implementation dual-licensed BSD-2-Clause and
CC0, under `src/libs/libksig/monocypher`. See [The C libraries](../05-developer/c-libraries.md).

One signature over the index covers every package, because the index carries each package's hash.
A per-package sidecar signature, `<file>.sig`, exists for the separate case of a package travelling
on a stick with no index beside it.

The scheme keeps these rules:

- A key id is a label, not a selector. Every signature line is tried against every key in the
  trusted directory, and against nothing outside it. Tools report the key that verified, not the id
  the line claimed, and a line naming an unknown id still verifies if a trusted key signed it. A
  signature that could supply its own key would verify nothing.
- A bad signature is treated differently from a missing one. Installing a package whose sidecar
  fails is refused. One with no sidecar is allowed, because locally built packages are the majority
  and are never signed. `KPKG_REQUIRE_SIG=1` is the stricter policy for a machine that installs
  only from a binhost.
- `--insecure` prints a warning every time it is used, so it is never left quietly in a script.
- Everything is verified before use: the index before it is believed, and a package's hash before
  it is unpacked.
- Several signatures are allowed. A signature file is one line per signature, so during a key
  rollover both keys sign and a client trusting either keeps working.
- The private key is written with mode 0600 and exclusive creation, so an existing key is never
  replaced, and reading it back is refused if its mode has loosened.

The trusted directory is the policy: every `*.pub` file in it is a trusted key. There is no
revocation list and no online check: trusting a key is copying a file in, and removing trust is
deleting it. The loader does not descend into subdirectories, which keeps `/etc/kdos/keys`
(host packages) and `/etc/kdos/keys/packs` (application packs) separate policies.

Ports built from source are not signed and need no signature. A port is compiled on the machine
that installs it, and its integrity rests on the `sha256 =` lines in its recipe. A signature
identifies who built a binary, which for a locally built package is the machine itself, so it adds
nothing.

## Deltas

```sh
kpkg delta zlib-1.3.1-1.tar.xz zlib-1.3.2-1.tar.xz
kpkg apply-delta zlib-1.3.1-1.tar.xz <delta> -o zlib-1.3.2-1.tar.xz
```

A delta is a binary difference between two packages, so a machine that already has the old
version downloads kilobytes instead of the whole package. Without `-o`, `kpkg delta` names the
file `<new>--from--<old>.kdelta`, which is the name `kpkg index` recognises. The engine is
`zstd --patch-from`, with two design decisions around it.

The first is that the delta is taken over the uncompressed archives. Two compressed files built
from nearly identical trees share almost no bytes, since removing repetition is what a compressor
does, so a delta between them is as large as the package.

The second is that a delta is never trusted, and never needs to be. It is applied, and the result is
hashed against the entry the signed index already carries. A tampered delta produces a package whose
hash does not match, which is discarded; it cannot make a client install anything the index did not
already name. So there is no delta signature and no second trust path.

A delta appears in the index as its own stanza: the `P:`, `V:`, `R:` and `A:` of the package it
rebuilds, the `F:`, `S:` and `C:` of the delta file, and `O:` naming the package file it applies
to. It has no `B:`, `E:` or `T:`. `kpkg binhost` uses one only when that old package file is still in
`PACKAGE_DIR`; `kpkg install` deletes each package it builds unless given `--keep-cache`. Any
failure along the way, a delta that does not match the index, does not apply, or rebuilds the
wrong bytes, falls back to the full package.

## Updating a machine

`kdos update` is the command a person types. It drives `kpkg`, `kpkg binhost` and the tool that
manages the A/B root slots (an installed disk's two root filesystems, one running and one
receiving updates; see [Boot and init](boot-and-init.md#ab-slot-selection)), and adds no new trust
path.

| Command | Does |
|---|---|
| `kdos update check` | Compare what is installed with what the ports tree on this machine pins; exit 1 when something is behind |
| `kdos update apply` | Install what is behind: from the binhost where it has a match, compiled otherwise |
| `kdos update theme` | Re-run the theme generators for your home directory after an artwork upgrade |

New versions arrive with the ports tree, not with the binhost: a recipe pins its version, so a
binhost package that matches this machine is by construction the version the tree would build. A
machine with no ports tree therefore cannot update: it cannot tell what is newer, and without a
recipe `kpkg` computes no recipe hash, so no binhost stanza matches. A stick built with
`KDOS_ISO_SOURCES=1` carries a ports tree.

The binhost is named by `binhost = /path/to/repo` in `/etc/kdos/update.conf`, or by
`$KDOS_BINHOST`, which wins. On a machine with A/B root slots, `apply` installs into the inactive
slot and marks it to be tried on the next boot, so the running system is untouched and a bad update
rolls back. If that slot is not mounted, `apply` refuses; `--in-place` makes it update the running
root instead, which gives up the rollback. Every option is in
[the kdos command](../04-programs/kdos-command.md#kdos-update).

## Vulnerability tracking

```sh
kdos cve              # every package
kdos cve openssl      # one package
kdos cve --json       # the same, as a document
```

`kdos cve` compares versions and does not scan files: a package is reported when its version is
older than the version the database gives as fixing an issue. On a running system the versions are
those in the installed package database; with no database, as in a checkout, they are the versions
the ports tree pins, and the report says which it used. The comparison is the package manager's own
version comparison, shared with the upstream-version checker (`ports/update`), so the two cannot
disagree about what "newer" means.

The data is a vendored, pruned copy of Alpine's security database, installed as
`/usr/share/kdos/secdb.txt` from `src/system/kdos-tools/secdb/secdb.txt`; `KDOS_SECDB` names
another file. It is a committed, diffable text file generated by `vendor.py` beside it and merged
from twelve Alpine branches (`main` and `community` for v3.19 to v3.24), so the answer needs no
network. Alpine is a close proxy for KDOS because it is also a musl distribution building the same
upstream tarballs. See [Decisions](../01-philosophy/decisions.md) for why Alpine rather than a
larger source.

Four details each change the answer:

- Alpine's packaging revision is stripped before comparing. Leaving it on makes every version look
  old.
- "Fixed in 0" means never affected in that branch, and falls out of the comparison, since nothing
  is older than 0.
- The newest fix a version is behind is the one reported, because it closes every earlier one too,
  and the identifiers from all matching rows are merged.
- A `secdb =` key in a recipe maps a port whose name differs from Alpine's.

A package the database does not carry is reported as UNKNOWN, never as clean. A large part of the
tree is in that state and the summary says so. A fix Alpine never shipped is invisible to the check.
The database's age is printed with every run, with a warning once it is more than 180 days old.

`ports/update --cve` is the online cross-check: it asks repology (repology.org, a service that
tracks package versions across distributions) whether each pinned version is flagged vulnerable,
one request per port against a rate-limited service. That cost is why the
vendored table is the everyday answer.

## See also

- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build as one story, from clone to ISO
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — this packaging set against other distributions'
- [The build system](../05-developer/build-system.md) — how phases are run and drive `kpkg`
- [Writing ports](../05-developer/writing-ports.md) — the recipe format and how to add one
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port, by shelf, with its phase
- [Developing](../05-developer/developing.md) — fetching sources and running the build
- [Packs and boxes](packs-and-boxes.md) — the other packaging system, and why it is separate
- [The security model](security-model.md) — the trust argument behind the keyrings
- [The kdos command](../04-programs/kdos-command.md) — `kdos cve` and `kdos update`

<!-- book-nav -->
---

*Part III — Architecture, chapter 15.* Previous: [14. The session](session.md) · [Contents](../README.md) · Next: [16. Packs and boxes](packs-and-boxes.md)
