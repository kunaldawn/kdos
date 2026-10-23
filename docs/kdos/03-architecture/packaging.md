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
| `/ports/core` | `ports/core/` | 2,083 | Upstream software, filed on 102 shelves |
| `/kdos/src/system` | `src/system/` | 7 | KDOS's own `kdos` command and tools, the packer, the box runtime and its init, the installer, and the two books on the image |
| `/kdos/src/art` | `src/art/` | 6 | KDOS's own theme generator, icon, cursor and GTK themes, boot splash and demo |
| `/kdos/src/desktop` | `src/desktop/` | 8 | KDOS's own compositor, panel, terminal, lock screen, resource monitor, box socket, recorder and portal |
| `/kdos/src/daemons` | `src/daemons/` | 5 | KDOS's own root daemons |

That is 2,109 recipes in all (counted as directories holding a `kpkgbuild`).

A repository holds a port either directly, as `<repo>/<name>/`, or one level down, as
`<repo>/<shelf>/<name>/`. The `src/` areas hold theirs directly. `ports/core` holds none directly:
every upstream port sits on a **shelf**, a directory named for its subject, such as
`ports/core/wl/wlroots/`, `ports/core/fonts/noto-fonts/` or
`ports/core/python-net/python3-requests/`. The shelves are a closed list, the file `ports/shelves`,
with one line per shelf giving its id, the source-archive volume that keeps its files, and what
belongs on it. [Writing
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
them itself. Its default, from `/etc/kpkg.conf`, is the recipe tree a shipped image carries:
`/ports/core` and the four areas of `/ports/src` that hold ports. Inside the build's chroot only
`/ports/core` of those exists, so `20_selfhost`, `30_foundation`, `31_compilers` and `70_image`,
which use that default, resolve against `/ports/core` alone (`00_cross` and `10_bootstrap` do not use `PORT_REPO`: their
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
| `20_selfhost` | `packages.txt` | 9 | 15 |
| `30_foundation` | `packages.txt` | 138 | 127 |
| `31_compilers` | `packages.txt` | 22 | 22 |
| `40_lang` | `packages.d/`, 13 files | 177 | 177 |
| `41_system` | `packages.d/`, 96 files | 1,014 | 1,014 |
| `42_graphics` | `packages.d/`, 54 files | 194 | 194 |
| `43_toolkits` | `packages.d/`, 47 files | 250 | 250 |
| `44_apps` | `packages.d/`, 58 files | 284 | 284 |
| `50_desktop` | `packages.txt` | 22 | 22 |
| `60_kernel` | `packages.txt` | 2 | 2 |

"Names" counts the port names a list writes; "Installs" counts the packages the phase installs
that no earlier phase did. Between them the lists name 2,103 distinct ports and install 2,107:
2,082 of the 2,083 in `ports/core`, and every recipe under `src/` except `kdos-installer`, which
the bootstrap builds by name. The one recipe in `ports/core` no list reaches is `texlive-doc`,
which `ports/unbuilt` names: a recipe kept for `kpkg install` and built by no phase, and removed
from the image by the orphan sweep if an earlier build left it installed. Any other recipe no list
reaches would never be built, so `testing/bookcounts.py`, run by `testing/docscheck.sh`, refuses
one, and refuses a name in `ports/unbuilt` that a phase does install.

Where a list names more ports than it installs, the extra names are ports an earlier phase already
installed, named again so that one whose recipe changed is rebuilt at that point in the order. A
name may be written again only in the list's *order run*: `packages.d/00-order.txt`, or the names
a `packages.txt` gives ahead of its first shelf heading. `30_foundation`'s order run is the build
tools recipes use without naming them in `depends =` (`make`, `file`, `muon`, `pkgconf`, the autotools, `cmake`,
`meson`, `ninja`, `python3`, `perl`, `bash`, and the userland a `build.sh` runs in: `coreutils`,
`grep`, `which` and `musl-utils`), led by the toolchain `20_selfhost` built (`musl`, `gcc`,
`binutils`) and followed by the rest of `20_selfhost`'s ports, so a changed recipe among them is
rebuilt before anything uses it (see [The base userland](#the-base-userland)).

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

The second job is stating the constraint that pins a port's position, such as the build tools at
the head of `30_foundation`. A run of ports whose order a comment pins cannot be split across shelf
files, so in a `packages.d/` directory it lives in `00-order.txt`, which sorts ahead of every shelf
file and installs first, in the order written. `41_system` keeps each Python module there straight
after the program whose vendor bundle carries a copy of it, so that the port, installed last, owns
the files.

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
`ports/core`, 70 recipes carry it, naming seventeen groups between them. Most are pairs taken from
one upstream release: `glib` and `glib-introspection` carry `group = glib`, `python3` and
`python3-tkinter` carry `group = python3`, `webkitgtk` and `webkitgtk6` carry `group = webkitgtk`,
and the same holds for `gcc-arm-none-eabi`, `gcc-riscv64-unknown-elf`, `limesuite`, `mgba`, `nmap`,
`qca`, `qscintilla`, `qwt`, `stlink`, `supertuxkart`, `texlive` and `v4l-utils`. The other two are
families released together
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
   `https://github.com/kunaldawn/kdos/releases/download/sources-<N>/<asset>`, where
   `ports/sources.idx` gives the release tag and the asset name for each hash; a hash the index
   does not name skips this step;
4. the recipe's own `source =` URL upstream;
5. for a port's own vendor bundle only, generating it again.

A **vendor bundle** is a port's language dependencies (Go modules, Rust crates, Python or Haskell
packages) packed as `<name>-vendor-<version>.tar.xz`; 164 ports carry one. It has a `sha256 =`
line but no `source =` URL, so when no copy exists anywhere it is generated rather than downloaded,
inside the `kdos-fetch` container that `ports/Containerfile.fetch` describes.

Whatever verifies is entered into the cache, so switching branches downloads nothing twice.

The source archive is a run of numbered GitHub pre-releases on `kunaldawn/kdos`, the volumes
`sources-1`, `sources-2` and on, titled by the first and last shelf they hold. `ports/shelves` gives
each shelf its volume, and a volume holds the files of a run of shelves in sorted order. Each is a
pre-release and never "latest", so the repository's latest release is always a KDOS system release.
An asset carries its shelf and the file's own name, such as `archiver--zstd-1.5.7.tar.gz`; when that
name is already taken in the volume by other bytes it is `<shelf>--<port>--<file>`, and then
`<shelf>--<port>--<hash12>--<file>`. A file only old history names is `attic--<file>` in the highest
volume. A volume holds at most 1,000 assets; a file whose volume is full goes to the lowest-numbered
volume with room, or opens a new one, and the index records where. GitHub turns every character
outside `[A-Za-z0-9._-]` into a dot, so `libsigc++` is stored as `libsigc..`, and the index records
the name GitHub stored. A file larger than 1,900 MiB, near GitHub's 2 GiB limit per asset, is stored in parts
`<asset>.part01` onwards; `ports/fetch` downloads each part, checks it against its own hash, joins
them and checks the whole. For a moment the parts and the joined file are on disk together, so a
split file needs about twice its size free: 10 GB for the 4.96 GB TeX Live tree.

The committed index `ports/sources.idx` starts with the line `# kdos-sources-index 2` and has one
line per file, `<hash> <tag> <asset> <port>/<file>`, with `parts=<N>:<h1>,…` after it for a split
file. A `<tag>` is `sources-<N>`, or `src-<shelf>` for a file still in the older layout of one
release per shelf, which fetches the same way until `ports/publish --rehome` moves it. An index
without that first line is not read at all, and every file then comes from upstream.
Whatever the name or the release, a file is used only once it hashes to the recipe's `sha256 =`.

Nothing in the archive is replaced, and an asset is removed only by two explicit maintainer
commands: `ports/publish --rehome`, which moves a file to the volume and name of the shelf its port
now sits on and deletes the old copy only after the new one is verified and the index is pushed, and
`ports/publish --orphans --prune=yes-delete`, which deletes files no current recipe, no recipe at
any `v*` tag and no freeze list names. A checkout years old therefore finds the exact bytes it was
written against even after the upstream host has gone, fetched with the newest index through
`ports/fetch --tree`. The current `ports/core` recipes name 2,487 distinct hashed files over 102
shelves, the largest shelf, `python-libs`, naming 81. 39 of them are small files git tracks beside
their recipes, and the other 2,448, about 38.6 GiB, are archived in four volumes. Stored by hash, a
file several ports use is one asset, in the volume of the first shelf that names it; the LLVM monorepo tarball, for example, is shared by eight ports.

The commands and settings:

| Command or variable | Does |
|---|---|
| `make fetch` | Fetch and verify everything; generate missing vendor bundles in the `kdos-fetch` container |
| `make fetch-check` | Offline (`ports/fetch --check`): list each archived source that is missing or corrupt, download nothing, exit 1 if any is |
| `ports/fetch [port…]` | Fetch only the named ports |
| `ports/fetch --tree <dir> [port…]` | Fetch for another checkout's `ports/core`; never generates a vendor bundle |
| `KDOS_SOURCES_BASE=` (empty) | Skip the archive and go straight to upstream |
| `KDOS_SOURCES_REPO` | The archive repository (default `kunaldawn/kdos`) |
| `KDOS_SOURCES_INDEX` | The index to read (default `ports/sources.idx`); only format 2 is read |
| `KDOS_SRCCACHE` | Move the cache, for example to share one between checkouts |
| `KDOS_FETCH_HOST=1` | Do everything in one pass on this host, generating bundles with its own toolchains |

An empty file or a hash mismatch is always refused, and a corrupt cache entry is deleted. A file
whose recipe carries no hash yet, as after a `version =` bump, can only come from upstream, and a
vendor bundle with no hash is generated.

The contributor's side of this is adding a new source to the archive: `ports/publish` uploads it
and writes its index line, and an opt-in pre-push hook (`script/hooks/pre-push`, enabled with
`git config core.hooksPath script/hooks`) refuses a push whose recipes name a hash that the pushed
`ports/sources.idx` does not, or whose file, or any of whose parts, the archive does not answer for. Both are described in
[Writing ports](../05-developer/writing-ports.md#publishing-sources); the fetch flow, step by step,
is in [Developing](../05-developer/developing.md#where-sources-come-from).

### Fetching sources on a machine

An installed machine has the recipe-only tree at `/ports` and no source, and builds an update with
`kpkg`, which fetches what it needs itself. `kpkg fetch <pkg>…` takes every `sha256 =` file the
port directory and `SOURCE_DIR` do not hold, in the order `ports/fetch` uses: the archive, at the
release and asset the tree's own `sources.idx` names, then the `source =` URL that names the file
upstream. A file split into parts is fetched part by part, each part checked against its own hash.
A copy is kept in `SOURCE_DIR` only once it hashes to the recipe's `sha256 =`; any other copy is
deleted. A file no `source =` entry names, such as a vendor bundle, is also linked into the port
directory, because its `build.sh` opens it there as `$PORT_SRC/<file>` and only the extraction of a
`source =` entry looks in `SOURCE_DIR`. `KDOS_SOURCES_BASE` means what it means to `ports/fetch`: unset is the published archive,
empty skips the archive, and a directory is an archive disk laid out as `<tag>/<asset>`.

| Exit | Meaning |
|---|---|
| 0 | Every source is present |
| 1 | A source could not be had: no copy verified, or neither the archive nor a `source =` URL names it (a vendor bundle the archive lacks) |
| 3 | No copy could be reached at all: the network is down |

A build fetches on its own only with `KPKG_FETCH=1`, which
[`kdos update apply`](#updating-a-machine) sets so that a dependency a new tree adds has its sources
fetched as `kpkg install` reaches it. Without it a missing source fails the build: `make build`
runs with no network, and a build that downloaded on its own would stop being a build of the
fetched tree.

## kpkg

`kpkg` is the package manager: one C program that builds ports, installs and removes packages,
resolves dependencies, signs and verifies binary packages, and makes deltas. It is one binary
answering to five names. The bootstrap phase installs it as `/usr/bin/kpkg` with the other four
names as symbolic links, and what it does depends on the name it was run as:

| Name | Does |
|---|---|
| `kpkg` | The front end, with the commands below |
| `kpkgadd` | Install a prebuilt package file: `kpkgadd [-f] [--root <path>] <file>`; `-f` skips the [conflict scan](#who-owns-a-file) and leaves a contested path claimed by both packages |
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
| `check [--root <path>] <pkg>…` or `check [--root <path>] --all` | Compare the root with what was installed; see [Checking an installed root](#checking-an-installed-root) |
| `meta <pkg or portdir>` | Print the recipe's metadata as shell assignments, which `ports/fetch` reads |
| `fetch <pkg>…` | Download each source the port directory and `SOURCE_DIR` lack; see [Fetching sources on a machine](#fetching-sources-on-a-machine) |
| `verify <pkg>` | Build the current recipe and a candidate written beside it as `kpkgbuild.new` (with an optional `build.sh.new`), then compare the two packages |
| `verify --repro <pkg>` | Build the same recipe twice; the two packages must be byte-identical |
| `keygen <name>` | Make an Ed25519 signing key pair |
| `sign <file> <key>` | Write `<file>.sig`, one signature over any file, which `verify-pkg` checks; a release's `SHA256SUMS` is signed this way |
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
| `--keep-cache`, `KPKG_KEEP_CACHE=1` | Keep each built package in `PACKAGE_DIR` after installing it, compressed with the kept `xz -9` setting; by default it is compressed with `xz -0` and deleted once installed |
| `-f`, `--force` | Rebuild the named packages, though not their dependencies. The rebuilt package is installed through the same [conflict scan](#who-owns-a-file) as any other |
| `install --build-only <pkg>…` | Build exactly the named ports into `PACKAGE_DIR` and install nothing; see [Building apart from installing](#building-apart-from-installing) |
| `install --commit <pkg>…` | Install what `--build-only` built for each named port, in the order named |
| `KPKG_CONF` | Configuration file (default `/etc/kpkg.conf`) |
| `KPKG_KEYRING` | Trusted-key directory (default `/etc/kdos/keys`) |
| `KPKG_REQUIRE_SIG=1` | Refuse any package without a valid signature |
| `KPKG_SKIP_INDEX` | Space-separated [shared indexes](../05-developer/writing-ports.md#shared-indexes) not to rebuild after an install or removal (`man`, `schemas`, `gio`, `pixbuf`, `mime`, `fonts`, `info`, `hwdb`, `xfonts`, `texmf`, `desktop`, `icons`, `vlc`). The build's chroot phases set `man`; nothing sets it on a running system |
| `KPKG_STRICT_RECIPE=1` | Rebuild an installed package whose recipe hash differs; without it, every installed package is skipped. See [Deciding what to rebuild](#deciding-what-to-rebuild) |
| `KPKG_STORE=<dir>` | Install a port from the [package store](#the-package-store) in `<dir>` when its key is there, and store every package it builds; empty or unset, the store is off |
| `KPKG_STORE_CHECK=1` | Build every store hit anyway and log a difference; see [Check mode](#check-mode) |
| `KPKG_STORE_SALT`, `KPKG_STORE_BASE` | Two terms of the store key: the hash of what the bootstrap phases built, and the ports every key depends on whether declared or not |
| `KDOS_ALLOW_UNVERIFIED=1` | Extract a source the recipe gives no hash for; see [What a build verifies](#what-a-build-verifies) |
| `KPKG_FETCH=1` | Have every build fetch its missing sources before it verifies them, as `kpkg fetch` does; `kdos update apply` sets it, and the build never does |

An unknown option is refused rather than read as a package name.

`/etc/kpkg.conf` sets `PORT_REPO` (default `/ports/core /ports/src/system /ports/src/art
/ports/src/desktop /ports/src/daemons`), `SOURCE_DIR`
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

### Building apart from installing

`kpkg install --build-only` and `kpkg install --commit` split one install in two, so ports that do
not depend on each other can build side by side while their installs still happen one at a time.

`--build-only <pkg>…` builds each named port in the order named and resolves nothing. Every
`depends =` entry of a named port must already be installed and current; one that is not is an
error that names it and stops the run at that port: the ports named before it keep their builds
and records, and it and the ports after it are not built. A port that is itself installed and current is skipped
as in an ordinary install, unless `-f` names it. The recipe hash is taken before the build, and the
package is compressed as a normal install would: the cheap preset unless the cache is kept. On
success the build writes `PACKAGE_DIR/<name>.pending`, replaced whole through a temporary file, so
it is either absent or complete:

```text
/var/cache/kpkg/packages/foo-1.0-1.tar.xz
<the recipe hash taken at build start>
transient
```

The third line is `kept` or `transient`, the xz preset the package was packed with; lines after it
are ignored. A `--build-only` that finds a record naming an existing package built from the same
recipe hash prints `already built` and builds nothing, so a run that was interrupted resumes at no
cost. Under a kept cache the record must also say `kept`: a transient package is not the bytes
`kpkgbuild` makes, so it is built again rather than left in the cache the binhost publishes.

`--commit <pkg>…` handles the names in the order given. For each it reads the record, installs the
package it names through the same conflict scan as `kpkg install`, records the recipe hash from the record under the database's writer lock, deletes the
package unless the cache is kept and the record says `kept`, deletes the record, and prints `kpkg: committed <name>`. A name
with no record is an error that stops the run. `kpkgadd` holds the writer lock for the install
itself, so installs from any number of callers into one root never overlap.

The image phase deletes leftover `.pending` records together with the package cache.

## Packages

A package is a compressed tar archive plus a database entry.

| | |
|---|---|
| File name | `<name>-<version>-<release>.tar.xz`, parsed from the right because a name may contain hyphens |
| Compression | `xz -9 -T0 --block-size=32MiB --no-adjust` for every package that outlives its install; `xz -0 -T0 --block-size=8MiB --no-adjust` for one `kpkg install` deletes straight after installing it. See [Reproducible packages](#reproducible-packages) |
| Database entry | `/var/lib/kpkg/db/<name>`: the version and release on the first line, then the manifest |
| Manifest | Every path the package owns, one per line, `./`-prefixed, directories with a trailing slash. Each line is the name's own bytes, as `tar --quoting-style=literal` lists it, so a name outside ASCII is stored as the UTF-8 the package holds rather than as tar's octal escapes; a name holding a newline cannot be stored |
| Recipe hash | `/var/lib/kpkg/db/.recipe/<name>`, one line; see [Deciding what to rebuild](#deciding-what-to-rebuild) |
| Package-file hash | `/var/lib/kpkg/db/.pkgsha/<name>`, one line: the SHA-256 of the `.tar.xz` it was installed from, written by `kpkgadd` after the entry; see [The package store](#the-package-store) |
| File record | `/var/lib/kpkg/db/.files/<name>`, one line per regular file the install placed: `<sha256> <mode> <path>`, the mode as four octal digits and the path as the manifest spells it; written by `kpkgadd` after the entry, once the install hook has run, and read by [`kpkg check`](#checking-an-installed-root) |
| Writer lock | `/var/lib/kpkg/db/.lock`, empty; held by `kpkgadd` and `kpkgdel` while they change the tree and the database |
| Install hook | `.POSTINSTALL` at the root of the archive, when the port has a `postinstall.sh` |

A package holds what the port's build installed into `$PKG`, less what the tree's size and
language policy removes: HTML and PDF manuals, every non-English translation, and the symbol table
of an executable or shared object over 32 MiB (see
[Writing ports](../05-developer/writing-ports.md#size-and-language-policy)). `kpkg` strips no other
binary and splits off no debug package, so the compile and link flags decide a package's size. No port ships
debug information: each recipe overrides the upstream defaults that add `-g` by flag (meson's
`--buildtype=release`, Go's `-ldflags "-s -w"`, GCC's `CXXFLAGS_FOR_TARGET`, CPython's `OPT`),
preflight enforces the meson and Go flags, and `testing/debuginfo.py` fails a built tree that
carries more than 1 MiB of debug sections outside ghc's `-debug` runtimes; see [Writing ports](../05-developer/writing-ports.md#what-kpkg-does-around-buildsh).

The file name is the only metadata a package carries. The install hook is a standalone bash
script: a shebang, the recipe's metadata, then `postinstall.sh` byte for byte. `kpkgadd` lifts it
out before placing any file, so it is never installed and never in the manifest, and runs it after
the install; a hook that fails produces a warning, not a failed install.

`kpkgadd` extracts into a staging directory on the target filesystem, so placing a file is a
rename. The archive is decompressed once: the extraction runs with `--quoting-style=literal`, its
verbose listing is byte for byte what `tar -tf --quoting-style=literal` prints, and it becomes the
manifest, less `./.POSTINSTALL`. GNU tar's default quoting writes every byte outside printable
ASCII as an octal escape, and a manifest line in that form names no file on disk, so removal and
every ownership question would miss it. A file that cannot be
placed aborts the install before any database entry is written, because an entry written past a
failure would claim a complete install of a package that is half on disk. Writing to the root is permitted whenever the root is writable, which is what lets a
build install into a sysroot (a directory standing in for a target's root filesystem) it owns
without being root. `kpkgadd` creates every directory 0755, whatever mode it was packaged with:
the package is rolled root:root, and a mode that leaned on a daemon's group would lock that
daemon out. A hook that needs a directory's mode sets it.

The database has one writer at a time. `kpkgadd` and `kpkgdel` take an exclusive lock on
`.lock` in the database directory before they read who owns what, and release it after the
shared indexes are rebuilt. Two installs into one root therefore run their conflict scans,
placement and database edits one after the other, and neither decides against a table the other
is rewriting; extraction happens before the lock and overlaps. The install hook runs inside the
lock, so a hook that calls `kpkg` waits on its own installer for ever. Every command that lists
the database skips its dot-names: `.lock`, `.recipe/`, `.pkgsha/` and `.files/` are not packages.

### Who owns a file

A file conflict is between *packages*. A path that exists but that no installed package claims is
**adopted**, not refused. This is what makes the bootstrap work: the earliest build phases install
a toolchain by hand, leaving files no database entry owns, and the bootstrap then rebuilds those
packages with `kpkg`.

The scan asks the database about every staged path that is not a directory, on a first install
and on an upgrade alike, whether or not the path exists under the root. A path another package
claims is a conflict even when its file was deleted from the root, and an upgrade that adds a path
another package owns is a conflict too: asked only of paths on disk, or only on a first install,
either would be taken without a word and left claimed by two packages. Every manifest is streamed
and only the claims on the staged paths are kept, rather than sorting the million-odd paths a
desktop tree holds on every install. The package's own claims, an upgrade's old version, are never
a conflict. An upgrade's orphan sweep and a removal read ownership the same way, for the files they
are about to delete.

A path another package owns is taken only when the recipe declares it: `replaces = <package>…`
names the packages this one may take paths from (see
[Writing ports](../05-developer/writing-ports.md#keys-the-package-manager-reads)). `kpkgadd` reads the line from
the recipe of the same name in the ports tree, so a package installed with no recipe beside it
declares nothing. A declared path changes hands: the file is placed, the path is removed from the
previous owner's manifest, and `kpkgadd` prints `Taking <n> path(s) from <package>`. Without that,
removing the older package would delete a file the newer one installed. The declaration runs one
way: the package a path was taken from, installed again while its taker holds the path, is refused
like any other conflict, so a taker names that package in `depends` too and installs after it. Any
other claim refuses the install before a file is placed:

```text
ERROR: File conflict detected:
  usr/bin/foo (owned by bar)
  baz may take a path from a package only when its recipe names that package in `replaces =`
```

The refusal holds for `kpkg install -f` and for the build, which installs every package through
this scan. `kpkgadd -f` alone skips it, placing the files over whatever claims them and leaving each
contested path claimed by both packages; nothing in the build passes it. Of several claimants of one
path, the one every ownership question names (the conflict scan, `kdos why`, `kpkg check`'s
unowned walk) is the first by package name in byte order, whichever C library `kpkg` was built
against and whether the whole table was read or only the claims on the staged paths.

Ownership is compared on the canonical path. The root's `bin`, `sbin`, `lib` and `lib64` are
links into `/usr`, so a package built with `--exec-prefix=` records `./bin/free` for the same file
another records as `./usr/bin/free`. Compared as strings the two never collide: the scan passes,
the file is claimed twice, and removing or upgrading either package deletes the other's file.
`kpkg` reads which top-level names are such links from the root it installs into, and folds each
path through them before comparing.

### The base userland

The command line is GNU's, and every name has one owner. `coreutils` installs each of its programs
as a binary of its own, with no multi-call binary; `grep`, `sed`, `gawk` (which `/usr/bin/awk`
names), `findutils`, `diffutils`, `patch`, `tar`, `gzip`, `cpio` and `which` are the GNU packages of
those names. `musl-utils` builds the `getconf`, `getent` and `iconv` programs musl's own source
does not carry, from Alpine's musl package; `util-linux`, `procps-ng`, `iproute2` and `iputils`
supply the system tools, and `sysvinit` supplies PID 1 and the commands that talk to it (see
[Boot and init](boot-and-init.md#rcs-and-the-service-scripts)). `mkpasswd`, which hashes a
password for `/etc/shadow` or `useradd -p`, comes from the `whois` port, whose source carries it.
Nothing on the image is a multi-call userland.

`coreutils`' configure line carries four constraints:

- `fu_cv_sys_stat_statvfs=n` makes `stat -f` and `df` read `statfs` instead of `statvfs`. musl's
  `statvfs` keeps only 32 bits of `f_fsid`, so without it `stat -f -c %i` prints a filesystem id
  whose high half is zero, which matches no other tool's id for the same filesystem.
- `--without-openssl` keeps libcrypto out of the base set. configure links it into every program,
  not only the checksum ones, whenever it finds it.
- `--enable-xattr` is what lets `cp -a`, `cp --preserve=xattr` and `mv` across filesystems carry
  extended attributes; without it they copy the data and drop the attributes with no message.
  `--enable-acl` and `--enable-libcap` do the same for ACLs and for `ls`'s view of file
  capabilities.
- `CPPFLAGS=-I/usr/include/utmps` and `LIBS="-lutmps -lskarnet"` build `who`, `users` and `pinky`
  against utmps, so they read the login records `utmpd` keeps; musl's own utmp functions are stubs
  that report no one. `util-linux`, `procps-ng`, `shadow` and `sysvinit` are built against utmps
  the same way, and every reader and writer must be, because utmps' record layout is not musl's
  (see [Login records](boot-and-init.md#login-records)). `procps-ng` also links with
  `-Wl,--no-as-needed`: `uptime` and `top` count users through `libproc2`, and a program that
  does not name `libutmps` itself loads musl ahead of it, so the user count would come from musl's
  stubs.

`--enable-install-program=arch,hostname` adds the two programs coreutils leaves out by default, and
`--with-libgmp` stops configure when gmp is missing instead of building `expr` and `factor` against
gnulib's smaller copy. `FORCE_UNSAFE_CONFIGURE=1` is there because `kpkg` builds as root, which
coreutils' configure otherwise refuses.

### Upgrades and removals

An upgrade removes orphans. A file present in the old version and absent from the new one is
removed rather than left on disk owned by nothing, unless another package claims it, in which case
it is that package's file and stays. "Absent" is judged on the canonical path, so `./bin/x` in the
old version and `./usr/bin/x` in the new are one file.

A removal walks the manifest in reverse, so a directory is reached only after everything inside it;
a directory that still holds another package's files survives. A file another installed package
claims is left in place. `kpkgdel` does not check reverse dependencies: `kpkgdel bash` removes
bash. The entry's sidecars in `.recipe/`, `.pkgsha/` and `.files/` go with it. A name that is not installed
is reported and skipped, and the rest of the command line is still processed.

An install or removal ends by rebuilding the shared indexes its manifest fed, from everything then
on disk: the GSettings schemas, the GIO module and pixbuf loader caches, the MIME database, the
desktop entries' `mimeinfo.cache`, the font cache and the X core fonts' `fonts.dir`, the info
directory, the TeX `ls-R` files, the udev hardware database and the `icon-theme.cache` of each icon
theme it touched, or of every theme when the package carries `gtk-update-icon-cache` itself, and
VLC's plugin cache, `plugins.dat`. The
manual index is merged instead: an install that only adds pages adds them to `mandoc.db`, and the
index is rebuilt from the whole tree only when a page was removed (by a removal, or as an upgrade's
orphan) or when there is no `mandoc.db` yet. A tool that is not installed yet is skipped, and a
tool that fails produces a warning, because the package is already on disk. `KPKG_SKIP_INDEX`
names indexes not to rebuild, space-separated (`man`, `schemas`, `fonts`, and so on); each one
skipped prints `index skipped: <name>`. The build's chroot phases set it to `man`, because
`70_image` writes every `mandoc.db` from scratch over the finished tree; a running system never
sets it. No package owns these
files, so no package ships them; `kpkgbuild` deletes the info `dir` file and each `fonts.dir` from
the staged tree for that reason. The list is in
[Writing ports](../05-developer/writing-ports.md#shared-indexes).

### Checking an installed root

`kpkg check <package>…` compares the root with what was installed, and `kpkg check --all` does so
for every installed package; `--root <path>` checks another root, such as an A/B slot. Each package's
manifest is walked against the root and against its file record in `.files/<name>`, the SHA-256 and
mode `kpkgadd` took from every regular file it placed. Each finding is one line:

| Line | Means |
|---|---|
| `<package>: missing <path>` | Nothing is at a path the manifest lists, or a directory entry is not a directory |
| `<package>: changed <path>` | A recorded file outside `/etc` whose bytes do not hash to its record, or that is not a regular file |
| `<package>: mode <path> <now>, installed <then>` | A recorded file outside `/etc` whose permission bits differ |
| `<package>: config <path>` | A recorded file under `/etc` that is changed in the same sense: configuration someone edited |
| `<package>: config mode <path> <now>, installed <then>` | A recorded file under `/etc` whose permission bits differ |
| `unowned <path>` | With `--all`: a file or link under `/usr` or `/opt` that no package claims |

A summary line follows, counting each kind. The exit status is 1 when anything is missing, changed
or has a changed mode, 0 otherwise, and 2 for a usage error or a name that is not installed. A
`config` line and an unowned file do not change the status: a package's file under `/etc` is a
default the machine is meant to edit, so a check that failed on it would fail every configured
machine; the skeleton the build copies from `fs/` and anything installed by hand under `/usr/local`
belong to no package by design. A file under `/etc` that is gone is still `missing`, and still
fails. The indexes `kpkg` writes itself (listed under
[Shared indexes](../05-developer/writing-ports.md#shared-indexes)) and Python's `__pycache__`
bytecode are not reported, and one a package ships that a trigger then rewrites, as `mktexlsr` does
`texlive`'s `ls-R` files, is checked for presence only.

A path is checked as the manifest spells it, so `./bin/x` on a merged-`/usr` root is checked through
the root's own `bin` link, and a file claimed under either spelling is owned. A path another package
took leaves the old owner's manifest, so it is checked only against its new owner's record. A
symbolic link, device node or FIFO is checked for presence only, and so is every file of a package
installed with no file record. The record is taken once the install hook has run, so a mode or a
file the hook sets is what is checked.

A freshly built image checks with no `missing`, `changed` or `mode` line and exits 0. The image
leaves out the build tree's logs and caches, but recreates every directory a package claims under
`/var/log` and `/var/cache`, empty and with the tree's mode and owner
([the live medium](boot-and-init.md#the-live-medium-and-persistence)); a package that installed a
file there, rather than a directory, would be `missing` on the image.

`kdos update apply` runs `kpkg check --all` on an A/B slot it installed into, before it puts the
slot's kernel on the ESP and marks the slot to be tried; a slot that fails the check is not tried,
and one whose only findings are `config` lines is.
See [Updating a machine](#updating-a-machine).

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
recipe unpacks it itself. Extraction runs as root with `--no-same-permissions` under a umask of
022, so no mode in an archive survives with group or world write: root's `tar` otherwise restores
every mode an archive carries, a tarball of `-rw-rw-rw-` files (gimp-help's is one) becomes a tree
of them, and `make install` copies those modes into the package. A tarball a recipe unpacks itself
is outside this rule; the [package lint](#the-package-lint) still refuses what it would ship.
Extraction also passes `--warning=no-unknown-keyword`: a tarball made by `git archive` or bsdtar
carries pax keywords GNU tar does not know, and each would otherwise be a line in the build log
about nothing that changes a file.

Between extraction and `build.sh`, every `config.guess` and `config.sub` under `$SRC_ROOT` whose
`timestamp='YYYY-MM-DD'` line is older than the copy automake installs (`/usr/share/automake-*/`)
is overwritten with automake's, keeping its mode and adding the execute bits. An autotools release
cut before musl was known to these scripts guesses the host as `x86_64-pc-linux-gnu`, and a
configure that trusts the guess builds for a C library the tree does not have. A copy newer than
automake's, or one with no `timestamp=` line, is left as it is, and so is every file inside a
vendored Rust crate: a directory holding `.cargo-checksum.json` records each file's hash there, and
cargo refuses the build when one differs. Before automake is installed, nothing is replaced. The refresh happens before `build.sh`, so a recipe that regenerates its build
system with `autoreconf -fi` gets automake's copies either way.

Before rolling the package, `kpkgbuild` deletes every libtool `.la` file, because each names
build-time paths that do not exist on the target. It copies the licence texts: every regular file
at the top of `$SRC` whose name starts, in any case, with `LICENSE`, `LICENCE`, `COPYING`,
`COPYRIGHT` or `NOTICE` goes to `usr/share/licenses/<name>/`, and the files of a directory of such a
name (REUSE's `LICENSES/`) to a subdirectory there. Source files named like a licence
(`license.c`, `COPYING.in`) are skipped, a symlink is followed only into the source tree or
`/usr/share`, and whatever `build.sh` already put at a target name, file or symlink, is kept and
never written through. The recipe's `license =` key states the SPDX expression the texts belong to, and
`kpkg info` prints it. It then applies the
[size and language policy](../05-developer/writing-ports.md#size-and-language-policy): HTML and PDF
manuals under `usr/share/doc` and `usr/share/texmf-dist/doc` (except the HTML a program opens
itself: mednafen's manual, the book, R's help index and style sheet, and the in-app manuals and
About pages of geany, gimagereader, texstudio, wsjtx, Wireshark, hatari, FreeCAD and SMPlayer),
every non-English message catalog, Qt translation, KDE handbook and manual-page directory, and
every directory that leaves empty are removed, and `objcopy --strip-all --keep-section=.debug_*`
takes `.symtab` and `.strtab` out of each ELF executable or shared object over 32 MiB that
carries nothing after its sections. `usr/share/licenses` is never pruned. These steps run on `$PKG` before the archive and its
manifest exist, so a removed path is never owned by the package. It then deletes every
`*.dist-info/direct_url.json`, which records the work directory pip installed from, and the
`__pycache__` directories under the bin directories, whose scripts are run and never imported. It
compiles every `.py` the package ships outside the bin directories, `etc` and `var` into
checked-hash bytecode in the package (`python3 -m compileall -f --invalidation-mode checked-hash`,
recording the installed path rather than the work directory). The bytecode is then in the package
and its manifest: `/usr` is read-only to a running program, so a module without it is compiled on
every import and the cache is never written. A package that ships an interpreter's own library
(`python3`) is left as its install compiled it, and before `python3` is installed nothing is
compiled. Last, the [package lint](#the-package-lint) reads the staged tree.

The package is written as `<name>-<version>-<release>.tar.xz.part` and renamed to its final name
only when the archive is complete; a failed or interrupted roll leaves no file under a package
name. `kpkg install` and `kpkg verify` install or compare the exact file that build reports having
written, never a file found in `PACKAGE_DIR` by name: with the cache kept, a search for `foo-`
also matches `foo-bar-1.0-1.tar.xz`.

Before it touches the work directory, `kpkgbuild` hashes every file a `sha256 =` entry names
that is present beside the recipe or in `kpkg`'s source directory (`SOURCE_DIR`, default
`/var/cache/kpkg/sources`), and refuses on any mismatch. This is the only time a source's bytes are
read to be hashed; extraction looks in the same two places in the same order, so the file it
unpacks is the file that was checked.

That is wider than the `source =` list on purpose, because of vendor bundles (see [Where sources
come from](#where-sources-come-from)). 164 ports carry a vendor bundle, most of them Go, Rust,
Python or Haskell programs, with pdfium among the rest, and each unpacks it in `build.sh` itself. A
vendor bundle is declared with a hash but named by no `source =` line, so a check that walked
`source =` alone would compile those ports from bytes nothing had looked at.

A declared file that is in neither place is skipped rather than refused. A source the build needs
is caught when extraction cannot find it, and failing on a declared file the build never opens
would refuse a port over a hash that cannot affect it.

Extraction then checks that every source it unpacks is declared. A source the recipe names with no
`sha256 =` for it is a hard failure, not a warning. `KDOS_ALLOW_UNVERIFIED=1` is the escape hatch
for bringing up a new port before its hash is known, and `testing/preflight.sh` checks that no
recipe in the tree needs it.

The first `source =` entry is looked for under the standard name `<name>-<version>.<ext>` (for
example `zlib-1.3.2.tar.gz`), which is the name `ports/fetch` saves it under; later entries keep
their URL's basename, and `file::url` names a file explicitly.

### The package lint

The last step before the roll reads the staged tree (`src/system/kdos-kpkg/lint.c`). Every rule
below is a fact about the files a user's machine receives, so a failure refuses the package: the
build prints each failing path with `ERROR: lint:` and a reason, and writes no package file. Paths
are read with the merged-`/usr` aliases folded, so `bin/x` is checked as `usr/bin/x`.

| Rule | Fails | Allowed |
|---|---|---|
| Layout | A payload path outside `bin`, `boot`, `etc`, `lib`, `sbin`, `usr` and `var`; or under `usr/lib64`, `lib64`, `usr/etc`, `usr/var`, `usr/doc`, `usr/com`, `usr/lib/systemd`, `lib/systemd`, `etc/grub.d` or `etc/bash_completion.d` | `etc/X11`, `usr/local`, `run` and `var/run` in a `kdos-*` package only |
| Readable | A regular file without `o+r` that is neither setuid nor setgid | `etc/shadow*`, `etc/gshadow*`, `etc/sudoers`, `etc/sudoers.dist`, `etc/sudoers.d/*`, `etc/ppp/*-secrets`, `etc/cups/*.conf*`, `usr/lib/cups/backend/*`, `etc/freeipmi/*.conf`, `etc/fwupd/fwupd.conf`, `etc/mumble/mumble-server.ini`, `etc/skel/.msmtprc`, `etc/skel/.mbsyncrc` |
| Executable | A regular file under `usr/bin` or `usr/sbin` with no `x` bit | Modules and data upstream installs beside its programs: icestorm's `icebox.py` and `iceboxdb.py`, gmsh's `onelab.py`, brltty's `brltty-prologue.*` and `brltty-config.sh`, sdcc's `sdcdb*.el`, wsjtx's `ALLCALL7.TXT` and `sounds/` |
| World-writable | A path with `o+w` and no sticky bit | `var/games/tetris-bsd.scores`, 0666: the game is not setgid, so a score file it cannot write as any player saves no score |
| Patch leftovers | A `*.orig` or `*.rej` file | `Cargo.toml.orig` and `package.json.orig`, which are upstream content; Cargo lists the first in each vendored crate's checksum file |
| pkg-config | A `.pc` with no or an empty `Version`; `/usr/local` or `/var/cache/kpkg` anywhere in it; `--sort-common`, `-z,now` or `pack-relative-relocs` (the tree's `LDFLAGS`) in a `Libs` line; a module `pkgconf --cflags --libs` cannot resolve | A `-f*prefix-map=/var/cache/kpkg/work=` flag, which maps the work directory away |
| ELF | An x86-64 executable or shared object with an executable stack, a `TEXTREL`, or an `RPATH`/`RUNPATH` entry under `/var/cache`, `/workspace` or `/tmp`, or neither absolute nor `$ORIGIN`-based; any ELF file naming `/usr/etc` or `/usr/var` as a path | Objects under a `testdata/` directory; `whereis` (a search-list entry), `useradd` (the `--usr-skel` default) and `orca-slicer` (a path on the printer) for the `/usr/etc` string |

`usr/lib64` and `lib64` are refused because the database folds only top-level aliases
(`kp_canon_load`, [Who owns a file](#who-owns-a-file)): with `/usr/lib64` a link to `lib`,
`./usr/lib64/x` and `./usr/lib/x` would be two ownership keys for one file, and neither install
would see the other's claim.

The pkg-config rule resolves every module the package ships with the package's own `pkgconfig`
directories searched first, as uid 65534 when `kpkg` runs as root, so a module whose file or whose
`Requires` is unreadable to an ordinary user fails here rather than in that user's build. A work
directory uid 65534 cannot reach is resolved as root instead. Before `pkgconf` is installed, no
module is resolved.

Four findings warn and do not fail, each printed once per build with a count and the first five
paths: a group-writable path outside setgid paths and `/var/games`, an object without
`BIND_NOW`, a plain `/usr/lib` `RPATH` entry, and an empty `RPATH` component, which musl's loader
skips. [Writing ports](../05-developer/writing-ports.md#what-the-package-lint-refuses) says what
a recipe does about each.

## Deciding what to rebuild

The build must not recompile some 2,000 ports on every run, and must not skip one whose recipe changed.
Two hashes decide, and they are the same two the binary host uses.

### `E:` — the recipe hash

The recipe hash is SHA-256 over the `kpkgbuild` file, `build.sh`, `postinstall.sh` and every
`.patch` file, sorted by name, each contributing its name, its length *and* its bytes. The length
stops two files from hashing the same as one by moving the boundary between them. The result is an
exact statement of what a package was built from.

For a port that names a `source =`, every other file in the directory, subdirectories included, is
hashed after the recipe files, in the same sorted name-length-bytes form, except the files a
`sha256 =` line names. A tarball or a vendor bundle is covered by its own `sha256 =` line, which
`kpkgbuild` checks before it builds anything (see [What a build verifies](#what-a-build-verifies)),
so hashing it here as well would only change the key for nothing. A port holding only its recipe
files and its named sources hashes exactly as its recipe files alone.

The files this reaches are the ones `build.sh` reads from `$PORT_SRC` and nothing else vouches
for. `linux` starts its kernel configuration from `kdos.config`, signs modules under the identity in
`x509.genkey` and copies in its panic-screen logo this way, and `epy` installs a `.desktop` file; editing one rebuilds the port. A stray file beside a recipe,
such as an editor backup, a `kpkgbuild.new` or a fetched tarball no `sha256 =` line names any
more, also changes the hash and rebuilds the port; `make fetch` removes the last kind when it is
a hard link into `ports/.srccache` (see
[Archives preflight accepts](../05-developer/writing-ports.md#archives-preflight-accepts)). A
directory named `__pycache__` is skipped at every depth: it is bytecode Python writes beside a
module it imports, and a port that links in a tree of scripts, as `kdos-kfs` links
`script/book`, would otherwise hash whatever a host run happened to import.

The one file never hashed is `kfs.md`, the port's note in the KDOS from Scratch book under
`docs/kfs`. The port's own build never reads it, so adding, editing or removing it leaves the hash, and the installed
package, as they were; this holds for a port with or without a `source =`. It is skipped only
directly beside the recipe: a file of that name in a subdirectory, such as a fork's `src/`, is
hashed like any other. The note still changes the book's text, so the book keeps a stamp of its
own, `docs/kfs/data/recipes.stamp`: `script/book/kfsgen.py` takes every port's recipe hash by this
rule, which must change with `kp_hash.c`'s, and then hashes every `kfs.md`, the phase lists and
every other file the book reads that no recipe hash covers. A changed note stales the stamp,
`testing/kfscheck.py` fails until `make book-stamp` rewrites it, and the rewritten stamp under
`docs/kfs` changes the recipe hash of `kdos-kfs`, the port that ships the book, and of no other.

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

The hash is taken from the port directory when the build starts, and recorded in
`/var/lib/kpkg/db/.recipe/<name>` after a successful install, never before. A record written ahead
of a build that then fails would claim a recipe is installed that is not. Because the hash is of the
recipe the build started from, an edit made to the port while it builds leaves a record that does
not match the edited port, and the next strict run rebuilds it. It is a separate file rather than a
field of the database entry, so the entry keeps the fixed shape given under [Packages](#packages):
the version and release on the first line, then the manifest.

The dependency solver applies the check, not the install loop. An installed and current package is
dropped before the loop runs, so a check placed later would reach only packages named on the
command line and miss every *dependency* whose recipe changed.

The build orchestrator asks the same question on the host before it enters the chroot. A package
phase's order is resolved against an empty database, so it names every port in the phase's closure;
kdosbuild calls the same `kp_installed_current()` for each, over `build/fs/var/lib/kpkg/db` and the
phase's repositories as the host sees them, once when the phase's order is resolved and again when
the run reaches the port, and a port that is still installed and current then never becomes a
`kpkg install` step. A port the build plan forces, and one the host cannot find, still run, and
`kpkg` applies the check again to every step that does. See
[How a phase runs](../05-developer/build-system.md#how-a-phase-runs).

## The package store

A port whose recipe, environment and dependencies are exactly those of a package built before is
installed from that package instead of being built. The store is `build/pkgstore`, it is off by
default, and `make build KDOS_PKG_STORE=1` turns it on (`check` builds anyway and compares; see
below). Inside `kpkg` it is `KPKG_STORE=<dir>`, and it applies to `kpkg install` and to
`--build-only`, after the installed-and-current skip and before the build. `-f` never reads it.

```text
build/pkgstore/<key[0:2]>/<key>/
    <name>-<version>-<release>.tar.xz
    META        P: V: R: F: C:<sha256 of the file>, one X:<name> <pkgsha> per undeclared link
```

**The key** is SHA-256 over `field=value` lines:

| Field | Value |
|---|---|
| `format` | `KP_STORE_FORMAT` in `kpkg.h`, raised whenever `build.c`, `prune.c` or `lint.c` changes what a package holds |
| `recipe` | The port's recipe hash, as `.recipe/` records it |
| `pack` | `kept` or `transient`, the xz setting the package carries |
| `salt` | `KPKG_STORE_SALT`: kdosbuild hashes `script/phases/00_cross`, `script/phases/10_bootstrap`, `script/lib`, `script/env`, the recipe hash of every port those scripts build, and `fs/etc/passwd`, `group` and `ld-musl-x86_64.path`. With no salt the key is unknown, and every port builds without storing |
| `env` | The sorted environment, less job counts (`MAKEFLAGS`, `NINJAFLAGS`, `KDOS_JOBS`, `CMAKE_BUILD_PARALLEL_LEVEL`, `CARGO_BUILD_JOBS`), the compiler cache (`KDOS_CCACHE`, `CCACHE_*`, the CMake launchers), every `KPKG_*`, the packaging and store switches, `KDOS_PHASE_*`, `KDOS_SNAPSHOT_*`, `MARK`, `CHROOT`, `PORT_REPO`, `TERM`, `PWD`, `OLDPWD`, `SHLVL` and `_` |
| `dep` | `<name> <pkgsha>` for every port in the declared transitive closure and in `KPKG_STORE_BASE` (set in `script/env/chroot.env`: the toolchain and base userland), sorted; `-` for one not installed |

`<pkgsha>` is the `.pkgsha/` record: the hash of the package *file*, so a dependency rebuilt into
different bytes changes the key of everything above it. An installed member with no `.pkgsha` makes
the key unknown: the port builds and nothing is stored. The file hash depends on the xz setting, so
a build that keeps its packages (`KDOS_MAKE_BINHOST=1`) and one that does not key every dependent
port differently; that costs misses, never a wrong hit.

A hit prints `Reusing <pkg> from the store (<key12>)`, installs the stored file, records the recipe
hash, and marks the entry used. A miss builds, and once the package is installed it is stored under
the key computed *before* the build. Each file is written under `.tmp.<pid>` and renamed, `META`
last; a directory without `META` is not an entry. A stored file whose hash differs from its `C:` is a
miss. No store failure fails an install.

**`X:` lines** cover what the key cannot see. After the install `kpkg` reads each ELF file the package
owns (its `DT_NEEDED`, with no process per file), resolves each library in `usr/lib`, `lib` and
`usr/local/lib`, and looks up the owner. An owner that is not the port and not in its closure is an
undeclared link: `<pkg> links <owner> without declaring it` is printed and the entry gets
`X:<owner> <pkgsha>`, which a lookup requires to match what is installed. They cannot see a library
opened with `dlopen`, a header-only or static dependency, a tool the build ran, or a feature a
configure script switched on because it found something; see
[Known gaps](../06-reference/known-gaps.md#the-package-store-keys-only-what-a-port-declares).

### Check mode

`KDOS_PKG_STORE=check` (`KPKG_STORE_CHECK=1`) builds every hit anyway and compares the result with
the stored file. A difference appends `<pkg> <key12> stored=<sha> built=<sha>` and the first
members that differ to `build/logs/pkgstore-check.log`, and the entry is replaced; the install never
fails. A port that shows up there is not reproducible, or reads an input the key does not cover.

### Why the binhost's identity is not reused

A binhost entry is matched on architecture, build-config hash and recipe hash
([The binary host](#the-binary-host)). That is enough for a finished system installing a finished
package; it has no dependency term, so during a build it would install a package linked against a
library that has since changed. The store key names every dependency's exact bytes instead.

### Eviction

`kpkg store gc <dir> <max-size>` deletes entries, least recently used first by `META`'s
modification time, until the store fits. `70_image`'s `015_pkgstore.sh` runs it when the store is on,
with `KDOS_PKG_STORE_MAX` (default `60G`).

## Reproducible packages

A package built twice from the same tree is byte-identical. That is a property of one function, the
archive roller inside `kpkg`, rather than of some 2,000 recipes, which is why `kpkg` rolls the archive
itself instead of letting each `build.sh` do it.

Each setting removes one source of difference between two builds:

| Setting | Without it |
|---|---|
| `--sort=name` | Directory order is filesystem order, which is not stable even between two copies of the same tree |
| `--mtime=@$SOURCE_DATE_EPOCH` | Every file carries the second it was installed; with the variable unset, `kpkg` uses 0 |
| `--owner=0 --group=0 --numeric-owner` | The builder's user id, and its *name* as text in the header |
| `--format=gnu` | Extended (pax) headers carry access and change times, which are wall clock; plain ustar cannot hold a path over 255 bytes, which some ports have |
| `--use-compress-program=xz -9 -T0 --block-size=32MiB --no-adjust` | The compressed bytes depend on the preset and the block size, which are pinned; the thread count does not reach them |
| `XZ_OPT` and `XZ_DEFAULTS` held out of the environment while `xz` runs | `-e` or `--check` in the environment changes the bytes, and a memory limit would lower the dictionary; every port's `build.sh`, including the next one in the same `kpkg install`, still sees both |
| `umask(022)` before the build | A file created without an explicit mode takes the builder's umask: the one source of drift that is not in the archive call |

Multi-threaded `xz` is deterministic: any `-T` of one or more in its multi-threaded mode writes the
same bytes on one core or sixteen, which the self-test checks by recompressing a package with
`xz -9 -T+1 --block-size=32MiB` and comparing. `-T1` is a different encoder, the single-threaded
mode, with different output, and is never used for a package. `--no-adjust` turns a memory limit
that would lower the dictionary into a failure instead of a different package. Both `xz` builds in
the tree are built with threads, which the multi-threaded mode needs.

Only a package that nothing reads after its install gets the cheap `-0` preset: `kpkg install`
without a kept cache, which deletes the file once `kpkgadd` has read it. `kpkgbuild` on its own,
`kpkg install --keep-cache` (so every package of `make build KDOS_MAKE_BINHOST=1`), `kpkg verify
--repro` and `kpkg apply-delta` all use the kept setting, so every package that can be published,
compared or rebuilt from a delta has the same bytes. A change to the kept setting changes every
package's hash, so a published binhost and its deltas are regenerated together. `kpkgbuild`
reports the time each package took to roll as `Packaged <name>: <N> MB in <s> s`, the size being
the staged tree before compression.

The other half is five settings in `script/env/common.env`, which every phase's environment sources:

| Line | Purpose |
|---|---|
| `SOURCE_DATE_EPOCH=1735689600` | A pinned epoch, not the current date and not derived from git: the build container mounts `build`, `src`, `fs`, `script`, `ports`, `docs`, `testing`, the `Makefile` and the `Dockerfile`, never the repository's `.git` |
| `TZ=UTC` | Dates formatted during the build do not depend on the builder's zone |
| `LC_ALL=C` | Sorting and formatting do not depend on the builder's locale. The chroot phases replace it with `C.UTF-8` in `script/env/chroot.env`; musl collates every locale byte by byte and formats dates in `C.UTF-8` as in `C`, so the output is the same |
| `-ffile-prefix-map=/var/cache/kpkg/work=/build`, on `CFLAGS` and `CXXFLAGS`, and on `FFLAGS` and `FCFLAGS` where the phase sets them | Rewrites the build directory out of `__FILE__` and debug paths |
| `-Wl,--build-id=sha1` | The build identifier is a function of the contents, not random |

The compiler cache never changes a byte. CMake ports in the chroot compile through ccache (see
[The compiler cache](../05-developer/how-kdos-is-built.md#the-compiler-cache)), and three settings
in `script/env/chroot.env` make a cached object the one a compile would write: no `base_dir`, which
would rewrite paths past `-ffile-prefix-map`; a compiler check that is a string of the compiler
ports' recipe hashes, gcc's version and the epoch, never a modification time kpkg has pinned; and
the compiler launched through CMake's launcher variables rather than a masquerade directory, whose
path CMake would record in shipped files. `testing/preflight.sh` fails when any of them changes.

`ports/fetch` rolls vendor bundles with the same tar settings and its own pinned `xz -9 -T1`, so
regenerating a bundle reproduces the file its `sha256 =` names; that compressor is part of every
vendor bundle's hash and is not the package compressor.

The kernel takes four more values from the builder unless told otherwise, and `linux`'s
`build.sh` pins them: `KBUILD_BUILD_TIMESTAMP` (`SOURCE_DATE_EPOCH` written as a date),
`KBUILD_BUILD_USER` and `KBUILD_BUILD_HOST` (both `kdos`) and `KBUILD_BUILD_VERSION` (`1`). Left
unset, `uname -v` and the boot banner carry the build container's hostname and the second the build
ran. Its module-signing key is pinned too, in `build/keys/` (see
[Module signing](security-model.md#module-signing)). The `linux` package is still the one exception
to byte-identical builds: an ECDSA signature uses a fresh random number each time it is made, so
every module's appended signature differs between two builds of the same tree.

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
no USE flags, so the whole question reduces to these equality tests. `kdos update` does not use a
binhost: it builds every update from source, and `kpkg binhost` is a tool to run by hand;
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

`kdos update` is the command a person types. It fetches a signed recipe tree and drives `kpkg` and
the tool that manages the A/B root slots (an installed disk's two root filesystems, one running and
one receiving updates; see [Boot and init](boot-and-init.md#ab-slot-selection)). There is no binary
package host: every update is built from source on the machine.

| Command | Does |
|---|---|
| `kdos update check` | Compare what is installed with what the ports tree on this machine pins, and report the last problem; exit 1 when something is behind |
| `kdos update fetch` | Replace `/ports` with the newest signed tree |
| `kdos update apply` | Fetch the sources of what is behind, then build and install it |
| `kdos update theme` | Re-run the theme generators for your home directory after an artwork upgrade |

New versions arrive with the ports tree, because a recipe pins its version. Every image carries
the recipe-only tree at `/ports`, without the source archives; see
[How KDOS is built](../05-developer/how-kdos-is-built.md#packaging-70_image). A newer one arrives
over the network: `ports/publish --release` puts the tree of the release's commit beside the ISO as
`kdos-ports.tar.xz`, with a `TREE` stamp naming the commit and its time, signed with the release key
as `kdos-ports.tar.xz.sig`. `kdos update fetch` downloads both from the latest release, verifies
the signature against `/etc/kdos/keys/tree`, which holds the public half of the release key the
image was built with, and only then unpacks the tree and exchanges it with `/ports`. A tree older
than the installed one is refused.

`kdos update apply` then fetches every source of every package that is behind with
[`kpkg fetch`](#fetching-sources-on-a-machine), stopping before any build when one cannot be had,
and builds each package with `kpkg install -f`. An update therefore needs the network for the tree and
for the sources, and compiles on the machine, for hours on a large set. On a machine with A/B root
slots, `apply` installs into the inactive slot and marks it to be tried on the next boot, so the
running system is untouched and a bad update rolls back. Before the slot is marked, `apply` runs
[`kpkg check --all`](#checking-an-installed-root) on it, and a missing file, or a changed one
outside `/etc`, leaves the slot untried; it then copies `/ports` into the slot, so the slot boots with the recipes it was built
from. If that slot is not mounted, `apply` refuses; `--in-place` makes it update the running root
instead, which gives up the rollback.

What went wrong is recorded in `/var/lib/kdos/update.state` (no tree, no network, a fetch or a build
that failed), and `kdos update check` reports it, as does the panel's update badge through the
nightly `kdos update check --fetch --out /var/lib/kdos/update.json`. Every option is in
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

Five details each change the answer:

- Alpine's packaging revision is stripped before comparing. Leaving it on makes every version look
  old.
- "Fixed in 0" means never affected in that branch, and falls out of the comparison, since nothing
  is older than 0.
- Each CVE is judged on its own, against the branch the version is on (its first two fields,
  `3.6` of `3.6.7`). Alpine records a CVE once per branch that shipped the fix, so mbedtls 3.6.7
  appears behind the 4.1.1 row for holes its own 3.6.7 row closed. A CVE is closed when a row on
  the version's branch fixed it at or below the version, or when the version is at or above every
  row naming it. An open CVE's fix is the lowest row above the version on its branch, or the lowest
  above it on any branch when its own has none; the newest of those is the one reported, because
  it closes every open one, and the open identifiers from all rows are merged.
- A row Alpine recorded under a version upstream never released is corrected or dropped as the
  table is written, from `secdb/corrections.txt` beside it: such a row is above every real version
  and would flag every pin for good. aerc's `1.21.0` is 0.21.0, and vorbis-tools' `9.54` is 1.4.3.
- A `secdb =` key in a recipe maps a port whose name differs from Alpine's.

A package the database does not carry is reported as UNKNOWN, never as clean. A large part of the
tree is in that state and the summary says so. A fix Alpine never shipped is invisible to the check.
The database's age is printed with every run, with a warning once it is more than 180 days old.

### Bundled copies

A library compiled into a program from a vendored crate has a version of its own that no package
records. The case in the tree is AWS-LC, the TLS library under rustls' default crypto provider,
which `aws-lc-sys` builds into the binary. A port whose crate offers a TLS backend on the system
OpenSSL takes it instead: `xh` builds with `native-tls`, and its `build.sh` stops unless the binary
NEEDs `libssl.so`. The rest offer no such feature, and switching would mean patching their manifests
and regenerating their vendor bundles, so their copies are listed in an inventory instead:
`src/system/kdos-tools/secdb/bundled.txt`, installed as `/usr/share/kdos/bundled.txt`, one row
`<port> <crate> <component> <version>` per copy.

| Port | Crate | AWS-LC |
|---|---|---|
| `iamb` | `aws-lc-sys` 0.45.0 | 5.7.0 |
| `termusic` | `aws-lc-sys` 0.38.0 | 1.69.0 |
| `chess-tui` | `aws-lc-sys` 0.40.0 | 1.72.0 |
| `mqttui` | `aws-lc-sys` 0.44.0 | 5.5.0 |
| `surfer` | `aws-lc-sys` 0.39.0 | 1.71.0 |
| `sniffnet` | `aws-lc-sys` 0.43.0 | 5.2.0 |

`kdos cve` checks each row whose port it checks under the component's name, reports a version
behind a recorded fix as `<port>/<component>`, and lists every row under *bundled copies*. The
version is the one the vendored `aws-lc/include/openssl/base.h` states
(`AWSLC_VERSION_NUMBER_STRING`), so a bump that regenerates a port's vendor bundle rewrites its row
in the same change; a stale row reports the old copy's version. Alpine carries no `aws-lc` package,
so today every row is listed as not in the database.

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
