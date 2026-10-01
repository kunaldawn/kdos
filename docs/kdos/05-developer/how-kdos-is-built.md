# How KDOS is built

This chapter tells the story of a KDOS build once, from start to finish: from `git clone` on your
machine to a bootable `build/iso-build/kdos.iso`. It is written for a reader who has never built a
distribution and wants to understand what happens, in what order, and why each stage exists before
opening the chapters that document each part in detail. It assumes nothing beyond familiarity with
compiling a program from source. [Why KDOS](../01-philosophy/why-kdos.md) says what the system is,
and [How KDOS differs](../01-philosophy/how-kdos-differs.md#self-hosting) compares building from
source this way with how other distributions produce their systems; this chapter says how it comes
to exist.

The chapter is narrative. The commands and iteration loops are in [Developing](developing.md), the
orchestrator's flags, snapshots and build plans in [The build system](build-system.md), the recipe
format in [Writing ports](writing-ports.md), the package manager in
[Packaging](../03-architecture/packaging.md), and every port by shelf and phase in
[The ports catalogue](../06-reference/ports-catalogue.md).

## The whole journey

A KDOS build compiles every program on the system from upstream source, the C compiler and the
kernel included. Nothing is taken from another distribution's binary packages. The main
exceptions are the seeds of four self-hosting compilers: Rust, Go and GHC each start from a
prebuilt compiler from the same upstream project, and Zig from the WebAssembly build of itself
inside its source tarball, all fetched and verified by hash like any other source. Firmware and
CPU microcode, which run on devices and the processor rather than under the kernel, ship as the
binary files their vendors publish. The complete inventory, including prebuilt data inside
compiled ports and the vendored artwork, is in
[Why KDOS](../01-philosophy/why-kdos.md#what-is-not-built-from-source). The build starts from a
small container image with a C compiler, uses it to make a compiler for KDOS, uses that to make a
minimal KDOS, and then moves inside that minimal KDOS and builds the rest of the system with KDOS's
own tools.

```text
 your machine                       the build container (Alpine, --network none)
 ────────────                       ─────────────────────────────────────────────
 git clone
 make fetch ──► ports/core/<shelf>/<port>/  (every upstream source, verified by hash)
 (network)
 make build ──────────────────────► script/kdosbuild.sh compiles and runs kdosbuild
                                      │
                                      ├─ 00_cross       container  cross binutils + gcc  → build/cross
                                      ├─ 10_bootstrap   container  a minimal KDOS         → build/fs
                                      │                            (musl, toybox, bash, gcc, make, kpkg)
                                      │  ── from here on, every step runs chrooted in build/fs ──
                                      ├─ 20_selfhost    chroot     the toolchain rebuilt as packages
                                      ├─ 30_foundation  chroot     build systems, interpreters, base libraries
                                      ├─ 31_compilers   chroot     LLVM, Rust, Go, GHC, Zig, Node.js, Ruby
                                      ├─ 40_lang        chroot     language modules and developer tools
                                      ├─ 41_system      chroot     services, networking, storage, the command line
                                      ├─ 42_graphics    chroot     Wayland, X11 libraries, Mesa, media frameworks
                                      ├─ 43_toolkits    chroot     GTK, Qt, KDE Frameworks
                                      ├─ 44_apps        chroot     graphical applications
                                      ├─ 50_desktop     chroot     the compositor, shell and daemons
                                      ├─ 60_kernel      chroot     the kernel
                                      └─ 70_image       chroot     initramfs, squashfs, ISO
                                                                   → build/iso-build/kdos.iso
```

Six terms recur throughout:

- A **port** is the recipe for one piece of software: a `kpkgbuild` file of metadata and a
  `build.sh` script beside it. Upstream software has its ports under `ports/core/`, 2,000 of them;
  KDOS's own programs have theirs under four areas of `src/`: `src/system/`, `src/art/`,
  `src/desktop/` and `src/daemons/`.
- A **shelf** is the subject directory an upstream port is filed in, `ports/core/<shelf>/<port>/`:
  `network`, `fonts`, `kf6` and 99 others, listed in `ports/shelves`. A port is still known by its
  bare name; the shelf only says where to find it.
- A **package** is what building a port produces: a compressed archive that the package manager,
  `kpkg`, installs and records in its database.
- A **phase** is one stage of the build: a numbered directory under `script/phases/`. There are
  thirteen, numbered in bands of ten with gaps (`00`, `10`, `20`, `30` and `31`, `40` to `44`, `50`,
  `60`, `70`) so that a new one can go between two others. A phase is named by its directory,
  `41_system`, or by the word after the number, `system`.
- A **chroot** runs a command with `build/fs`, the tree the build fills, as its root directory, so
  the command sees only what the build has put there.
- A **snapshot** is an archive of the build tree taken when a phase completes, so that a later
  build can resume from it. After the first, each is a **layer**: only what changed since the
  snapshot before it.

Other terms, such as box and pack, are defined where they first appear, and every one
is in the [Glossary](../06-reference/glossary.md).

## What your machine needs, and why everything runs in a container

The host needs Docker, `curl` and `sha256sum` for fetching, and disk space; the full list is in
[What a development machine needs](developing.md#what-a-development-machine-needs). It does not
need any compiler toolchain of its own for the build, and its own libraries never reach the result.

`make build` does two things. It builds a container image, `os-dev`, from the repository's
`Dockerfile`: Alpine Linux 3.23 with, among others, `gcc`, `g++`, `musl-dev`, `make`, `bison`,
`flex`, `texinfo`, `linux-headers`, `python3`, `bash`, GNU `coreutils`, `tar`, `xz`, `zstd` and
`rsync`. It then runs that image with:

| Setting | Why |
|---|---|
| `--network none` | The build must be a function of the fetched sources alone. A port that tries to download something during its build fails at once instead of quietly depending on the network |
| `--privileged` | The later phases mount `/proc`, `/sys` and bind mounts inside the target tree and `chroot` into it, which needs those privileges |
| `--cpu-shares=256` | A weight, not a cap: an idle host gives the build every thread; under contention it yields to other containers and services, and on a systemd cgroup-v2 host shares the CPU evenly with the user session. `KDOS_CPU_SHARES` changes the weight. The environment every phase shares sets one job count, `KDOS_JOBS`, for make, `cmake --build` and cargo: the host's thread count, clamped to one job per 2 GiB of memory |
| `--cpus=N`, only with `KDOS_JOBS=N` | Caps the container at the job count, and at the host's thread count, so cargo and go, which size themselves from the container's cgroup `cpu.max` (bind-mounted read-only into the chroot), follow a lowered `KDOS_JOBS` too; ninja is handed `-j$KDOS_JOBS` by `script/bin/ninja` |
| `build/` mounted writable; `src/`, `fs/`, `script/` and `ports/` read-only | A build cannot modify its own sources |

The container is a fixed, known starting point. Whatever distribution your machine runs, the
compilers that start the bootstrap are the same ones, which is why a build on one developer's
machine behaves like a build on another's. Alpine is also built on musl, the C library KDOS uses,
which keeps the bootstrap's first steps simple. The repository's `.git` directory is not mounted,
so nothing inside the build can read version control; `make build` passes the commit and a
"dirty" flag in as environment variables instead, and the orchestrator records them in each
snapshot.

## Fetching: the only step that uses the network

A clone carries recipes, patches and KDOS's own code, but not the upstream source archives the
recipes name. Those are fetched once, before any build, by `make fetch`, which runs `ports/fetch`.

Every upstream file is named in its recipe by a `sha256 =` line. The hash is the file's identity:
any copy that hashes to it is the right file, wherever it came from. For each file,
`ports/fetch` stops at the first copy that verifies, looking in this order:

1. **The port directory**, if the file is already there.
2. **The local cache**, `ports/.srccache/sha256-XX/<hash>`, one file per hash, hard-linked into
   every port directory that names it.
3. **The KDOS source archive**: numbered GitHub pre-releases, the volumes `sources-<N>`, each
   holding the shelves `ports/shelves` maps to it, each asset named `<shelf>--<file>`. The committed
   file `ports/sources.idx` says which volume and which asset hold which hash, one line per file; a file over GitHub's 2 GiB asset limit is held in parts,
   joined and checked as a whole.
4. **Upstream**, the URL on the recipe's `source =` line.
5. **Regeneration**, for a port's own vendor bundle only (below).

The archive is asked before upstream because it cannot disappear when an upstream host does. Nothing
in it is replaced, and nothing is removed except by the maintainer's explicit `--rehome` and
`--prune=yes-delete` runs of `ports/publish`, which spare every file a recipe at a `v*` tag names.
So it can hold what older recipes named as well: `ports/publish --history` seeds it with the files
old commits' recipes point at, a file no current port names kept as `attic--<file>` in the highest
volume, so a checkout written against an older version can still be built. `ports/sources.idx` is
read only in its format 2, whose first line is `# kdos-sources-index 2`; a hash it does not name
comes from upstream. The current recipes name 2,487 distinct files; 39 of them are carried in
git, and 2,448 are fetched. A complete cache holds those 2,448 files; its size is in
[What a build costs](#what-a-build-costs).

**Vendor bundles.** Rust, Go, Python, Haskell and Node software usually downloads its own dependencies
while it builds, which a build with no network cannot allow. For those ports the dependencies are
collected in advance into a *vendor bundle*, `<name>-vendor-<version>.tar.xz`, made by the
language's own package manager and held to its own `sha256 =` line like any other source. 159
recipes in `ports/core` declare one with a `vendoring =` key: 71 Rust, 38 Go, 45 Python, 4 Haskell
and 1 Node. Five more name a vendor bundle with no `vendoring =` key (`python3-lsp-ruff` through its
`pypackages` line, and the hand-made bundles of `pdfium`, `libreoffice`, `librepcb` and `surfer`),
so 164 ports carry one in all. A bundle is normally fetched from the archive like everything else.
When none of the first four locations has it, `ports/fetch` generates it inside a separate
container, `kdos-fetch`, whose toolchains are pinned to the versions this tree compiles with, and
builds the tarball reproducibly so the result matches its recipe hash. How to write such a recipe is
in [Vendoring](writing-ports.md#vendoring).

After `make fetch`, `make build` finds every upstream source in the port directories and uses no
network. A tree fetched once builds offline for as long as its recipes stay the same.
`make fetch-check` confirms, offline, that nothing is missing or corrupt.

The other side of fetching is publishing. A recipe whose `sha256 =` names a file the archive does
not hold builds on the machine that wrote it and nowhere else, so `ports/publish` uploads new files
and writes their lines into `ports/sources.idx`, and the `pre-push` hook in `script/hooks/` refuses
a push whose recipes name a hash the archive lacks. That hook is enabled once per clone:

```sh
git config core.hooksPath script/hooks
```

See [Publishing sources](writing-ports.md#publishing-sources).

## The orchestrator and snapshots

Inside the container, `make build` runs `script/kdosbuild.sh`. That script compiles the build
orchestrator, **kdosbuild**, from `src/devtools/kdosbuild/` and the libraries it links (`libkbase`,
`libkbuild`, `libkpkg`, `libktui` and `libkcolor`) whenever those sources, the flags or the compiler
changed since `build/.kdosbuild.sum` was written, and runs it. kdosbuild is the program that walks
the phases, runs each step, writes its log under `build/logs/<phase>/`, and draws the build screen
(or prints plain lines when there is no terminal).

kdosbuild discovers the phases by listing `script/phases/` for directories whose names start with
a number, and runs them in sorted order. Each phase has an environment file, `phase.env`, in its
own directory. kdosbuild parses five keys from it without running it: a title
(`KDOS_PHASE_TITLE`), a description (`KDOS_PHASE_DESC`), whether the phase runs inside the chroot
(`CHROOT=1`), and which paths to archive, and which to leave out, when the phase completes
(`KDOS_SNAPSHOT_PATHS` and `KDOS_SNAPSHOT_EXCLUDE`). The rest of the file is shell that the phase's
own steps source: the repositories `kpkg` may use, and one of the shared files under `script/env/`,
which carry the compiler flags and the settings that make packages reproducible.

A phase holds either numbered scripts, which run in sorted order, or a list of port names: one
`packages.txt`, or a `packages.d/` directory of lists, one per shelf, read in order as one. For a
list, kdosbuild asks `kpkgdepends` for the install order of every named port and its whole
dependency closure, then turns each port into its own step, `kpkg install <port>`, run one at a
time. Every list names exactly the ports its phase installs, and preflight checks it: a port cannot
drift into another phase because a dependency changed, without a failed check saying so.

With `BUILD_ARGS="--port-jobs N"`, a package phase runs by dependency level instead: the ports
whose same-phase dependencies sit on lower levels build side by side with `kpkg install
--build-only`, up to `N` at once, and a commit step installs the whole level with `kpkg install
--commit` in the serial order before the next level starts. The list's pinned run still builds one
port at a time, first. Since nothing is installed while a level builds, each port sees exactly what
it would see in a serial build, provided its recipe declares everything it builds against. See
[Building a package phase by level](build-system.md#building-a-package-phase-by-level).

When a phase completes, kdosbuild archives the paths it declared into `build/snapshots/<phase>/`.
The first two phases declare `cross`, `fs` and `mark` (the cross toolchain, the target tree and the
"already done" markers); the package phases up to `50_desktop` declare `fs`; `60_kernel` and
`70_image` declare nothing and are re-run on the tree they find. The first snapshot of a path holds
the whole tree; each later one is a layer holding only the entries that changed since the snapshot
before it, found by comparing a walk of the tree with an index kept in `build/.snap-lineage/`, and
a list of what was removed. A later build can restore any snapshot, by extracting the full archive
at the bottom of its chain and each layer above it, and continue from the phase after it,
so a failure hours in does not mean starting again. Before a build begins, a picker on the terminal
asks which snapshot to start from and whether to write new ones. Snapshots, their disk cost and the
rules that keep a restore safe are in [Snapshots](build-system.md#snapshots).

## The cross toolchain: `00_cross`

The first problem is that there is no compiler that produces KDOS programs. The container's
compiler produces programs for Alpine: it looks for headers in the container's `/usr/include` and
links against the container's libraries. A program built that way and copied into KDOS would carry
the container's assumptions with it.

So the first phase, `00_cross`, builds a **cross toolchain**: a compiler that runs in the
container but targets a system that does not exist yet, named by the target triple
`x86_64-kdos-linux-musl`. Both of its two scripts extract their sources from the fetched port
directories with the helper in `script/lib/port.sh`, which finds a port by name on whichever shelf
it sits, and install into `build/cross`:

| Step | Builds |
|---|---|
| `00_binutils.sh` | The assembler and linker (binutils 2.47) for the target, with `build/fs` as their system root |
| `01_gcc.sh` | GCC 16.2.0, with GMP, MPFR and MPC compiled into it, for C and C++, built without the info manuals |

The cross GCC is deliberately incomplete. There is no C library for the target yet, so it is
configured `--without-headers`, with no shared libraries, no threads and no C++ standard library.
It is enough to compile a C library and nothing more. Because its system root is `build/fs`, it
never looks in the container's own directories: every header and library it finds is one the
build put there. A complete `build/cross` is about 291 MB (277 MiB).

Each script ends by touching a marker under `build/mark/cross/` and exits at once when the
marker exists, so re-running the phase does no work twice.

## A minimal KDOS: `10_bootstrap`

`10_bootstrap` builds, in `build/fs`, the smallest system that can build the rest of itself: a C
library, a shell, the core command-line tools, a native compiler, `make`, and the package manager.
It still runs in the container, and most of it is cross-compiled with the tools from `00_cross`.
None of it goes through the package manager, because the package manager is the second-to-last
thing it builds. Its sixteen scripts run in this order:

| Script | Builds or does | Why here |
|---|---|---|
| `000_file_system.sh` | The directory skeleton, the merged-`/usr` links (`/bin`, `/sbin`, `/lib` and `/lib64` point into `/usr`), `/var/run` as a link to `/run`, and a copy of the repository's `fs/` overlay | Everything later installs into these directories; the overlay carries KDOS's own configuration |
| `010_linux_headers.sh` | The kernel's user-space headers (Linux 7.2.7) into `/usr/include` | The C library is compiled against them |
| `020_musl_libc.sh` | musl 1.2.6, the C library | With it, the cross compiler can link a complete program |
| `030_libstdc++.sh` | The C++ standard library, from the GCC sources | C++ programs, the native GCC among them, need it |
| `040_ncurses.sh` | ncurses 6.6, after first building a `tic` for the container to compile terminal descriptions | readline and bash link it |
| `050_xz.sh`, `060_gzip.sh`, `061_tar.sh` | The tools that unpack source archives | Inside the chroot, every build begins by extracting a `.tar.xz` or `.tar.gz` |
| `062_toybox.sh` | toybox 0.8.14, a single binary providing most of the basic commands | The chroot needs `ls`, `cp`, `sed` and the rest. `tar` and `file` are compiled out, as are the applets whose real tool installs at a different path, whose real tool this phase has already installed at the same path (gzip's `gunzip` and `zcat`), or that no port provides, so a toybox link never shadows or replaces a real tool; applets such as `sed` and `find` stay, and the later port that installs the real tool at the same path overwrites the link |
| `070_readline.sh`, `080_bash.sh` | readline and bash 5.3, with `sh` linked to `bash` | Every recipe's `build.sh` is a bash script |
| `090_binutils.sh`, `100_gcc.sh` | A native binutils and GCC: built by the cross compiler, but running on KDOS and producing programs for KDOS. The GCC is built without the info manuals and installs to the package's own paths (`--libexecdir=/usr/lib`), so `20_selfhost`'s `gcc` package replaces every file it installed | Inside the chroot there is no container compiler to fall back on |
| `110_make.sh` | GNU make | Almost every upstream build runs it |
| `120_kpkg.sh` | `kpkg`, compiled from `src/system/kdos-kpkg` with `libkbase`, `libkpkg` and `libksig`, installed as `/usr/bin/kpkg` with four more names linked to it | Nothing can read a recipe before `kpkg` exists |
| `130_kinstall.sh` | The installer, `kinstall`, from `src/system/kdos-installer` | It links nothing beyond musl, so it exists on every tree from this phase onwards |

`gzip` and `tar` are configured without a target, so the container's own compiler builds them;
they run in the chroot because the container is also x86_64 and musl, with its dynamic loader at
the same path. `20_selfhost` rebuilds `tar` with KDOS's own compiler.

The copy of `fs/` in `000_file_system.sh` is more careful than a plain copy, because the build tree
is incremental: it deletes files that `fs/` has stopped providing, merges `/etc/passwd`,
`/etc/group` and `/etc/shadow` so that service accounts added by packages survive a re-sync, and
sets the modes and owners git cannot carry, such as `/etc/shadow` at `600`. Those rules are in
[Syncing `fs/`](build-system.md#syncing-fs).

Every script but `000_file_system.sh` leaves a marker in `build/mark/bootstrap/` and, like the
`00_cross` scripts, exits at once when it finds it; `000_file_system.sh` has none and re-syncs `fs/`
on every run. The `kpkg` step's marker holds a hash of every source file it compiles, and the step
exits only when that hash still matches, so a change to the package manager or its libraries
rebuilds it even though no port covers it.

At the end of `10_bootstrap`, `build/fs` is a tiny but complete Linux userland: something you could
`chroot` into and compile a program with.

## Entering the chroot

Every later phase sets `CHROOT=1`, so kdosbuild runs each of its steps through
`script/chroot/exec.sh` instead of directly in the container. For each command, that script:

1. takes a private mount namespace of its own (`unshare --mount`);
2. mounts `/dev`, `/proc`, `/sys`, and fresh temporary filesystems on `/tmp` and `/run`;
3. bind-mounts the repository at `/kdos`, `build/` at `/kdos/build` and `ports/` at `/ports`;
4. runs the command with `chroot build/fs`, an empty environment (`env -i`) apart from `HOME`,
   `TERM`, `PATH` and a handful of named build switches, and `/kdos` as the working directory.

Every one of those mounts exists only inside the command's namespace. Nothing is unmounted
afterwards: the mounts vanish with the command's last process, even when it is killed, and two
commands running at the same time never see each other's `/tmp` or `/proc`.

A `chroot` makes `build/fs` the root directory for the command it runs. From that moment the
compiler, the shell, the libraries and the tools a build uses are the ones `10_bootstrap` made, and
the container's own are out of reach. The empty environment means no variable from the container
leaks into a build; a phase's settings reach a step only because the step sources that phase's
`phase.env` inside the chroot. The details, including why an opt-in flag has to be named in
`script/chroot/exec.sh` as well as in the `Makefile`, are in [The
chroot](build-system.md#the-chroot).

### The compiler cache

Once `30_foundation` has installed ccache, every CMake project a chroot step configures compiles
through it. `script/env/chroot.env` sets `CMAKE_C_COMPILER_LAUNCHER` and
`CMAKE_CXX_COMPILER_LAUNCHER` to `ccache`, and CMake still records `/usr/bin/gcc` and
`/usr/bin/g++` as the compilers. Meson and autotools ports are not cached. The cache is
`build/ccache`, outside the target tree, capped at 20 GB; `make cleanbuild` keeps it, `make clean`
removes it, and `make build KDOS_CCACHE=0` turns it off. `zig` compiles with `CCACHE_DISABLE=1`: ccache
preprocesses a file before compiling it and holds the result in memory to hash it, and the
preprocessed form of zig's generated 222 MB `zig2.c` runs past 300 GB, so ccache fails on
`std::bad_alloc` where gcc alone compiles the file in under 7 GB.

A cached object is byte-identical to the one a compile would write, and three settings hold that:

- **No `base_dir`.** It rewrites absolute paths in the command line to relative ones, so
  `-ffile-prefix-map` stops matching them and the paths recorded in the objects change.
- **The compiler check is a string, never the compiler's modification time.** kpkg pins every
  installed file's time to `SOURCE_DATE_EPOCH`, so a rebuilt gcc of the same size would look
  unchanged. The string is the recipe hashes of gcc, binutils and the LLVM ports, gcc's version and
  the epoch: any of them changing misses the whole cache.
- **No masquerade directory on `PATH`.** CMake would record `/usr/lib/ccache/gcc` as the compiler,
  and that path is written into CMake and pkg-config files the packages ship.

With the cache off, `CCACHE_DISABLE=1` is set instead, because meson and several projects use ccache
whenever they find it and would fill `/root/.cache/ccache` inside `build/fs`. `testing/preflight.sh`
fails when any of these settings changes.

### The package store

`make build KDOS_PKG_STORE=1` also keeps every package the chroot phases build in `build/pkgstore`,
keyed on the port's recipe hash, the build environment less its job counts, a hash of the bootstrap
phases, and the exact package-file hash of every dependency the port declares plus the toolchain and
base userland. A later build that reaches a port with the same key installs the stored package and
prints `Reusing <pkg> from the store (<key12>)` instead of building it; a dependency rebuilt into
different bytes changes the key of everything above it. It is off by default. `KDOS_PKG_STORE=check`
builds every hit anyway and logs a difference, which is the setting for a release. `make cleanbuild`
keeps the store, `make clean` removes it, and `70_image` evicts it down to `KDOS_PKG_STORE_MAX`
(default `60G`). What the key covers and what it cannot see is in
[The package store](../03-architecture/packaging.md#the-package-store).

## What a port build is

From `20_selfhost` onwards, everything is built the same way: `kpkg install <port>`, run inside
the chroot. This is the one mechanism the rest of the build repeats about two thousand times, so it
is worth following once with a real port: `pv`, a small tool that shows the progress of data
through a pipe. It sits on the `cli` shelf, and `41_system` installs it: it is named in
`script/phases/41_system/packages.d/cli.txt`, the file of that shelf's ports in that phase's list,
described under [The userland](#the-userland-40_lang-to-44_apps).

The port is a directory, `ports/core/cli/pv/`, holding two files git tracks and the source archive
`make fetch` put there. Its `kpkgbuild`, less the banner every file in the tree opens with, is:

```ini
name        = pv
version     = 1.12.0
release     = 1
source      = https://codeberg.org/ivarch/pv/releases/download/v$version/$name-$version.tar.gz
sha256      = 31fdbdb449c7143cd2968567bef7599e9f031950e6158ee7bb76e40aebf6ffb8  pv-1.12.0.tar.gz
description = Pipe viewer — progress, throughput and ETA for any stream
homepage    = https://www.ivarch.com/programs/pv.shtml
depends     = ncurses
```

and its `build.sh`, less the banner and a comment explaining the choice of tarball and flags, is
three lines:

```sh
./configure --prefix=/usr --disable-static --with-ncurses --disable-nls
make
make DESTDIR=$PKG install
```

The `kpkgbuild` is parsed, never executed: `kpkg` reads it as keys and values, expanding `$name`
and `$version` itself. Only `build.sh` is bash. This is what installing it involves:

1. **Ordering.** When `41_system` starts, kdosbuild hands the whole list to `kpkgdepends`, with the
   package database hidden (`PKGDB_DIR=/dev/null`) so the order covers every port whether or not
   it is installed. `pv`'s `depends = ncurses` puts `ncurses` ahead of it. Each port in the order
   becomes a step, and the step for `pv` runs `kpkg install pv` in the chroot, with
   `script/phases/41_system/phase.env` sourced first.
2. **Deciding.** `kpkg` resolves `pv`'s dependencies against the real database. `ncurses` was
   installed earlier from an unchanged recipe (`20_selfhost` installs it for `gawk`, and
   `40_lang`'s list names it again near its top), so it is dropped. `pv` is either not installed,
   or installed from a recipe whose hash differs from the one recorded, so it is built. A `pv`
   installed from this exact recipe would be skipped, which is what makes re-running a phase cheap.
3. **Verifying.** `kpkg` hashes `pv-1.12.0.tar.gz` in the port directory and refuses to go on if
   it does not match the `sha256 =` line.
4. **Building.** It extracts the archive into `/var/cache/kpkg/work/pv/pv-1.12.0`, changes into it,
   and runs `build.sh` under bash with `set -e` and a handful of variables set: `$PKG` is an empty
   staging directory, `/var/cache/kpkg/work/pv/pkg`. `make DESTDIR=$PKG install` installs into the
   staging directory, not into the live tree. Everything the build prints is the step's log,
   `build/logs/41_system/<N>_pv.install.log`.
5. **Packaging.** `kpkg` rolls the staging directory into `pv-1.12.0-1.tar.xz`, with sorted names,
   owner 0, a pinned timestamp and a pinned multi-threaded `xz`, so that the same recipe on the same
   tree produces the same bytes. A package the step deletes after installing it is compressed with
   `xz -0`; with `KDOS_MAKE_BINHOST=1` every package is kept and gets `xz -9`.
6. **Installing.** `kpkgadd` checks the package's paths against every other package's, places the
   files, writes the package's manifest to `/var/lib/kpkg/db/pv`, records the package file's hash in
   `/var/lib/kpkg/db/.pkgsha/pv` and the recipe hash in `/var/lib/kpkg/db/.recipe/pv`. With the
   package store on, the archive is copied into `build/pkgstore` under the key taken before the
   build. The built archive is then deleted; the tree keeps only the installed files and the
   database entry.

The database under `/var/lib/kpkg/db` is how the build knows what is installed, what each package
owns, and which recipe each came from. It ships in the image, and the running system's `kpkg` uses
the same one. The recipe hash covers `kpkgbuild`, `build.sh`, any `postinstall.sh`, every
patch and every other file beside them that no `sha256 =` line names, so editing any of them is
enough for the next build to rebuild the port. For KDOS's own
programs, which have no upstream archive, it also covers the port's whole directory and all of
`src/libs`. The rules are in
[Deciding what to rebuild](../03-architecture/packaging.md#deciding-what-to-rebuild), file
ownership in [Who owns a file](../03-architecture/packaging.md#who-owns-a-file), and the
recipe format in [Writing ports](writing-ports.md).

## The toolchain rebuilt as packages: `20_selfhost`

`20_selfhost` is the first phase inside the chroot. Its list names eight ports: `tar`, `musl`,
`zlib`, `binutils`, `diffutils`, `m4`, `gawk` and `gcc`. With their dependencies (`xxhash` for
binutils; GMP, MPFR and MPC for GCC; readline and ncurses for gawk) the resolved order is 14 ports.

The phase exists for two reasons. First, `10_bootstrap` installed its programs without the package
manager, so nothing records who owns their files. When `kpkg` installs a package over a path that no
package claims, it adopts the path, so rebuilding these ports turns the hand-installed C library,
assembler, linker and compiler into packages with manifests. Second, it makes KDOS
**self-hosting**: after this phase, the C library and the compiler are themselves compiled by a
compiler running on KDOS, inside KDOS, and depend on nothing the container produced. `m4`, `gawk`
and `diffutils` are tools the GMP and GCC builds run. `tar` is rebuilt because `10_bootstrap` built
it with the container's compiler.

## The foundation and the compilers: `30_foundation` and `31_compilers`

These two phases install everything needed to rebuild KDOS from KDOS. They are split by one
question: does a port need one of the big compilers?

`30_foundation` names 126 ports and installs 116; the other ten were installed by `20_selfhost` and
are named again so that a changed recipe is rebuilt here. The list opens with a pinned run: the 16
build tools recipes use without naming them in `depends` (`musl`, `gcc`, `binutils`, `make`,
`pkgconf`, the autotools, `m4`, `bison`, `flex`, `cmake`, `meson`, `ninja`, `python3` and `perl`),
then `bash` and `toybox`, then `tar`, `diffutils`, `gawk`, `ncurses`, `readline` and `zlib`, the
rest of `20_selfhost`'s list and the libraries `gawk` links, so each is current before anything in
the phase uses it. The other names sit under a heading for each shelf. Here the build gains
CMake, Meson and Ninja; Python, Perl and Lua; GNU `sed` and `findutils` over toybox's smaller
applets (upstream build systems assume GNU extensions); `curl`, `git` and OpenSSL; `util-linux`,
`eudev` and `shadow`; and ccache, the compiler cache every later CMake build uses. `bash`, which only `10_bootstrap` built, is built here as a package for the
first time and adopts the files that phase left.

`31_compilers` names 22 ports and installs all 22: LLVM, clang, lld, compiler-rt, libunwind and
openmp, with the pinned 21 series some ports build against; Rust, cargo-c, bindgen and cbindgen; Go; GHC and
cabal-install; Zig; Node.js; Ruby; pandoc and asciidoctor; and `ccache-manual`, ccache's manual page, which needs
asciidoctor. On the last measured build
these 22 took about 7.6 of the 8.7 hours the two phases spend, which is why they are a phase of
their own: a failure among them restores `30_foundation` and loses minutes, not the hour of
foundation work before them.

From `20_selfhost` onwards, the shared environment names the compiler outright (`CC=gcc`,
`CXX=g++`); `20_selfhost` itself unsets it, since it is building that compiler. Once clang is
installed, some configure scripts would otherwise prefer it, and the toolchain would change
depending on which ports happened to be installed.

## The userland: `40_lang` to `44_apps`

The bulk of the system is five phases, 1,842 ports between them, split by what each port's
dependencies reach. Every port's dependencies are in its own phase or an earlier one:

| Phase | Installs | Holds | Largest shelves |
|---|---|---|---|
| `40_lang` | 167 | Language modules and developer tools whose dependencies need nothing past the compilers: the `python3-*` and `perl-*` modules, build, documentation and debugging tools, version control | `python-libs`, `devtools`, `python`, `python-net` |
| `41_system` | 967 | Everything with no graphics and no toolkit among its dependencies: services, networking, storage, the command line, codecs, firmware, fonts, science and hardware libraries | `image-libs`, `devlibs`, `formats`, `network`, `audio-codecs` |
| `42_graphics` | 186 | What reaches Wayland, the X11 libraries, Mesa, cairo, pango, GStreamer, FFmpeg or PipeWire but no toolkit: the graphics and media stacks, Xwayland, `foot` | `x11`, `gpu`, `wl`, `game-libs`, `graphics-libs` |
| `43_toolkits` | 242 | GTK, Qt 5 and 6, KDE Frameworks, wxWidgets, FLTK and WebKitGTK, and the libraries and services built on them | `kf6`, `qt6`, `qt-extra`, `gtk`, `qt5` |
| `44_apps` | 280 | The natively ported graphical applications | `graphics`, `emulators`, `games-board`, `studio`, `hamradio` |

Their environment adds `src/system` and `src/art` to the repositories `kpkg` searches, so KDOS's
own tools, theme, icons, cursors, splash, pack tools, box launcher and `kdos-bb` are built here as
ordinary ports alongside upstream software, each in the phase its dependencies put it in:
`kdos-tools` in `42_graphics`, because its keyboard support reaches Wayland, and `kdos-appbox` in
`44_apps`. The dependency graph also puts `podman`, `distrobox` and `qemu` in `43_toolkits`:
`podman` and `distrobox` because GnuPG's passphrase prompt, which they reach through `gpgme`, builds
against Qt; `qemu` because its display backend depends on GTK 3 (`gtk3`, `vte3`) directly.

The toolkits serve applications only: each is built with its Wayland backend as the default and its
X11 backend compiled in for Xwayland, and none of them is linked by the desktop that `50_desktop`
builds. A **box** is a rootless podman container that one application, or one working environment,
runs in, which is how the catalogue's applications run without being ported; the box launcher,
`kdos-appbox`, starts and manages them.

Each of the five phases keeps its list as a directory, `packages.d/`, with one file per shelf:
`41_system/packages.d/network.txt` holds the `network` shelf's ports that phase installs. A file
named `src-system.txt` or `src-art.txt` holds KDOS's own ports from that `src/` area. The files are
read in sorted order as one list, and `kpkgdepends` installs a name written earlier before one
written later unless a dependency says otherwise, so where order matters it is written down:
`00-order.txt` sorts first and holds the runs a comment pins. In `40_lang` it is `toybox` followed
by the ports that own the commands a toybox upgrade takes back; in `41_system` it is `coreutils`
ahead of the text games that install with GNU `install`, and the Python modules that must install
after the applications whose vendor bundles carry their own copies. Every file, with its ports, is
in [The ports catalogue](../06-reference/ports-catalogue.md). A new port is named in its shelf's
file of the phase its subject and its dependencies place it in; the rule is in
[Package lists](build-system.md#package-lists).

Shelves are distinct from the `group =` key a recipe may carry, which only the upstream version
checker reads, to offer related version bumps together; see
[The group key](../03-architecture/packaging.md#the-group-key).

## The desktop: `50_desktop`

`50_desktop` names and installs 22 ports, with an order of 376 once dependencies are followed,
all but those 22 installed and skipped by this point. Its environment adds `src/desktop` and
`src/daemons` to the repositories, because that is where the compositor, the shell and the root
daemons live, and no earlier phase can build them. It installs wlroots; the compositor `kdos-comp`;
`kdos-boxsock`, which gives each box its own Wayland socket; the panel `kdos-shell`; the terminal
`kdos-term`; the lock screen `kdos-lock`; the resource monitor `kdos-res`; the daemons
`kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `kdos-mountd` and `kdos-packd`; the input method
`fcitx5` with three language engines; the desktop portals `xdg-desktop-portal-wlr` and
`xdg-desktop-portal-kdos`; and the screen recorder `kdos-record`.

A **pack** is one application, runtime or base as a single signed file, an EROFS filesystem image
with its metadata appended; packs are how a set of applications exported on one machine is
imported on another, and `kdos-packd` installs and mounts them.

The desktop is its own phase because the Wayland base in `42_graphics` is useful without a
compositor, and because keeping it separate gives the userland a snapshot, `44_apps`, that holds
none of the desktop: resuming from it re-runs the desktop's list, the kernel phase and the image,
and nothing below them. A change to one desktop program is cheaper still as a build plan on the
current tree. The programs themselves are described in
[The programs](../04-programs/README.md).

## The kernel: `60_kernel`

`60_kernel` names two ports, `dwarves` and `linux` (Linux 7.2.7), and is the last port phase before
the image. The kernel's recipe depends on `bc`, `bison`, `flex`, `perl`, `openssl`, `elfutils`,
`zstd`, `kmod`, `dwarves`, `rust`, `bindgen` and `clang`. All of them except `dwarves` are installed
by this point, so the phase builds `dwarves` (the `pahole` tool that generates BTF, the kernel's
compact description of its own types, which the kernel configuration enables) and then the kernel.
The kernel lands in `/boot/vmlinuz-kdos` with its modules under `/lib/modules`, which the
merged-`/usr` link makes `/usr/lib/modules`.

## Packaging: `70_image`

At this point `build/fs` holds the complete system. `70_image` turns it into a bootable medium.
Its twelve scripts run inside the chroot in the order their numbers give:

| Step | Does |
|---|---|
| `010_binhost.sh` | With `KDOS_MAKE_BINHOST=1`, copies the packages this build made into a signed binhost under `build/binhost/`; otherwise does nothing |
| `015_pkgstore.sh` | With the package store on, evicts `build/pkgstore` down to `KDOS_PKG_STORE_MAX`, least recently used first; otherwise does nothing |
| `020_cleanup.sh` | Removes build-only files: the build user's caches, the kpkg work and package caches, podman's store, and Python bytecode, which it then recompiles as hash-checked bytecode so the image is reproducible |
| `030_launchers.sh` | Reconciles the generated application launchers in `/etc/skel` with the packs the image carries; with none, it removes every one |
| `040_orphans.sh` | Removes every installed package that has no recipe in the tree |
| `050_theme.sh` | Seeds the default theme into `/etc/skel`, after checking that the accent the phase names is the one compiled into `libkcolor` |
| `060_udev_hwdb.sh` | Compiles the udev hardware database, `/etc/udev/hwdb.bin` |
| `070_user.sh` | Creates each ordinary user's home directory from `/etc/skel` |
| `080_whatis.sh` | Builds the manual-page index `apropos` and `whatis` search |
| `090_initramfs.sh` | Builds the initramfs, `build/initramfs.cpio.gz` |
| `100_packs.sh` | Creates the pack store's directories; with `KDOS_PACK_KDOS=1`, also packs the system as the base pack `kdos` |
| `110_iso.sh` | Assembles the ISO tree and writes `build/iso-build/kdos.iso` |

The order carries dependencies: the binhost is written before the cleanup empties the package
cache, the launchers and theme are written into `/etc/skel` before `070_user.sh` copies it into
homes, and the orphan sweep runs before the two indexes are built so a removed package's rules and
manual pages are not indexed.

**The initramfs** is the small filesystem the kernel unpacks into memory at boot, whose only job is
to find and mount the real root. `090_initramfs.sh` builds it from the finished tree: toybox and
bash, util-linux's `switch_root`, `mount`, `losetup` and `blkid`, eudev, the tools for an encrypted,
RAID or LVM root, `e2fsck`, the boot splash, `kdos-bootctl` for
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection) (choosing which of two
installed copies of the system to boot), the kernel
modules the early boot needs, and the `init` script itself. It copies each program with every
library it links and refuses to finish if any program names a library the initramfs lacks. CPU
microcode goes in an uncompressed archive in front of the compressed one, where the kernel looks
for it. What the initramfs does at boot is in
[The initramfs](../03-architecture/boot-and-init.md#the-initramfs).

**The ISO** is assembled by `110_iso.sh` in `build/iso_root`:

1. The kernel and initramfs are copied to `boot/`, outside the EFI system partition, because the
   bootloader reads the ISO filesystem directly.
2. The whole root filesystem is compressed into `system.sfs` with `mksquashfs`, in zstd at level
   15 by default (`KDOS_ISO_COMP` picks another; see
   [The packaging steps](build-system.md#the-packaging-steps)), leaving
   out the pseudo-filesystems, caches, logs and the repository bind mounts, and recreating their
   mount points as empty directories.
3. The Limine bootloader is copied in for both BIOS and UEFI, with a `limine.conf` that offers
   three live entries (normal, a clean session that ignores the persistence store, and verbose),
   plus memtest86+ on UEFI when it is present, drawn in the default accent with the KDOS artwork
   and console font.
4. `xorriso` writes a hybrid image that boots from optical media and from a USB stick, and
   `limine bios-install` adds the BIOS boot code.
5. The script checks the result: an EFI system partition is present, the MBR carries its boot
   signature, and its boot code area is not empty. Any failure fails the build.

The medium carries no applications: podman builds each one from the catalogue on the machine that
wants it, and a set exported elsewhere can be imported as packs; see
[Two lanes, one box](../03-architecture/packs-and-boxes.md#two-lanes-one-box). Two opt-in
switches change what it carries, `KDOS_ISO_SOURCES=1` (the tree's sources under `/sources`) and
`KDOS_PACK_KDOS=1` (the base pack); see [The first build](developing.md#the-first-build). How the
medium boots is in [Boot and init](../03-architecture/boot-and-init.md).

## Rebuilding a small part

A full build runs every phase, but a change rarely needs one. Two mechanisms make a narrow rebuild
safe.

The first is that re-running is cheap by construction. The `00_cross` scripts and every
`10_bootstrap` script but `000_file_system.sh` exit at once when their markers are current, and
kdosbuild runs no step for a port whose recipe hash matches what is installed. Running a package
phase again lists its whole order, but enters the chroot only for what changed.

The second is the **build plan**: flags that tell kdosbuild which phases, which scripts and which
ports to run on the tree already in `build/`, without restoring anything. After changing `pv`'s
recipe, for example:

```sh
make build BUILD_ARGS="--phases 41_system,70_image --rebuild pv"
```

kdosbuild runs `41_system` and `70_image` only. `41_system` resolves its order as always; every
other port in the order is installed and current, so kdosbuild marks it `installed` and runs no
step for it; `pv` is forced with `kpkg install -f` and rebuilt; its dependencies are not forced. `70_image` then produces a new ISO
from the updated tree. A plan that narrows the build also stops kdosbuild writing snapshots,
because a snapshot of a partly re-run tree would be filed under a phase whose contents it does not
match. The recipe for each kind of change (a file under `fs/`, a port, a desktop program, a
library) is the table in
[Rebuilding one thing](developing.md#rebuilding-one-thing), and the rules of a plan are in
[Build plans](build-system.md#build-plans).

For a desktop program there is a faster loop still, `testing/quick.sh`, which builds the program
and patches it into an already booted ISO without repacking the medium; see
[The fast loop](testing.md#the-fast-loop).

## What a build costs

These figures come from the chapters that measure them:

| Cost | Figure | Source |
|---|---|---|
| Upstream sources, fetched once | About 41.5 GB (38.6 GiB), each file held once in `ports/.srccache` | [Developing](developing.md#what-a-development-machine-needs) |
| A build from nothing | Most of a day | [Building from scratch](developing.md#building-from-scratch) |
| A complete set of phase snapshots | Not yet measured for the eleven phases that take one. On a synthetic ten-phase tree of about 4.4 GB, layers took 1.86 GB against 7.99 GB for full snapshots, 23% | [Snapshots](build-system.md#layers-and-full-snapshots), [Developing](developing.md#what-a-development-machine-needs) |
| One program rebuilt, with packaging | Not yet measured with the zstd image; most of it is writing the ISO, and `KDOS_ISO_COMP=zstd:3` shortens that part | [The fast loop](testing.md#the-fast-loop) |
| One program rebuilt without packaging, patched into a booted ISO | About three minutes, or 1m37s when nothing is rebuilt and the session is not restarted (`KDOS_QUICK_NOBUILD=1 KDOS_QUICK_KEEP=1`) | [The fast loop](testing.md#the-fast-loop) |

Snapshots are optional, and the startup picker can turn them off. A build from scratch also needs
the progress markers emptied first: `--fresh` runs every phase on the tree already in `build/`, and
does not start from nothing; see [Building from scratch](developing.md#building-from-scratch).

## See also

- [Developing](developing.md) — the commands: fetching, the first build, narrow rebuilds, releases
- [The build system](build-system.md) — kdosbuild's flags, snapshots, build plans and the chroot
- [Writing ports](writing-ports.md) — the recipe format, key by key, and adding a port end to end
- [Packaging](../03-architecture/packaging.md) — `kpkg`, the ports tree and its shelves, phases, and
  reproducible packages
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port, by shelf and phase
- [Build troubleshooting](build-troubleshooting.md) — recurring build failures, by symptom
- [Boot and init](../03-architecture/boot-and-init.md) — what the medium this build produces does
  when it boots
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — how this source-built, self-hosting
  approach compares with other distributions

<!-- book-nav -->
---

*Part V — Building and developing, chapter 30.* Previous: [29. kdos-bb](../04-programs/kdos-bb.md) · [Contents](../README.md) · Next: [31. Developing](developing.md)
