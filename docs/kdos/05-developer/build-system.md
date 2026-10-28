# The build system

This chapter describes the machinery underneath `make build`: the container the build runs in, the
phases and how each one runs, the chroot the later phases execute inside, what the first and last
phases do to the root filesystem (the `fs/` sync, the orphan sweep and the packaging steps), how
sources are fetched beforehand, and the two mechanisms, snapshots and build plans, that let a
change be tested without a full rebuild. It also documents `kdosbuild`, the program that
orchestrates the build, down to its flags, keys, log files and machine-readable output. It is
written for contributors who need to resume, narrow or debug a build, or change the build
machinery itself. Read [How KDOS is built](how-kdos-is-built.md) first
for the story of a build from `git clone` to a bootable ISO, and [Developing](developing.md) for the
everyday commands; this chapter explains why those commands behave as they do.

## The shape of a build

A build turns the repository into two things: a root filesystem at `build/fs` and a bootable image
at `build/iso-build/kdos.iso`. Four layers take part, each started by the one before it:

1. **`make build`** refuses to start while `build/iso-build/kdos.iso` is held open by another
   process, since rewriting an image a virtual machine is reading gives that guest I/O errors
   (`make build ALLOW_ISO_IN_USE=1` overrides the check). It then builds the `os-dev` image from the
   repository's `Dockerfile` (Alpine 3.23 with GCC, GNU make, the GNU userland, bison, flex, zstd
   and Python) and
   runs it with no network, `--privileged` and a CPU weight rather than a cap (see below). It asks docker for a terminal only when
   its own standard input is one, so a build started with no terminal (from CI, or with
   `</dev/null`) still starts. Redirecting only the output is not enough: the container still gets
   a terminal and draws the full-screen interface into the file, so a logged build from an
   interactive shell takes `BUILD_ARGS=--plain` (see [Headless and structured output](#headless-and-structured-output)).
2. **`script/kdosbuild.sh`**, inside that container, compiles the orchestrator from source when
   its sources, flags or compiler changed, and runs it.
3. **`kdosbuild`**, the orchestrator, discovers the phases under `script/phases/` and runs them in
   order, writing logs as it goes, and a snapshot of each phase when asked to.
4. **The phases** do the work: the first two in the container itself, the rest inside the target
   root filesystem through `script/chroot/exec.sh`.

The container sees the repository at `/workspace`. `src/`, `fs/`, `script/`, `ports/`, `docs/`
and `testing/`, the `Makefile` and the `Dockerfile` are mounted read-only and only `build/` is
writable, so a build cannot modify its own sources; nothing else of the checkout, `.git`
included, is mounted. The
`Makefile` passes fourteen variables in: `HOST_UID` and `HOST_GID` (see [kdosbuild](#kdosbuild)),
`KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY` (recorded in each snapshot and stamped into
`/etc/os-release`), `KDOS_GIT_DATE` (the commit's date, stamped beside them), `KDOS_GIT_TIME` (the
commit's time in Unix seconds, stamped into `/ports/TREE`), the job count `KDOS_JOBS`
(empty unless the developer set it), the `system.sfs` codec `KDOS_ISO_COMP`, the compiler-cache
switch `KDOS_CCACHE`, the package-store switch `KDOS_PKG_STORE` and its cap `KDOS_PKG_STORE_MAX`,
and the three opt-in
packaging flags `KDOS_ISO_SOURCES`, `KDOS_PACK_KDOS` and `KDOS_MAKE_BINHOST`. It runs the container with `--cpu-shares=256`, a weight
rather than a cap, and adds a `--cpus` cap only when `KDOS_JOBS` is set. `make build` downloads
nothing; every source has to be in place beforehand, which is what `make fetch` does (see
[Fetching sources in a container](#fetching-sources-in-a-container)).

Four terms are used throughout:

- A **port** is a recipe for building one package: a `kpkgbuild` metadata file with a `build.sh`
  beside it. See [Writing ports](writing-ports.md).
- A **shelf** is the subject directory an upstream port is filed in. Upstream software has its
  recipes at `ports/core/<shelf>/<name>/`, on 102 shelves listed in `ports/shelves`; KDOS's own
  programs have theirs directly under a `src/` area, `src/system/`, `src/art/`, `src/desktop/` or
  `src/daemons/`. A port is known by its bare name wherever it is filed, so the shelf never appears
  in a package list or on a command line. See
  [Repositories and shelves](../03-architecture/packaging.md#repositories-and-shelves).
- A **phase** is one stage of the build: a directory under `script/phases/` whose name starts with
  a number and an underscore.
- The **target tree** is `build/fs`, the root filesystem the build assembles and every phase from
  `20_selfhost` on runs inside.

Other terms, such as chroot, cross toolchain, pack and glyph tier, are defined in the
[Glossary](../06-reference/glossary.md).

## Phases

The orchestrator lists `script/phases/`, keeps the directories whose names are a number followed
by an underscore, and runs them in sorted name order. The part of the name after the number is the
phase's short name: `41_system` is the phase `system`, and either form is accepted wherever a
phase is named. Each phase either holds numbered shell scripts or a package list naming the ports
it installs. Nothing else under `script/` is a phase: `env/` holds the environment the phases
share, `chroot/` the chroot wrappers, `lib/port.sh` the port reader the first two phases source,
`kdosbuild.sh` the host wrapper the `Makefile` runs, `bin/` the `ninja` and `meson` wrappers the
chroot phases put first on the search path (see [The phase environment](#the-phase-environment)),
`book/` the renderer of *KDOS from Scratch*, and `hooks/` the git
[pre-push hook](writing-ports.md#the-pre-push-hook).

| Directory | Title | Runs in | Contents | Snapshots |
|---|---|---|---|---|
| `00_cross` | Cross Toolchain | Container | 2 scripts: cross binutils and gcc for `x86_64-kdos-linux-musl` (musl is the C library KDOS is built on) | `cross fs mark` |
| `10_bootstrap` | Base Userland | Container | 22 scripts, `000_file_system.sh` to `130_kinstall.sh`: the `fs/` overlay, kernel headers, musl, libstdc++, ncurses, xz, gzip, tar, the GNU userland (coreutils, diffutils, findutils, gawk, grep, patch, sed), readline, bash, binutils, gcc, make, kpkg, kinstall | `cross fs mark` |
| `20_selfhost` | Self-Hosting Bootstrap | Chroot | `packages.txt`: fortify-headers first, then tar, musl, zlib, binutils, diffutils, m4, gawk and gcc rebuilt inside the chroot, with what they depend on; gcc's install hook removes the `x86_64-kdos-linux-musl` trees `10_bootstrap`'s compiler left | `fs` |
| `30_foundation` | Build Foundation | Chroot | `packages.txt`: build systems, perl and python3, base libraries, archive, TLS and documentation tooling, ccache | `fs` |
| `31_compilers` | Compilers | Chroot | `packages.txt`: LLVM, clang, lld and their pinned 21 series, compiler-rt, libunwind, openmp, rust, cargo-c, bindgen, cbindgen, go, ghc, cabal-install, pandoc, zig, nodejs, ruby, asciidoctor, the ccache manual page | `fs` |
| `40_lang` | Languages | Chroot | `packages.d/`: language modules, language implementations, build, documentation and developer tools, version control | `fs` |
| `41_system` | System | Chroot | `packages.d/`: everything with no graphics and no toolkit in its dependency closure, among it services, networking, storage, command-line tools, codecs, firmware, fonts, science and hardware libraries, and KDOS's theme, icons, cursors, splash, the two books and the pack tools | `fs` |
| `42_graphics` | Graphics Stack | Chroot | `packages.d/`: the Wayland and X11 libraries, Xwayland, Mesa, the media frameworks, and `kdos-tools` | `fs` |
| `43_toolkits` | Toolkits | Chroot | `packages.d/`: GTK, Qt 5 and 6, KDE Frameworks and the libraries built on them | `fs` |
| `44_apps` | Applications | Chroot | `packages.d/`: the natively ported graphical applications, and `kdos-appbox` | `fs` |
| `50_desktop` | Desktop | Chroot | `packages.txt`: wlroots, `kdos-comp`, `kdos-shell`, `kdos-term`, `kdos-lock`, `kdos-res`, `kdos-boxsock`, `kdos-record`, the five root daemons, fcitx5 and its engines, the portals | `fs` |
| `60_kernel` | Kernel | Chroot | `packages.txt`: `dwarves` and `linux` | none |
| `70_image` | Image | Chroot | 16 scripts: see [The packaging steps](#the-packaging-steps) | none |

How many ports each list names and installs is counted in
[The ports catalogue](../06-reference/ports-catalogue.md#the-phases).

"Container" means the build container itself, running as root; "Chroot" means inside `build/fs`,
described in [The chroot](#the-chroot). A phase runs in the chroot when its `phase.env` sets
`CHROOT=1`. The names are the non-comment lines of the phase's list; the ports installed are what
the phase installs after the phases before it, since a name an earlier phase already installed
from the same recipe is skipped.

**The numbering has gaps.** Phases are numbered in bands of ten, and the phases that divide one
stage are numbered inside its band: `30` and `31`, `40` to `44`. A new phase takes a free number
between two others, and no existing phase, snapshot, log directory or page has to be renamed for
it. Steps inside a script phase are numbered the same way (`060_gzip.sh`, `061_tar.sh`,
`062_coreutils.sh`), so the order they run in is the order their numbers say.

The first two phases build a cross toolchain and use it to cross-compile a minimal userland into
`build/fs`, including `kpkg`, the package manager, and `kinstall`, the installer. Both are compiled
directly from `src/system/kdos-kpkg` and `src/system/kdos-installer` rather than installed as
ports, because nothing can be installed as a port until `kpkg` exists. `kdos-kpkg` has no recipe
at all, and `kdos-installer`'s recipe is named in no phase's list. The other ports of `src/system`
and `src/art` are built in `41_system`, `42_graphics` and `44_apps`, each where its dependencies
first exist. From `20_selfhost` on, the
build is self-hosting: the package phases are lists of ports that `kpkg` builds inside `build/fs`
with the compilers `20_selfhost` rebuilt there, and `70_image` runs its scripts in the same chroot.

The desktop is a phase of its own for two reasons. It is the only phase whose package search path
includes `src/desktop` and `src/daemons`, which hold the compositor, the shell and the root daemons.
And it gives the userland phases a snapshot of their own below the desktop: restoring `44_apps`
re-runs the 22 ports the desktop's list installs, then `60_kernel`'s `dwarves` and `linux`, which
that tree does not yet hold, then `70_image`. A change to one desktop program needs none of that; a
build plan, `--phases 50_desktop,70_image --rebuild <port>`, rebuilds that port on the current tree.
The kernel is the last port phase before the image.

### Package lists

A package phase's list is either one `packages.txt` or a directory, `packages.d/`, of `*.txt`
files. The files of a `packages.d/` are read in byte order, the order `LC_ALL=C` sorts in, as if
they were one file; a file whose name does not end in `.txt`, or starts with a dot, is not part of
the list. In each file a line is one port name, and blank lines and lines starting with `#` are
ignored.

The five userland phases, `40_lang` to `44_apps`, use `packages.d/`, one file per shelf:

- `<shelf>.txt` holds that shelf's ports, for example `41_system/packages.d/network.txt`.
- `src-<area>.txt` holds KDOS's own ports from one `src/` area, for example
  `42_graphics/packages.d/src-system.txt` with `kdos-tools`.
- `00-order.txt`, in a phase that has one, sorts before every other file and holds the runs whose
  order a comment pins, whatever shelves their ports are on.

The other package phases keep a single `packages.txt`. In `30_foundation`, `31_compilers` and
`50_desktop` a shelf heading, a comment line of the form `# <shelf> — <description>`, opens the
ports of each shelf, and names written ahead of the first heading are that list's pinned run, the
counterpart of `00-order.txt`. The two shortest lists, `20_selfhost`'s and `60_kernel`'s, have no
headings. The orchestrator ignores comments; the headings are read only by
`testing/phaseclosure.py`. Preflight checks that every `packages.d/` file is named after a shelf
`ports/shelves` lists or a `src/` area that holds ports, and holds only ports filed there.

**Where a new port is listed.** A port goes in the list of a phase whose subject it fits and in
which every port it depends on is either installed by an earlier phase or named in the same list,
and in that phase's file for its shelf:
`41_system/packages.d/network.txt` for a port filed at `ports/core/network/<name>/`, or
`src-<area>.txt` for one of KDOS's own. In a phase with a single `packages.txt` it goes under its
shelf's heading, and a shelf the list has no heading for yet gets one. A port whose dependencies
reach Wayland but no toolkit belongs in `42_graphics`, a graphical application built on a toolkit
in `44_apps`, and a port an earlier phase's port depends on in that earlier phase;
`testing/phaseclosure.py` names the phases involved when the choice is wrong. See
[Writing ports](writing-ports.md) for the rest of adding a port.

`kdosbuild` refuses to start a build, and exits with status 2, when a phase has both a
`packages.txt` and a `packages.d/` (which list is meant would be a guess), when a `packages.d/`
holds no `.txt` file, or when a phase has no list and no script: each would install nothing and
report success. It names the phase on every run, `--list` included. It also refuses when
`script/phases/` holds no phase at all.

### Every phase installs exactly its list

A list names every port its phase installs, and nothing else. From `30_foundation` on this is
checked: the ports a phase's list pulls in through `depends =`, less everything the phases before
it installed, must be exactly the ports the list names, less the same. `testing/phaseclosure.py`
computes it the way `kpkgdepends` resolves, and preflight runs it. A port a phase installs without
naming it fails the check, with the phase, the chain of dependencies that pulled it in, and the
later phase that names it if one does. The check also refuses a name that is not a port on the
phase's `PORT_REPO`, a name listed twice in one phase, a phase with both kinds of list, and a
dependency that resolves only on a later phase's `PORT_REPO`, since that port cannot be built where
the phase needs it. `20_selfhost` is not held to it: its nine names install fifteen ports.

A name an earlier phase already installed, a **re-list**, is accepted only in a list's pinned run:
`00-order.txt` in a `packages.d/`, or the names ahead of the first shelf heading in a
`packages.txt`. In a shelf's file or under a shelf's heading it is refused, naming the phase that
installs the port, so a shelf's list cannot claim a port another phase builds.

The four phases from `41_system` to `44_apps` are also held to what a port's closure reaches, by
the table in [Which phase lists a port](writing-ports.md#which-phase-lists-a-port): a
`41_system` port reaches neither the graphics stack nor a toolkit, a `42_graphics` port reaches
the graphics stack and no toolkit and is not an application (a port nothing depends on, filed on
an application shelf), a `43_toolkits` port reaches a toolkit, and a `44_apps` port
is one nothing depends on whose closure reaches a toolkit, or the graphics stack from an
application shelf. A port listed against its row fails with the phase the table gives it.

The rule is what makes a phase mean something. A port cannot quietly move into an earlier phase
because something there started depending on it, and a list cannot reach forward into a later one.
A new `depends =` entry that breaks the layering fails preflight, naming the port and both phases,
instead of changing what a snapshot holds.

Between them the lists hold about 2,100 names. A port already installed from the same recipe is
skipped when a later list names it again, so a repeated name costs nothing unless its recipe
changed, and then it rebuilds that port at the point the list names it. `30_foundation` uses this
on purpose, and is the only list that does. Its pinned run opens with `fortify-headers`, then the
18 build tools recipes use without naming them in `depends` (`musl`, `gcc`, `binutils`, `make`,
`file`, `muon`, `pkgconf`, the autotools, `m4`, `bison`, `flex`, `cmake`, `meson`, `ninja`,
`python3` and `perl`), then `bash` and the GNU userland every `build.sh` runs in (`coreutils`,
`grep`, `which` and `musl-utils`), so a changed one is rebuilt before anything in the phase uses
it. The rest of what `20_selfhost` installs follows for the same reason: `tar`, `diffutils`,
`gawk` and `zlib` from its list, and `ncurses` and `readline`, which its dependencies pull in.
Nine of the run's names repeat `20_selfhost`'s list: `fortify-headers`, `musl`, `gcc`,
`binutils`, `m4`, `tar`, `diffutils`, `gawk` and `zlib`.

`41_system/packages.d/00-order.txt` holds only its header comment and names no port, so
`41_system` has no pinned run; a run goes there when a comment has to pin its order.
No other userland phase has an order file.

The lists install every recipe in the tree but two. `kdos-installer` is compiled directly by
`10_bootstrap`, and `texlive-doc` is listed in `ports/unbuilt`, the recipes kept for a
`kpkg install` on a machine that wants them and built by no phase; `040_orphans.sh` removes a
package `ports/unbuilt` names from the image if an earlier build left it installed. The four ports
no list names, `gmp`, `mpfr`, `mpc` and `xxhash`, are `20_selfhost`'s dependencies.
`testing/bookcounts.py`, which `testing/docscheck.sh` runs, fails on a recipe no phase installs
unless `ports/unbuilt` names it, and on a name there that a phase does install. [The ports
catalogue](../06-reference/ports-catalogue.md) lists every port by shelf with the phase that
installs it. Shelves, and the recipe's own `group =` key, which is a different thing, are explained
in [Packaging](../03-architecture/packaging.md#the-group-key).

### Where kpkg looks for a port

Where `kpkg` looks for a recipe is set per phase by `PORT_REPO` in its `phase.env`:

| Phase | Port repositories |
|---|---|
| `20_selfhost`, `30_foundation`, `31_compilers` | None set, so `kpkg` takes the list in the tree's `/etc/kpkg.conf`: `/ports/core` and four `/ports/src/` directories that do not exist in the build chroot, leaving `/ports/core` |
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

Inside a repository, `kpkg` looks for `<repo>/<name>/` and then for `<repo>/<shelf>/<name>/`:
`ports/core` holds its ports one shelf down, and each `src/` area holds its own directly. A name
found at two places inside one repository is an error that names both paths, and `kpkg` stops
rather than pick one. A recipe nested below its shelf is refused by the walk that lists every port
and is not found by a lookup. A name held by two repositories is taken from the first; preflight
refuses a name filed twice anywhere in the tree, across every shelf and `src/` area, so in this
repository that never decides anything. Shelves are never listed on `PORT_REPO` itself, which holds
at most eight repositories; an entry past the eighth is dropped with a warning.

## How a phase runs

When the orchestrator reaches a phase, it expands the phase into steps and runs them one at a time;
with `--port-jobs` a package phase runs several of its ports at once, level by level (see
[Building a package phase by level](#building-a-package-phase-by-level)). How the steps are made
depends on what the phase holds.

**A script phase** has one step per `*.sh` file in its directory, in sorted order (a name starting
with a dot is ignored). The step runs `bash <script>` in the container, or
`script/chroot/exec.sh bash <script>` for a chroot phase. The script sources its own phase
environment with a `source script/phases/<phase>/phase.env` line near its top; the one exception is
`70_image/100_packs.sh`, which needs nothing from it. Preflight checks that every file a build
script sources exists, and that a step sources its own phase's `phase.env` and not another's.

**A package phase** is resolved first. The orchestrator runs `kpkgdepends`, the `kpkg` tool that
prints an install order, through the same wrapper:

```sh
source script/phases/41_system/phase.env && export PKGDB_DIR=/dev/null && kpkgdepends <every name in the list>
```

Pointing `PKGDB_DIR` at `/dev/null` makes the package database look empty, so `kpkgdepends` prints
the full install order for the list and its whole dependency closure, not only what is missing.
Each name in that order becomes a row of the phase, and each row that has work to do is a step:

```sh
source script/phases/41_system/phase.env && kpkg install <port>
```

A port the [build plan](#build-plans) forces with `--rebuild` gets `kpkg install -f <port>`.

Before the first step runs, the orchestrator asks of every row the question `kpkg install` would
ask, on the host, without entering the chroot: is this port installed, and does its recipe hash
still match the port as it stands? It reads the database the chroot reads,
`build/fs/var/lib/kpkg/db`, and finds each port in the phase's `PORT_REPO` mapped onto the
repository (`/ports/...` is `ports/...`, `/kdos/...` is the repository root), with the check that
`KPKG_STRICT_RECIPE=1` turns on. A port that is installed and current is marked done with the note
`installed`; it stays in the list, dimmed, so the phase still shows its whole closure. That first
answer is provisional: when the run reaches the row, the orchestrator asks again, so a recipe or a
library edited while earlier ports built still reaches a later port in the same run, as it would
have if `kpkg` had asked when the step started. A row still current is passed over and never runs,
and is never announced as a running step; a row that is not current by then loses its mark and its step runs.
The phase finishes, and its snapshot counts it complete, only once every marked row has been asked
again. In a phase re-run on a
built tree that is most of the order, and each of those rows would otherwise cost a chroot entry to
print `Skipping`. Three kinds of row always run: a port the build plan forces with `--rebuild`, a
port the host cannot find in exactly one place under those repositories, and every row of a phase
whose `phase.env` names `PKGDB_DIR`, `KPKG_ROOT`, `KPKG_CONF` or `--root`, whose database the host
cannot follow. The decision is `libkpkg`'s own `kp_installed_current()`, so the host and `kpkg` have
one rule between them, and `kpkg` asks it again for every step that does run: a port whose
`kpkgbuild`, `build.sh`, `postinstall.sh` or patches changed is rebuilt without being named. See
[Deciding what to rebuild](../03-architecture/packaging.md#deciding-what-to-rebuild). The
completion panel and the `--json` result count these rows as `installed`, apart from the steps that
ran.

Every install goes through `kpkgadd`'s conflict scan, forced rows included. A port that installs
a path another package owns fails its step with `File conflict detected`, naming each path and its
owner, unless its recipe names that owner in `replaces =`; the declared path then changes hands. See
[Who owns a file](../03-architecture/packaging.md#who-owns-a-file).

The orchestrator reads what `kpkgdepends` prints, so that output has to be clean. `kpkgdepends`
writes the install order to standard output and nothing else, the orchestrator reads its standard
error separately, and `script/chroot/exec.sh` sends its own diagnostics to `build/logs/chroot.log`.
Every token read back must look like a package name (it starts with a letter or digit and holds
only letters, digits, `.`, `_`, `+` and `-`), so stray output fails the phase instead of being
installed as a package. Resolution also fails when `kpkgdepends` exits non-zero, prints nothing, or
resolves more than 4096 packages, the most one package phase may hold and the most `kpkg`'s
resolver returns; a phase that silently built only the first 4096 would report success. A resolution
failure is written to `build/logs/<phase>/expansion.log`.

A phase whose list names no ports, or whose plan selects none of its scripts, finishes at once.

### Building a package phase by level

`kdosbuild --port-jobs N`, given through `BUILD_ARGS` (`make build BUILD_ARGS="--port-jobs 3"`),
builds up to `N` ports of a package phase at once, `N` from 1 to 8. The default, 1, is the serial
run described above, with the same steps and the same commands. Script phases always run one step
at a time.

With `N` above 1, the orchestrator first runs `kpkg help` through the phase's wrapper; a `kpkg`
without `--build-only`, as a tree restored from an old snapshot can carry, runs the phase serially
and says so in a notice. Otherwise it gives every port of the resolved order a **level**, on the
host, from the recipes the phase's `PORT_REPO` finds:

- A port's edges are the names on its `depends =` line that are ports of the same phase and come
  earlier in the order. A port the host cannot find in exactly one place depends on every port
  before it.
- The list's **order run**, `00-order.txt` in a `packages.d/` or the names ahead of the first shelf
  heading in a `packages.txt`, is a serial prefix: every port up to the last of its names in the
  resolved order depends on the port before it, and every later port depends on that last one. A
  run a comment pinned stays one at a time, and finishes before anything else of the phase starts.
- A port's level is one more than the highest level among its edges.

The rows of the phase are then its ports level by level, each level in the resolved order and
followed by one commit step titled `install L<n> (<count>)`. The ports of a level build side by
side, each with:

```sh
export KDOS_JOBS=<k> KDOS_JOBSERVER=/kdos/build/jobserver && source script/phases/41_system/phase.env && kpkg install --build-only <port>
```

`--build-only` builds the port into the package cache and installs nothing. Once every port of the
level has built, the commit step runs `kpkg install --commit <ports>`, which installs them one at a
time in the resolved order. Nothing is installed while a build runs, so every port builds against
the earlier phases and the lower levels and nothing else: what it sees is decided by the tree, not
by which port finished first. `--port-jobs 2` and `--port-jobs 3` therefore build the same thing as
the serial run, provided every recipe declares what it builds against (see
[Writing ports](writing-ports.md)).

The ports of a level share the jobs through a **jobserver**, GNU make's job-slot protocol. `J` is
`KDOS_JOBS` as the build was given it, or else every CPU the orchestrator may use. The orchestrator
makes a FIFO at `build/jobserver`, which every chroot entry sees at `/kdos/build/jobserver`, and
puts `J − 1` tokens in it; each client holds one job without a token and takes a token for every
job beyond it. `common.env` hands the FIFO to GNU make through `GNUMAKEFLAGS` and to cargo through
`CARGO_MAKEFLAGS`, and leaves `CMAKE_BUILD_PARALLEL_LEVEL` and `CARGO_BUILD_JOBS` unset, so `cmake
--build` and cargo reach it rather than a fixed count; `script/bin/ninja` gives ninja the FIFO in
place of `-j`. A port building alone therefore runs `J` jobs, and ports side by side share `J` as
each needs it: one that is configuring, linking or packaging leaves its slots to the others. The
FIFO is refilled to `J − 1` whenever a level opens, when no port holds a token, so a client killed
while holding one costs that level a slot and no more. It is removed when the orchestrator exits.

Each port's own `KDOS_JOBS` is still `k = J / N` (at least 1), and `MAKEFLAGS` stays `-jk`: that is
the count a recipe gets when it passes one itself, to rust's `x.py`, a GHC or a waf build, which
take no part in the jobserver. A ninja call whose `-j` is exactly `k`, as `ninja $MAKEFLAGS`
passes, is taken for the build's own count and joins the jobserver; any other `-j` is the
recipe's. Go sizes itself from the CPUs it may use, and with a jobserver no port is pinned to a
subset of them.

When the FIFO cannot be made, the level runs without a jobserver: each port gets the fixed count
`k`, and each running port is held to a window of `2k` of the orchestrator's CPUs (all of them if
that is fewer), the windows spread across the CPUs and overlapping, so that cargo, rustc and go
size themselves to their window rather than to the whole machine.

A free slot takes the level's next pending port in the resolved order, and only while
`MemAvailable` is at least a quarter of `MemTotal`; with nothing running a port always starts. A port the host found installed and current is asked again when its level opens,
as the serial run asks when it reaches the row. A port `--build-only` reports as current builds
nothing and is left out of the commit, and a commit with nothing to install is marked done without
running.

**A failure drains the level.** The first port to fail is the build's failed step; no port starts
after it, the ports already running finish, and the build then stops. The level is not committed:
the packages its ports built stay in the package cache, each recorded in a
`<name>.pending` file beside it, and a resumed build's `--build-only` reuses one whose recipe hash
still matches rather than building it again. `Q` sends `SIGTERM` to every running port's process
group, and a second `Q` sends `SIGKILL`.

**Ownership.** A recipe takes paths through `replaces =` only from a package installed before it:
the replaced package installing after its taker is refused like any other conflict. Within a commit
the order is the resolved order, as in the serial run. A port that takes paths from a port of the
same phase that the resolved order puts after it succeeds here when that port was committed at a
lower level, where the serial run would refuse the later port. The commit step watches for
kpkgadd's `Taking <n> path(s) from <port>` and names such a pair in a notice, `<taker> took paths
from <port>; the serial order had <port> last — pin <port> before <taker> in 00-order.txt`; pinning
the pair in that order makes the serial run install them the way the levels did.

The header shows how many steps are running in place of the running step's time. A phase's
duration is the wall-clock span of its steps.

### Running a step

Each step runs in a child process with standard input from `/dev/null` and standard output and
standard error merged into one pipe. The child is placed in its own session, so that stopping a
step reaches the whole process tree under it (`make` and every compiler it started), not only the
`bash` at its top. A stop sends `SIGTERM` to the group, then `SIGKILL` after five seconds, and
after ten seconds the orchestrator stops waiting for the step, records it as failed with status
143, and ends the run.

A step that exits non-zero stops the build: the step and its phase are marked failed and nothing
after it runs. A port that fails inside a level first lets the ports running beside it finish. A step that cannot be started at all, because the pipe or the fork is refused on a
machine out of file descriptors or memory, fails the same way with return code 999 and a named
notice.

The orchestrator exports `KDOS_REPLAY` to every step: `1` for a step a build plan named explicitly,
`0` otherwise (see [Build plans](#build-plans)).

### Guards in the early phases

Every script in `00_cross` and `10_bootstrap` except `000_file_system.sh` starts with a guard: if
its marker file under `build/mark/cross/` or `build/mark/bootstrap/` exists, it exits 0 at once.
The expensive early work is therefore done once per tree, and a re-run of these phases costs
seconds. `120_kpkg.sh` stores a hash of every source file `kpkg` is compiled from in its marker
instead of an empty file, so a change to `kpkg` or its libraries rebuilds it on the next run. Every
guard stands down when `KDOS_REPLAY=1`. `000_file_system.sh` has no guard and syncs `fs/` on every
run.

These scripts find a port's recipe and sources through `script/lib/port.sh`, since `kpkg` does not
exist yet. It finds a port only one shelf down, at `ports/core/<shelf>/<name>/`; a recipe loose at
`ports/core/<name>/` is not found. Exactly one shelf may hold the name; a second is an error naming
both, rather than a toolchain built from whichever recipe a glob happened to list first.

### Step logs

Every step writes its complete output to its own file:

```text
build/logs/<phase directory>/<NNNN>_<name>.log
```

`NNNN` is the step's position within its phase, counted from zero. For a script step, `<name>` is
the script's file name with its numeric prefix removed; for a package step it is the port name
followed by `.install`, and for the commit step of level `n` it is `L<n>.commit`. For example:

```text
build/logs/10_bootstrap/0000_file_system.sh.log
build/logs/42_graphics/0123_mesa.install.log
build/logs/42_graphics/0131_L7.commit.log
```

Under `--port-jobs` the positions follow the rows, level by level, and each running port writes its
own file.

The file is verbatim, escape sequences included. The full-screen interface keeps only the last
2000 lines of each step in memory; the file keeps everything. Three further files sit under
`build/logs/`:

| File | Holds |
|---|---|
| `snapshots.log` | Every notice the build shows (snapshots taken, skipped or failed, restores, plans, stop requests), with a time stamp, so a notice survives the interface that displayed it |
| `chroot.log` | Warnings from `script/chroot/exec.sh`: a cgroup tree, or a `docs/`, `testing/`, `Makefile` or `Dockerfile` it could not bind read-only, an open-files limit it could not raise |
| `<phase>/expansion.log` | Why a package phase could not be resolved |

Reading a failed step's log is covered in [Build troubleshooting](build-troubleshooting.md).

### Step timings

The orchestrator records how long each step took in `build/snapshots/timings.json`, which sits
beside the snapshots and is never part of one. It is a JSON object with four members. `steps` maps
`<phase directory>/<step>` to seconds, where a script step is named by its title and a port by its
name; `phases` maps a phase directory to the wall-clock seconds of the whole phase; `machine` maps
each key of the two to the machine that measured its figure; and `machines` describes each machine
by what the run was given to build with:

```json
{
 "steps": {
  "00_cross/binutils": 43.41,
  "10_bootstrap/linux headers": 7.48,
  "41_system/zlib": 1.44
 },
 "phases": {
  "00_cross": 381.09
 },
 "machine": {
  "00_cross/binutils": 1,
  "10_bootstrap/linux headers": 1,
  "41_system/zlib": 2,
  "00_cross": 1
 },
 "machines": {
  "1": {"cpu": "AMD Ryzen 9 7950X 16-Core Processor", "cpus": 32, "memory": 66000000000, "jobs": 0},
  "2": {"cpu": "AMD Ryzen 9 7950X 16-Core Processor", "cpus": 32, "memory": 66000000000, "jobs": 6}
 }
}
```

A machine is the first `model name` of `/proc/cpuinfo` (`unknown` on a processor that has none),
the number of CPUs the orchestrator may run on, `MemTotal` in bytes, and `KDOS_JOBS` as the build
was given it, `0` when it was unset and each step worked it out from the CPUs and the memory
([the phase environment](#the-phase-environment)). They are read inside the container: `cpus` is
the orchestrator's CPU affinity, which the `--cpus` cap `make build` sets from `KDOS_JOBS` does not
narrow, so `jobs` is what records that cap, and `memory` is the host's. Two runs that differ in any of
the four are two machines, so a run with `KDOS_JOBS=6` files its figures apart from one without.
An SBU is a ratio of two figures and holds only between figures of one machine, which is what the
`machine` map lets a reader tell; a machine no figure names any more is dropped when the file is
written.

Every phase is recorded, `70_image` included. Each figure is the last measured time, not an
average, rounded to two decimals. A step's figure is written when the step succeeds, and every
write is a merge: the file is read, the step's key and its machine are set, and the whole file is
written back atomically. A `--continue-from`, `--phases` or `--steps` run therefore changes the
keys it measured and keeps every other one, each still naming the machine of the run that measured
it, so the figures of one file may come from several runs and machines. A file that does not parse
is started again from the run's own figures.

Only a step that did its work is recorded, so a step that did nothing never replaces the figure of
one that did:

| Step | Recorded when |
|---|---|
| A script | It wrote any output. The guard of a `00_cross` or `10_bootstrap` script exits in silence, so a guarded script keeps the figure of the run that built it |
| A port | `kpkg` printed `Building <port>...` or `Rebuilding <port> (forced)...` for that port and for no other. A port skipped as installed and current, reused from the package store or already built under `--build-only` keeps its earlier figure. A serial `kpkg install` that also built a dependency it found missing is not recorded, because its time covers both ports |
| A commit step | Never; it installs and builds nothing |

A phase's wall clock is recorded only when every row of it was measured in that run, so a phase
that skipped installed ports, ran a guard, was retried, or ran only the steps a plan named keeps its
earlier figure. The `kpkg` step a [retry](#retrying-a-failed-phase) runs ahead of a phase is filed
as `10_bootstrap/kpkg`, the script's own phase, whichever phase it runs ahead of.

`make book-data` copies `steps` into the KDOS from Scratch book under `docs/kfs`, whose build-time
column counts in units of `00_cross/binutils`, with the machine each figure names; its stamp
describes the machine that measured the unit, and its build-times appendix marks a figure from
another one. `make cleanbuild` keeps the file with the snapshots;
`make clean` removes it.

## The phase environment

Each phase's environment is `phase.env` in its own directory: `41_system` reads
`script/phases/41_system/phase.env`. The name does not end in `.sh`, so the step discovery of a
script phase never takes it for a step. The settings every phase shares are written once, in three
`.env` files under `script/env/`, and each `phase.env` sources one of them:

| File | Sourced by | Holds |
|---|---|---|
| `common.env` | Every phase, through one of the two below | The settings that make packages reproducible (`SOURCE_DATE_EPOCH`, `TZ`, `LC_ALL`, `-ffile-prefix-map`, `--build-id=sha1`), described in [Reproducible packages](../03-architecture/packaging.md#reproducible-packages); the job count `KDOS_JOBS` (when not given, the thread count clamped to one job per 2 GiB of memory) exported as `MAKEFLAGS=-j$KDOS_JOBS`, `CMAKE_BUILD_PARALLEL_LEVEL` and `CARGO_BUILD_JOBS`, or, given a jobserver in `KDOS_JOBSERVER`, `GNUMAKEFLAGS` and `CARGO_MAKEFLAGS` naming it in place of the last two; `KPKG_STRICT_RECIPE=1` |
| `host.env` | `00_cross` and `10_bootstrap` | The target triplet, the paths of the workspace, `build/`, the sysroot and the cross toolchain, `pkg-config` pointed at the sysroot, the cross toolchain first on `PATH`, and the base compiler flags. It empties `build/tmp` |
| `chroot.env` | `20_selfhost` onwards | `PKG_CONFIG_PATH`, the compiler named outright (`CC=gcc`, `CXX=g++`), the release compiler and linker flags with fortification (`CFLAGS`, `CXXFLAGS`, `FFLAGS`, `FCFLAGS`, `LDFLAGS`), and after `common.env` the defaults for the build systems that do not read `CFLAGS` (`CMAKE_BUILD_TYPE=Release`, `CARGO_PROFILE_RELEASE_DEBUG=0`, `GOFLAGS`, `CGO_CFLAGS`, `CGO_CXXFLAGS`, `CGO_LDFLAGS`), all set out in [Writing ports](writing-ports.md#the-release-flags); `script/bin` first on `PATH` once `/usr/bin/ninja` exists, whose `ninja` adds `-j$KDOS_JOBS` to a ninja call that names no job count and whose `meson` adds `--wrap-mode=nodownload` to a setup that names no wrap mode; the CMake compiler cache; `ac_cv_prog_cxx_cxx11` set empty; `LANG` and `LC_ALL` set to `C.UTF-8` over `common.env`'s `C`; `PIP_ROOT_USER_ACTION=ignore`; `CONFIG_SITE`, naming `script/env/config.site`, which puts an Autoconf build's `sysconfdir`, `localstatedir` and `sharedstatedir` at `/etc`, `/var` and `/var/lib` under `--prefix=/usr`; from `40_lang` on, the Rust `-sys` switches that link the system library (`LIBGIT2_NO_VENDOR`, `RUSTONIG_SYSTEM_LIBONIG`, `LIBSQLITE3_SYS_USE_PKG_CONFIG`, `ZSTD_SYS_USE_PKG_CONFIG`); `TERM=dumb`; `KPKG_SKIP_INDEX=man`, which leaves the manual index to `70_image`; and `KPKG_STORE_BASE`, the toolchain and base-userland ports every [package store](../03-architecture/packaging.md#the-package-store) key depends on. It removes nothing: `kpkg` empties each port's own work directory before building it and again after a successful build |

`common.env` appends its flags to `CFLAGS`, `CXXFLAGS` and `LDFLAGS`, so `host.env` and
`chroot.env` set their base flags first and source it after them. A base assignment made after it
would drop the reproducibility flags without a word; `chroot.env`'s `CGO_*FLAGS` are copied after
it for that reason.

A `phase.env` carries what is the phase's own: a metadata block for the orchestrator at the top,
`CHROOT=1` for a chroot phase, `PORT_REPO` where the phase needs more than the default, and after
sourcing the shared file, whatever differs. The `30_foundation` file is typical:

```bash
# --- build-system metadata ---
# The orchestrator parses these from this file's own text and follows no
# `source`, so they stay here rather than in a shared file.
export KDOS_PHASE_TITLE="Build Foundation"
export KDOS_PHASE_DESC="build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/* fs/root/.cache/* fs/root/.cargo/* fs/root/.npm/* fs/var/tmp/*"

export CHROOT=1

export LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:/usr/lib64:/usr/local/lib64"${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

source "${BASH_SOURCE[0]%/*}/../../env/chroot.env"
```

What the phases set beyond the shared files:

| Phase | Sets |
|---|---|
| `00_cross`, `10_bootstrap` | `MARK`, the marker directory `build/mark/cross` or `build/mark/bootstrap` (see [Guards in the early phases](#guards-in-the-early-phases)) |
| `20_selfhost` | Unsets `CC`, `CXX` and `ac_cv_prog_cxx_cxx11`: it builds the native compiler every later phase names, with only the bootstrap gcc installed, so its configure scripts find their own compiler and C++ mode |
| `30_foundation`, `31_compilers` | `LD_LIBRARY_PATH` over `/usr/lib`, `/usr/local/lib` and the `lib64` pair |
| `40_lang` to `44_apps`, `50_desktop`, `60_kernel` | `PORT_REPO` (see [Where kpkg looks for a port](#where-kpkg-looks-for-a-port)) |
| `70_image` | `KDOS_ACCENT`, the colour scheme the image ships in, which `050_theme.sh` checks against `libkcolor`'s compiled default; unsets `PKG_CONFIG_PATH` and `ac_cv_prog_cxx_cxx11`, since the phase builds no port and a tool it runs should see the chroot's own defaults |

The compiler is named rather than discovered because some `configure` scripts prefer clang once it
is installed, and a distribution that builds itself cannot let its toolchain depend on which ports
are present. The empty `ac_cv_prog_cxx_cxx11` stops an Autoconf `configure` lowering the C++
standard to C++11 (see
[Build troubleshooting](build-troubleshooting.md#autoconf-lowering-the-c-standard)).

The orchestrator **parses** each `phase.env` and never sources it. `host.env` removes a work
directory (`rm -rf $BUILD_DIR/tmp`), and sourcing a `phase.env` in the orchestrator's own process
would run that removal, and every other line, in the orchestrator's process instead of the step's.
The parser therefore:

- reads only the `phase.env` file's own text, and follows no `source` line;
- reads only lines of the form `export NAME=VALUE`;
- honours only five keys: `CHROOT`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC`, `KDOS_SNAPSHOT_PATHS` and
  `KDOS_SNAPSHOT_EXCLUDE`;
- takes the value as a literal with no expansion. A quoted value ends at its closing quote, and an
  unterminated quote reads as empty. An unquoted value ends at whitespace or a `#`.

Because it follows no `source`, those five keys are written in every `phase.env` and never in a
shared file: `CHROOT=1` written only in `chroot.env` would run every chroot phase in the
container. `testing/phaseclosure.py` reads `PORT_REPO` from the same text for the same reason.
`CHROOT=1` selects the chroot wrapper; any other value, or no line, runs the phase in the
container. A phase with no `phase.env`, or no title in it, is titled after its short name with
underscores and hyphens turned into spaces. The snapshot keys are described in
[Snapshots](#snapshots).

### `env -i` means every variable must be named

The chroot is entered with an empty environment (`env -i`). Only these variables reach a step
inside it:

| Variable | Value | Read by |
|---|---|---|
| `HOME` | `/var/tmp/kdos-home`, which `exec.sh` creates (mode 0700) before each entry; snapshots leave `var/tmp` out and `110_iso.sh` excludes the directory, so no tool's dotfiles, caches or registries reach the image | Everything |
| `TERM` | The caller's | Everything |
| `PATH` | `/usr/bin:/usr/sbin:/bin:/sbin:/usr/local/bin`, with `/kdos/script/bin` put in front by `chroot.env` once ninja is installed; the directory holds the `ninja` and `meson` wrappers and is not in the image | Everything |
| `KDOS_REPLAY` | `0` or `1` | A step whose "already done" guard must stand down because a plan named it (see [Build plans](#build-plans)) |
| `KDOS_JOBS` | A number, or empty | `script/env/common.env`, which computes the job count inside the chroot when it is empty |
| `KDOS_ISO_SOURCES` | `0` or `1` | `70_image/110_iso.sh`, to copy the sources onto the ISO (see [The packaging steps](#the-packaging-steps)) |
| `KDOS_PACK_KDOS` | `0` or `1` | `70_image/100_packs.sh`, to pack this root filesystem as the base pack `kdos`, and `110_iso.sh`, to put that pack on the ISO |
| `KDOS_MAKE_BINHOST` | `0` or `1` | `70_image/010_binhost.sh`, to write the packages this build made into a signed binhost |
| `KDOS_ISO_COMP` | `xz`, `zstd:1` to `zstd:22`, or empty for `zstd:15` | `70_image/110_iso.sh`, as the codec of `system.sfs`; any other value stops the step |
| `KDOS_CCACHE` | `1` (the default) or `0` | `script/env/chroot.env`, which sends CMake compiles through ccache into `build/ccache` once the `ccache` port is installed, and sets `CCACHE_DISABLE=1` otherwise (see [The compiler cache](how-kdos-is-built.md#the-compiler-cache)) |
| `KPKG_KEEP_CACHE` | The value of `KDOS_MAKE_BINHOST` | Every chroot `kpkg install`, which then keeps the package it built, compressed with `xz -9` instead of the `xz -0` a deleted package gets, for `010_binhost.sh` to index |
| `KDOS_PKG_STORE` | `0` (the default), `1` or `check` | `script/chroot/exec.sh` itself, which sets the next two from it |
| `KPKG_STORE` | `/kdos/build/pkgstore` when `KDOS_PKG_STORE` is `1` or `check`, else empty | Every chroot `kpkg install`, which installs a port from the [package store](../03-architecture/packaging.md#the-package-store) when its key is there and stores what it builds |
| `KPKG_STORE_CHECK` | `1` for `check`, else `0` | `kpkg install`, which then builds every store hit and logs a difference to `build/logs/pkgstore-check.log` |
| `KPKG_STORE_SALT` | Set by kdosbuild on the host when the store is on | `kpkg install`, as the store key's hash of the bootstrap phases |
| `KDOS_PKG_STORE_MAX` | A size, default `60G` | `70_image/015_pkgstore.sh`, the cap the store is evicted down to |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY`, `KDOS_GIT_DATE` | The short commit, `1` when the tree had unrecorded changes, and the commit's date as `YYYY-MM-DD` | `70_image/045_identity.sh`, which stamps them into `/etc/os-release` |
| `KDOS_GIT_TIME` | The commit's time in Unix seconds | `70_image/105_ports.sh`, which stamps it with the commit into `/ports/TREE` |

A variable the `Makefile` passes into the build container but `script/chroot/exec.sh` does not
name reaches the container phases and none of the chroot ones. `70_image` is a chroot phase, so an
opt-in packaging flag that is not forwarded silently produces an ordinary image, and a `KDOS_JOBS`
that is not forwarded is recomputed in the chroot from the host. Adding any build variable is
therefore two edits: the `Makefile` passes it into the container with `-e`, and
`script/chroot/exec.sh` names it on the `env -i` line.

`/usr/local/bin`, where KDOS's own tools such as `kdos` and `kdos-appbox` install, comes last in
the search path. The image steps call those tools by name, so the directory has to be on the path:
without it, `030_launchers.sh` finds no `kdos-appbox`, skips itself as not installed, and the image
ships launchers for packs it does not carry. Putting it first would let a binary there shadow a
system one while a port is being configured, which is a much harder failure to spot. `/usr/bin`
comes before `/bin`, which links to it, because CMake turns each entry into a search prefix and a
package configuration found under `/lib/cmake` computes its prefix as `/`.

## The chroot

The phases from `20_selfhost` on run inside the target root filesystem, `build/fs`.
`script/chroot/exec.sh` enters it once for every command the orchestrator runs there (every
`kpkgdepends`, every `kpkg install`, every image step):

1. It requires root, then runs itself again under `unshare --mount --propagation private`, so
   every entry has a private mount namespace of its own. It requires `build/fs` to exist.
2. It bind-mounts `/dev`, and mounts a fresh `proc` at `/proc`, `sysfs` at `/sys` and a `tmpfs` at
   each of `/tmp` and `/run`. It bind-mounts the container's cgroup tree read-only over the fresh
   sysfs's empty `/sys/fs/cgroup`: cargo and go size themselves from its `cpu.max`, and
   without it they see every host thread whatever `--cpus` cap the container has.
3. It bind-mounts the repository at `/kdos`, `build/` at `/kdos/build`, `ports/` at `/ports`, and
   `script/`, `src/` and `fs/` under `/kdos`, and `docs/` read-only at `/kdos/docs` for the
   `kdos-doc` and `kdos-kfs` ports, which render the two books from it. `testing/` is bound
   read-only at `/kdos/testing` and `ports/` again at `/kdos/ports` for `70_image`'s
   `087_gates.sh`, whose checks run from `/kdos` and read the recipes under it. The `Makefile` and
   the `Dockerfile` are bound read-only over their own paths under `/kdos`, for the listings
   `kdos-kfs` renders of them and for the sources `110_iso.sh` copies onto a developer medium. The
   repository bind mount does not carry the container's own mounts beneath it, so each directory
   and file a step needs is mounted explicitly.
4. It raises the open-files limit, soft and hard, to at least 4096. QtWebEngine's link runs
   `ulimit -n 4096` itself and fails when the hard limit is lower; a limit it cannot raise is
   logged to `build/logs/chroot.log`.
5. It enters with `env -i` and the variables above, and changes to `/kdos`, so a relative path such
   as `script/phases/30_foundation/phase.env` means the same inside as outside.

Nothing is unmounted, because nothing needs to be. Every mount the wrapper makes lives in its own
namespace: no other entry and not the container sees it, and it disappears when the entry's last
process exits, killed or not. Two entries running at once each have their own `/proc`, `/dev`,
`/sys`, `/tmp`, `/run` and repository binds, and because `/tmp` and `/run` are fresh for every
command, nothing a step leaves there survives into the next one. A mount the orchestrator finds
under `build/fs` before a snapshot was made by something other than the wrapper.

`script/chroot/enter.sh` is the interactive counterpart, for inspecting a tree by hand as root. It
takes a private mount namespace the same way, mounts only `/dev`, `/proc`, `/sys`, `/tmp` and
`/run` there, copies the host's `/etc/resolv.conf` into
the tree, and starts a login shell there (or runs the command it is given). It does not mount the
repository, and it is not used by the build.

## Syncing `fs/`

`fs/` holds the files KDOS adds to the root filesystem directly: configuration, scripts, skeleton
home directories. `10_bootstrap/000_file_system.sh` copies it into `build/fs` whenever `10_bootstrap` runs, after
laying out the directory skeleton, the merged-`/usr` links (`/bin`, `/sbin`, `/lib`, `/lib64`) and
`/var/run` and `/var/lock` as links into `/run`.

`fs/usr/lib64` is a link to `lib`, so there is one library directory. gcc is patched to put its
runtime in `/usr/lib` (`t-linux64-lib.patch`, `MULTILIB_OSDIRNAMES m64=../lib`), so nothing it
links records a `/usr/lib/../lib64` RPATH; meson picks `lib` when `/usr/lib64` is a link, and
CMake's `GNUInstallDirs` is patched to default to `lib`. The `gcc` recipe moves libstdc++'s
pretty-printer hook, `libstdc++.so.6.*-gdb.py`, to `/usr/share/gdb/auto-load/usr/lib`: gdb runs an
objfile's script only from its safe path (`$debugdir:$datadir/auto-load`), so beside the library it
is declined and every STL container prints as its internals. `cp` cannot replace a real directory
with a link, so a tree whose `usr/lib64` is still a directory keeps it: the step reports it and then
tolerates any failure of the whole copy, not only that path's, so a sync into such a tree can leave
other files of `fs/` uncopied without stopping the build. The link reaches the tree when it is built
from empty.

A plain recursive copy overwrites but never deletes, so a file removed from `fs/` would linger in an
incremental build tree. The step therefore records every path `fs/` provided in
`/var/lib/kdos/fs-manifest`, and on the next sync deletes any path in the old manifest that `fs/`
does not provide any more. Two details keep that safe:

- The manifest lists only files and symbolic links, and it is written after the copy, so a path a
  package later installs over is not the manifest's to remove.
- The manifest is built without `find -printf`, a GNU extension. The step runs on the build
  container's userland, not the tree's, and a `find` without the extension would write an empty
  manifest, which protects nothing.

A copy also keeps the destination's mode, so the step then replays the execute bit of every regular
file from `fs/`: 755 when it is executable in the repository, 644 otherwise. A few paths need modes
and owners that git cannot record, and the step sets them explicitly:

| Path | Mode | Owner |
|---|---|---|
| `/etc/shadow` | 600 | root |
| `/etc/polkit-1/rules.d` and `50-kdos.rules` in it | 755, 644 | root |
| `/etc/udev/rules.d/` and everything under it | 755 for directories, 644 for files | root |

`/etc/passwd`, `/etc/group` and `/etc/shadow` are merged, not overwritten: package install hooks add
service accounts long after this step runs, and a plain copy on a later sync would delete them.
Entries from `fs/` win; accounts that exist only in the tree are appended back.

Home directories are created later, by `70_image/070_user.sh`, for each ordinary user (UID 1000
to 65533) with a home directory in `/etc/passwd`. It clears the generated trees that `/etc/skel`
owns outright (`.icons`, `.themes`, `.config/kdos-comp`, `.config/cosmic`, `.config/kdos-con`,
`.local/share/applications` and `.local/share/color-schemes`) before copying `/etc/skel` in, so
something `/etc/skel` stops providing does not linger in a home. An edit made by hand to one of
those trees in a home under `build/fs` is lost on the next packaging run. The same step creates the
XDG user directories, the mail, calendar and contact stores, and sets `.msmtprc` and `.mbsyncrc` to
mode 600.

## Sweeping orphaned packages

A port deleted from `ports/` leaves its package installed unless something removes it. `fs/` has a
manifest; packages have `70_image/040_orphans.sh`, which removes every installed package that has
no recipe in any repository a phase's `PORT_REPO` names or in `src/libs`: `ports/core`,
`src/system`, `src/art`, `src/desktop`, `src/daemons` and `src/libs`, each searched the way `kpkg`
searches, directly and one shelf down. It also removes every installed package whose port
`ports/unbuilt` names: no phase builds those, so one installed is left from an earlier build and
would otherwise ship. `testing/preflight.sh` reports the same packages in a few
minutes, before a build, and fails when the step's list of repositories leaves out a `src/` area
that holds a recipe, since every package of that area would then be swept from the image.

The sweep refuses to run, and fails the build, when it finds no recipe under `/ports/core` at all,
or when more than half the installed packages would go. Either is a broken mount or a lookup that
does not match the tree, not a set of removed ports, and sweeping would ship an image with nothing
in it. A package that fails to uninstall is reported and left in place rather than stopping the
build, so one damaged package cannot prevent the ISO from being produced.

## The packaging steps

`70_image` runs these scripts in sorted order inside the chroot. The numbers are the sequencing:
the binhost is written before the cleanup empties the package cache, the sweeps and the theme run
before the homes are made from `/etc/skel`, `/etc/os-release` is stamped before the base pack is
made, and the indexes are built over the finished tree before
the initramfs and ISO carry it into the image.

| Step | Does |
|---|---|
| `010_binhost.sh` | With `KDOS_MAKE_BINHOST=1`, copies the packages this build made into `build/binhost/` and signs an index over them with `kpkg index --sign`, making the key in `build/binhost-key/` on first use; without it, does nothing. The directory accumulates across runs and nothing empties it, and the index gives every package file the `E:` recipe hash of the port as it stands now, so a file an earlier run left is indexed as if built from the current recipe. A complete binhost needs `build/binhost/` emptied by hand and then a build from nothing with the flag set: `--fresh` alone builds no port already installed in `build/fs`, so it makes no package for one (see [Building from scratch](developing.md#building-from-scratch)) |
| `015_pkgstore.sh` | With the package store on, runs `kpkg store gc` over `build/pkgstore` down to `KDOS_PKG_STORE_MAX`, least recently used entries first; with it off, does nothing |
| `020_cleanup.sh` | Removes build caches, `/tmp` and `/var/tmp` contents, the `kpkg` work directory, and the built-package cache with its `.pending` records, and any podman container store left in `/home/kdos/.local/share/containers` or `/var/lib/containers/storage` (applications are built on the machine that wants them). Resets `/root` to exactly what `fs/root` holds, mode 0700, so no CMake package registry, git setting or log a build wrote there ships. Removes Python bytecode no package owns and checks that every packaged module has its own (see below). Removes every file no package owns that the [size and language policy](writing-ports.md#size-and-language-policy) prunes from a package (a non-English message catalog, a translated manual page, an HTML or PDF manual under `/usr/share/doc`) and the gawk and binutils manuals those ports leave out, so a copy `10_bootstrap` installed and its port does not adopt never ships |
| `030_launchers.sh` | Reconciles the generated application launchers in `/etc/skel` with the packs the image carries, through `kdos-appbox genlaunchers` |
| `040_orphans.sh` | Removes installed packages that have no recipe in the tree, or whose recipe `ports/unbuilt` lists |
| `045_identity.sh` | Writes `BUILD_ID` (the commit, `-dirty` when the tree had unrecorded changes) and `KDOS_BUILD_DATE` (that commit's date) into `/etc/os-release`, replacing any earlier pair; with no `KDOS_GIT_COMMIT` it writes neither. The commit's date and not the clock's, so two builds of one commit stamp the same file. It runs before `100_packs.sh` so the base pack carries the stamp |
| `050_theme.sh` | Checks that `KDOS_ACCENT` is libkcolor's compiled default, then seeds that theme into `/etc/skel` with `kdos theme` and checks the files it writes; compiles the GLib schemas and the desktop-file database where their tools are installed |
| `060_udev_hwdb.sh` | Compiles `/etc/udev/hwdb.bin` |
| `070_user.sh` | Removes empty directories under `/etc/skel/.config`, then creates the home directory of each ordinary user (UID 1000–65533) from `/etc/skel`, first clearing the generated trees skel owns outright (see [Syncing `fs/`](#syncing-fs)). Removes `/etc/passwd-`, `group-`, `shadow-` and `gshadow-`, the backups shadow's tools write, which carry the live accounts' password hashes |
| `080_whatis.sh` | Builds the manual-page index for `apropos` and `whatis` |
| `085_fontcache.sh` | Clamps the font directories' times to `SOURCE_DATE_EPOCH` and writes the fontconfig cache, failing unless fontconfig accepts it (see below) |
| `087_gates.sh` | Runs `testing/unowned.py`, then `testing/linkcheck.py` and `testing/entrycheck.py` over the finished tree, and fails the phase when linkcheck or entrycheck reports or a file no package owns is left under `/usr/share/locale`, `/usr/share/man`, `/usr/share/doc` or `/usr/share/info`; any other unowned file is a report. See [Testing](testing.md#files-nothing-owns) and [Testing](testing.md#run-time-references) |
| `090_initramfs.sh` | Builds the initramfs (the small RAM filesystem the kernel boots into first) in `build/initramfs` and `build/initramfs.cpio.gz` |
| `100_packs.sh` | Creates the pack store directories, and with `KDOS_PACK_KDOS=1` the base pack `kdos` in `build/kdos-base` |
| `105_ports.sh` | Stages the recipe-only ports tree in `build/image-ports`: every port's recipe files without the sources it hashes or any archive, `ports/shelves`, `ports/sources.idx`, `src/`, `docs/kdos`, `docs/screenshots`, `docs/kfs`, `script/book` and a `TREE` stamp; puts `build/keys/kdos-release.pub` in `/etc/kdos/keys/tree`; writes the source offer `/usr/share/kdos/SOURCES` (see [How KDOS is built](how-kdos-is-built.md#packaging-70_image)) |
| `110_iso.sh` | Squashes the tree into `system.sfs`, a read-only squashfs image in 1 MiB blocks compressed with zstd at level 15, or with the codec `KDOS_ISO_COMP` names (`xz` or `zstd:<1-22>`; anything else is fatal), appends the staged ports tree as `/ports`, assembles `iso_root` with the kernel, the initramfs and Limine (the bootloader), and writes `build/iso-build/kdos.iso`. With `KDOS_PACK_KDOS=1` it also copies the base pack onto the ISO9660 filesystem at `/packs/kdos.kpack`. It draws the boot menu in the console's own font, converted by `psf2limine.py`, which sits beside it in `70_image/` |

The ISO carries no applications. `100_packs.sh` always creates `/var/lib/kdos/packs/staging` (mode
01777) and `/var/lib/kdos/packs/mnt`; the only pack a medium can carry is the opt-in base pack
`kdos`, beside `system.sfs` rather than inside it. Applications are built by podman on the machine
that wants them. See [Packs and boxes](../03-architecture/packs-and-boxes.md). The boot path the
initramfs and Limine set up is described in [Boot and init](../03-architecture/boot-and-init.md).

With `KDOS_ISO_SOURCES=1`, `110_iso.sh` copies the tree onto the ISO9660 filesystem under
`/sources`, beside `system.sfs` rather than inside it, so it costs the installed system nothing. It
copies `src/`, `script/` and `fs/` from `/kdos`, where the chroot binds each of them, and every
entry of `/ports` except its dot-directories: the source cache, whose bytes every port directory
already holds by hard link, and the compiled host helpers. A binhost this build wrote goes beside
the tree as `/sources/binhost`. The `Makefile` and the `Dockerfile` are copied from where the
chroot binds them under `/kdos`; `CLAUDE.md` is copied only when it exists there, and `make build`
does not mount it into the container, so an image from `make build` carries the first two and not
the third. A `SOURCES` stamp records the tree's size, the build
time and a port count, the number of recipes under `/ports/core`. The copy roughly doubles the
image, because the fetched upstream sources are already compressed and are not squashed again.

## Databases stamped into the image

Three databases that no single package installs are built from every package's files (they are
[shared indexes](writing-ports.md#shared-indexes)). `kpkg` keeps the hardware database and the
font cache current on every install and removal. It skips the manual index throughout the build,
because `chroot.env` sets `KPKG_SKIP_INDEX=man`: merging each port's pages would be thrown away
here. `060_udev_hwdb.sh` rebuilds the hardware database, `080_whatis.sh` writes the manual index
and `085_fontcache.sh` the font cache, each from scratch over the finished tree, and each checks
the result, so the image never carries an index left half-built by an interrupted build.

**The hardware database.** `udevadm hwdb --update` compiles `/etc/udev/hwdb.bin` from the `hwdb.d`
text files eudev ships. Many of eudev's rules import properties from it, and without the file those
imports return nothing and log nothing above debug level. The machine then silently behaves as
hardware with no quirks: laptop key mappings, the `EVDEV_ABS_*` touchpad sizes libinput relies on,
and `ID_VENDOR_FROM_DATABASE` / `ID_MODEL_FROM_DATABASE` are all missing. The file goes in
`/etc/udev`, which libudev reads first, rather than `/usr/lib/udev`: the initramfs copies
`/usr/lib/udev` whole, and about 10 MB of database would sit in RAM on every boot for a stage that
uses none of it.

**The manual index.** `makewhatis` writes a `mandoc.db` into each manual root that exists,
`/usr/share/man` and `/usr/local/share/man`. `man` works without one, but `apropos` and `whatis`
print nothing and exit 0 without it, which looks the same as "nothing matches".

**The font cache.** `fc-cache -sf` writes `/usr/lib/fontconfig/cache` after every directory under
`/usr/share/fonts` has had its time set to `SOURCE_DATE_EPOCH`. Under that variable fontconfig
stores the epoch as each directory's time, and a cache whose stored time differs from the
directory's is stale: fc-cache deletes it as an invalid cache file, and every user's first
fontconfig program scans every face, on the live medium at every boot. The step fails unless a
cache file exists and a second `fc-cache -sv` reports no directory to cache again and no invalid
file.

The three steps run after the orphan sweep, so a removed package's rules, pages and faces are not
indexed, and before the initramfs and ISO steps. Neither trusts an exit status: `udevadm hwdb --update` exits 0
having compiled nothing when it finds no sources, and `makewhatis` exits non-zero for one
unreadable page while still writing a complete database. The hardware step counts its source files
and checks that the file it wrote is not empty; the manual step fails unless at least one
`mandoc.db` was written.

**Python bytecode.** Bytecode ships in the packages: `kpkg` compiles every `.py` a package ships,
outside the bin directories, `etc` and `var`, into checked-hash bytecode in the package itself
(see [Packaging](../03-architecture/packaging.md#what-a-build-verifies)), so the database owns
every `.pyc` and an upgrade or removal replaces or removes it. `020_cleanup.sh` deletes every
`.pyc`, `.pyo` and empty `__pycache__` that no package owns, which is what the build's own tools
wrote by importing modules in the chroot, with a build-time timestamp. It then fails the image when
a `.py` a package owns under `/usr` or `/opt`, outside `/usr/bin` and `/usr/sbin`, has no `.pyc`
for the image's interpreter, and prints each such path. gcc's libstdc++ pretty printers
(`/usr/share/gcc-*/python`) and gdb's auto-load scripts are exempt: gcc is built in `20_selfhost`
and `30_foundation`, before `python3` is installed, so `kpkg` has no interpreter to compile them,
and gdb reads them only in a debugging session. A `*.tmpl.py` template passes, and a file the
interpreter cannot compile (Python 2 scripts in documentation, templates under another name) is
listed and passes. Ownership is read with the merged-`/usr` aliases folded. Hash-checked bytecode
carries no timestamp, and the interpreter checks the hash on import, so a source file a later
upgrade replaces is recompiled rather than served stale. A module with no bytecode would be
compiled on every import of every start, because `/usr` is read-only to a running program.

**Unowned documentation.** `kpkg` applies the
[size and language policy](writing-ports.md#size-and-language-policy) to each package it makes,
but `10_bootstrap` installs its tools with no package, and a port installed over them later adopts
only the paths it ships itself. A bootstrap copy of something the policy prunes from that port, or
of a file the port's `build.sh` deletes, is adopted by nothing and stays in the tree. The bootstrap
steps build with `--disable-nls` and delete those files after their install, and `020_cleanup.sh`
removes, where no package owns them and `fs/` does not provide them, every non-English message
catalog under `/usr/share/locale`, everything under a `/usr/share/man` directory that is not a
`man<section>` one, every `.html`, `.htm` and `.pdf` under `/usr/share/doc`, and gawk's
`gawknotes.info` and binutils' `dlltool`, `windres` and `windmc` pages. `087_gates.sh` then fails
the image on any file no package owns left under `/usr/share/locale`, `/usr/share/man`,
`/usr/share/doc` or `/usr/share/info`.

## Fetching sources in a container

`make build` never downloads anything; `make fetch` (which runs `ports/fetch`) does, beforehand.
Most of that work happens on your machine with `curl` and `sha256sum`, resolving each source from
the port directory, the source cache, the KDOS source archive or upstream (see
[Where sources come from](developing.md#where-sources-come-from)). The exception is a *vendor
bundle*: a tarball of a Rust, Go, Node.js, Python or Haskell port's dependencies, generated by that
language's own package manager. A port whose vendor bundle is still missing after the first pass is
handed to a container image of its own, `kdos-fetch`, built from `ports/Containerfile.fetch` with
docker or podman, which generates it. A clone whose archive is complete never builds that image.

The image carries Rust, Go, Node.js, GHC and cabal at the versions this tree's `rust`, `go`,
`nodejs`, `ghc` and `cabal-install` recipes name, passed in as build arguments. That pinning is
what makes a bundle usable: a package manager newer than the compiler that will build the port can
write a lock file the older one refuses.

The container runs as your user, because every file it writes lands in your tree and your source
cache. That user has no account inside the image, so its home directory and each toolchain's cache
are pointed at `build/fetch-home`; cargo and go refuse to run without a writable home. A cache
outside the repository (`KDOS_SRCCACHE`) is mounted into the container beside it.
`ports/fetch` also compiles a recipe reader on your machine; with no C compiler there, the whole
fetch runs in the container instead. `KDOS_FETCH_HOST=1` skips the container and generates bundles
with your machine's own toolchains.

## Snapshots

A full build takes many hours. A run given `--snapshot`, or started with snapshot writing turned on
in the [startup picker](#the-startup-picker), therefore archives every phase from `00_cross` to
`50_desktop` as it completes, to `build/snapshots/<phase directory>/`, so that a later build can
start from it instead of from the beginning. Writing is off otherwise: a run writes no snapshot
unless asked to, and restoring from the snapshots that exist works either way. `60_kernel` and `70_image` take no snapshot: they are re-run on the tree they
find, which costs minutes, where archiving the image phase's tree would cost more than running it.

Each phase declares what its snapshot holds. `KDOS_SNAPSHOT_PATHS` lists paths relative to
`build/`; `KDOS_SNAPSHOT_EXCLUDE` lists patterns that keep out work directories, the
pseudo-filesystems and the chroot's bind mounts. The first two phases archive `cross`, `fs` and
`mark`, since the cross toolchain and the markers are part of their result; the package phases
archive `fs`. A declared path that does not exist yet is left out.

**How exclude patterns match.** Each pattern is matched with `fnmatch(3)`, no flags, against an
entry's whole path relative to `build/`. `*` therefore crosses `/` and matches a leading dot, and a
pattern is anchored at `build/`: `fs/tmp/*` leaves out everything below `fs/tmp` but keeps the
directory itself, and does not touch `fs/var/tmp`. An excluded directory is left out whole.

What a snapshot of `fs` leaves out: the contents of `tmp`, `dev`, `proc`, `sys`, `run`, the
bind-mounted `kdos` and `ports`, and, from `20_selfhost` on, `kpkg`'s work directory and the build
caches `root/.cache`, `root/.cargo`, `root/.npm` and `var/tmp`. No recipe reads another port's
output out of those caches (each cargo build keeps its own `CARGO_HOME` in its source tree), so a
restored tree builds the same without them.

A phase that declares no paths is never snapshotted. That is how a phase opts out of its snapshot
for good: set `KDOS_SNAPSHOT_PATHS=""` in its `phase.env`, or remove the line, and nothing else has
to change. A restore past it takes `fs` from the newest snapshot below it, and a build resumed from
there re-runs the phase, which `kpkg` makes cheap for every port already installed. A snapshot
directory still under such a phase's name is a leftover: it is never restored or layered into a
restore, the picker does not offer it, and `--list` marks it `leftover (phase declares no paths)`
so that `--delete` can remove it.

### Layers and full snapshots

Each path of a snapshot is archived one of two ways:

- **Full**: every entry of the path, the whole tree.
- **Layer**: only what changed since another snapshot, its *base*, named by the base's id. Beside
  the archive, `<path>.gone` lists, NUL-separated, the paths a restore deletes before extracting
  it: the top of every removed subtree, and every path whose type changed.

A layer holds each entry that is new, changed type, or has a different inode or ctime from the
base. Any change to an entry, whether its content, mode, owner, an extended attribute, its link
count or a replacement by a new file, moves its ctime or its inode, so nothing that changed is
missed. It also holds the directory of every changed or removed entry, because extracting or
deleting inside a directory resets that directory's times and the layer puts them back, and always
the root of the path, so a layer is never an empty archive.

A snapshot path is a layer when all of these hold, and full otherwise:

- the `tar` in use accepts `--no-recursion`, `--null`, `--files-from` and `--verbatim-files-from`;
- `--full-snapshots` was not given;
- the path's index file (below) is readable, and the root it records is the same directory, by
  device and inode, as the path now;
- the snapshot the index names as its head exists, in a phase directory or held, and its whole
  chain resolves;
- the head's phase comes before this phase, or is this phase and the head is its own partial
  snapshot;
- the new chain would be at most 64 archives long.

In practice: the first snapshot of a path is full; each later phase's is a layer on the one
before; `--fresh` or `--continue-from K` on a tree already past `K` takes `K` full and layers the
phases after it on that; after a restore, the next phase's snapshot is a layer on what was
restored; and after a snapshot that failed, the next one is a layer on the last one that succeeded,
and still carries everything since. The phase snapshots of one synthetic ten-phase tree, about 4.4
GB at the end, took 1.86 GB as layers against 7.99 GB as full snapshots; a set from a real build
has not been measured.

**The index.** `build/.snap-lineage/<path, with / as _>.idx` describes the path as the snapshot
named `head` archived it, or as a restore of `head` left it. It is a header,

```
kdos-snap-index 1
head <snapshot id>
phase <index> <phase directory>
partial 0|1
root <st_dev> <st_ino>
```

then one record per entry, `<type, octal> <inode> <ctime in ns> <allocated bytes>\t<path>\0`, in
walk order: each directory's names sorted by byte, a directory before its contents, which is
byte order with `/` below every other byte. The next snapshot compares a fresh walk with it in a
single merge. It is rewritten, as a `.tmp` file and a rename, after every snapshot that commits,
and after every restore; a restore deletes it before deleting anything else. An index that is
missing, malformed or out of order reads as no index, and the path's next snapshot is full.
`make cleanbuild` removes `build/.snap-lineage/` along with the tree, so the first snapshot after
it is full.

**The walk is the archive.** A snapshot walks each path itself, with the phase's exclude patterns,
and hands `tar` the list with `--no-recursion`: the whole list for a full archive, the layer's list
for a layer. The archive and the index describe the same entries by construction. The walk does
not enter another filesystem, skips a live mount point, walks a directory it reaches twice only
once, and fails the snapshot for a tree deeper than 1024 levels rather than archive it with a hole.
With a `tar` that cannot take such a list, every snapshot is full and archived by `tar`'s own
recursion with `--exclude`.

### What a snapshot directory holds

A snapshot directory holds one archive per path, a `<path>.gone` for a layer that removes
anything, and a `manifest.json`:

| Codec | Archive | Used when |
|---|---|---|
| `zstd` | `<path>.tar.zst`, compressed with `zstd -1` on all cores; an archive written at another level restores the same | `zstd` is installed, as it is in the build image |
| `gzip` | `<path>.tar.gz`, compressed with `gzip -1` | No `zstd` |
| `none` | `<path>.tar` | Neither |

`tar` is run with `--numeric-owner`, `--one-file-system`, `--sparse`, `--xattrs` and `--acls`, each
only when the `tar` in use supports it. The manifest, schema 4, records the snapshot's `id`
(`<phase directory>-<unix time>-<8 hex digits>`), the phase, the commit and whether the checkout
was dirty (from `KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY`), the phase's duration and the snapshot's
own, how many of the phase's steps had finished, whether the snapshot is complete, the codec, and
under `paths`, for each path: its archive, `kind` (`full` or `layer`), `base` (the base's id, or
`null`), `removed` (the `.gone` file, or `null`) and `removed_count`, the archive's raw size
(allocated bytes of its files), compressed size and member count, and `tree_bytes` and
`tree_files`, the whole path as it stood. A manifest of schema 3 or older names its array
`entries`, is always full, and is given the id `legacy-<phase directory>-<created × 10>`; it is
restored as a full archive, and can be a base once it has been restored. A `kdosbuild` that knows
only `entries` finds no array in a schema-4 manifest and treats that snapshot as absent rather than
restore a layer as a whole tree.

**Writing.** A snapshot is written into `build/snapshots/.new-<phase directory>/`: its archives,
its `.gone` lists and its manifest. Then it commits:

1. The snapshot it replaces is moved aside: into `.held/` (below) when any other snapshot's chain
   runs through it, else to `.trash-<phase directory>`.
2. The staging directory is renamed to the phase directory.
3. The trash is deleted.
4. The index files are written with the new id as head.
5. Every held snapshot no phase snapshot's chain reaches any more is deleted.

A snapshot interrupted or failed before step 2 deletes its staging directory and leaves the
previous snapshot and the index files as they were. A `.new-*` or `.trash-*` left behind by a
killed build is deleted when the next snapshot starts. `tar` exiting 1, its status for warnings
such as a file changing while it was read, is not treated as a failure.

**Held snapshots.** A snapshot that another snapshot's chain still runs through is never deleted
outright. When its phase is taken again, or `--delete` or the picker's `D` removes it, it moves to
`build/snapshots/.held/<phase directory>@<id>/`, so every snapshot that layers on it still
restores exactly as before. It is deleted as soon as no phase snapshot's chain reaches it. A
rebuild from phase `K` on therefore holds the old chain from `K` up while the new one is written,
and frees each old snapshot as the one replacing it is taken; the peak is the old chain from `K`
plus the new one.

**Disk space.** A snapshot is refused unless the free space on `build/snapshots` holds its estimate
plus a fifth. The estimate is the allocated bytes of the regular files it lists, from the walk,
times the base's compressed-to-raw ratio for that path, or half when there is no base, since
`zstd -1` on a root filesystem lands near 2x. Each of the eleven phases that snapshots archives
`fs`; the first as the whole tree, the rest as layers, so a complete set is the tree compressed
once plus what each phase added, changed or rewrote. Check the free space before a full build with
snapshot writing on; see [Building from scratch](developing.md#building-from-scratch).
`make cleanbuild` empties `build/` but keeps `build/snapshots`, the compiler cache `build/ccache`
and the package store `build/pkgstore`; `make clean` removes all three. Both keep `build/keys`, so a
signing key kept there survives a clean. The one key the build itself uses is the kernel's
module-signing key, `build/keys/linux-module-signing.pem`, which the `linux` port writes there when
it is missing and signs every module with (see
[Module signing](../03-architecture/security-model.md#module-signing)).

**Safety rules.** Snapshot and restore delete and re-extract the declared paths as root, so a
declared path is either accepted as written or rejected, never adjusted. An absolute path, a path
starting with `~`, an empty one, a bare `.`, or any path with a `..` component is refused and
reported as a notice. A name that merely begins with dots is allowed, and a trailing `/` is
removed. Every name in a `.gone` list must be the path itself or lie under it, with no `..`, `.`
or empty component, or the restore is refused before it touches anything; and a name with a
symlink anywhere above it is refused at the moment of deletion, since deleting through the link
would delete outside `build/`. A snapshot is also refused while anything is still mounted under
`build/fs`. The chroot wrapper leaves no mounts there, so any the orchestrator finds belong to
something else; it releases them first, lazily if it has to, and reports any it cannot.

### The startup picker

```sh
make build                                       # opens the picker
make build BUILD_ARGS=--fresh                    # skip it; run every phase on the existing tree
make build BUILD_ARGS="--restore selfhost"       # restore, continue at the next phase
make build BUILD_ARGS="--continue-from foundation" # resume on the CURRENT tree, no restore
make snapshots                                   # list them
```

When the build has a terminal on both standard input and standard output, and none of `--fresh`,
`--restore`, `--continue-from`, `--plan` or a command-line plan has already decided, the build
opens a picker before running anything. It answers two questions:

- **What to restore.** Row 0 is *start fresh*. Below it is one row per phase that has a usable
  snapshot, showing when it was taken, the size of its own archives, its commit, its step count and
  the phase's duration. A commit marked `*` differs from the current one or was dirty; a step
  count such as `12/40!` marks a partial snapshot. The selection opens on the last phase, in build
  order, that has a snapshot. The line beneath the list names each path the selected row restores,
  with its size and kind, and what restoring it reads, as in
  `restores: fs 61M layer · restore reads 1.8G from 10 archives`; the next line names the phase the
  build continues from.
- **Whether to write snapshots during this build.** It opens off, or on when `--snapshot` was
  given; `S` toggles it, and the footer shows `writing: on|off`. While it is off, a line under the
  list says the run leaves no restore points.

| Key | Does |
|---|---|
| `↑` `↓` or `j` `k` | Select a row |
| `Enter`, or a second click on the selected row | Start: fresh, or from the selected snapshot |
| `S` | Toggle snapshot writing |
| `P` | Open the [plan picker](#the-plan-picker) instead |
| `D` | Delete the selected snapshot. One that later snapshots layer on is held until they go, and the status line says which |
| `Q`, `Esc` | Quit without building |

The picker opens even when no snapshot exists: that is the from-scratch run, where writing
snapshots costs gigabytes and a part of the time, and the choice matters most.

`--restore`, `--continue-from` and `--delete` accept a phase by its short name (`selfhost`), its
directory name (`20_selfhost`), its 1-based position, or `latest`, the last phase in build order
that declares snapshot paths and has a snapshot. `--continue-from 60_kernel` or `70_image` runs
those phases on the existing tree.

*Start fresh* and `--fresh` run every phase on the target tree already in `build/fs`; they do not
empty it. A `00_cross` or `10_bootstrap` script whose marker exists exits at once, and a package
phase runs no step for a port already installed from the same recipe. A build from nothing needs `build/fs`,
`build/mark` and `build/cross` emptied first; see [Building from
scratch](developing.md#building-from-scratch).

`--continue-from` marks every phase before the one named as skipped and runs the rest on the
current tree. The skipped phases are neither re-run nor re-snapshotted, so a later tree is never
filed under an earlier phase's name. The first phase it runs on a tree that was already past it is
snapshotted full, and the snapshots it replaces are held while anything layers on them.

### How a restore works

A restore replaces the declared paths under `build/` with the contents of the snapshots and marks
the phases they cover as done:

- **Newest wins, per path.** Each path comes from the newest snapshot at or below the target phase,
  so a phase that declares only part of the tree does not lose the rest. Restoring `30_foundation`
  takes `fs` from `30_foundation` and `cross` and `mark` from `10_bootstrap`.
- **Each path is its chain.** The snapshot chosen for a path is followed down its bases to a full
  archive. The path is deleted and the full archive extracted; then each layer in turn has the
  entries its `.gone` lists deleted and is extracted over the result. Restoring a middle phase
  stops at that phase's own layer.
- **Only phases that declare paths.** The plan is built from the phases whose `phase.env` declares
  snapshot paths; a leftover snapshot under any other phase is never layered in, and restoring such
  a phase is refused with `<phase> declares no snapshot paths; its snapshot is a leftover`.
- **Nothing may be missing.** A restore is refused when the target phase has no snapshot, or when a
  path that the target or any earlier phase declares has no snapshot at or below the target to come
  from (for example `cross`, once both the `00_cross` and `10_bootstrap` snapshots are deleted). A
  result missing a component would be a tree that never had it.
- **Damaged means absent.** A manifest that does not parse, has neither a `paths` nor an `entries`
  array, or names an archive or a `.gone` list that is not on disk is treated as no snapshot at
  all, never as a partial one. So is a snapshot whose chain does not resolve: a base that is gone,
  a loop of bases, or a chain longer than 64. Restoring its layers alone would produce a tree that
  never existed; `--list` shows it under `unusable` so that `--delete` can remove it.
- **Everything is checked first.** Every path, archive and `.gone` list is validated before
  anything is deleted, so a rejected restore leaves `build/` untouched.
- **Interrupted restores block.** A restore writes `build/.restore-in-progress`, naming its target,
  before it deletes anything, and removes it when the last archive is extracted and the paths are
  indexed. While the marker exists, snapshotting and the next build both refuse to run, and the
  picker shows the interrupted restore; restore a snapshot again or run `make cleanbuild`.

Extraction keeps numeric owners, every extended attribute (`--xattrs-include=*`, so file
capabilities in `security.capability` come back as well as `user.*` attributes) and ACLs, and,
when run as root, the recorded owners and the setuid and setgid bits. After the last archive,
each restored path is walked with the exclude patterns of the phase its top snapshot came from and
its index written with that snapshot as head, so the next snapshot is a layer on what was
restored.

**Partial snapshots.** Pressing `S` during a build queues a snapshot of the phase in progress, taken
once a step finishes with no other step running, even when snapshot writing is off. It is recorded as partial, with
the number of steps finished. Restoring a partial snapshot re-runs its phase rather than continuing
after it, which is safe because a port already installed and current runs no step. The phase's
snapshot when it completes is a layer on its own partial one, which is held while it does.

## Build plans

A snapshot answers "go back to this point". A build plan answers "re-run only this": it restores
nothing, and narrows what the next run executes on the tree already in place.

```sh
make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"
make build BUILD_ARGS="--phases 41_system,70_image --rebuild openssh,networkmanager"
make build BUILD_ARGS=--plan     # interactive
```

| Flag | Selects |
|---|---|
| `--phases LIST` | Only these phases |
| `--steps LIST` | Only these scripts, each as `PHASE:script.sh` |
| `--rebuild LIST` | Rebuild these ports even though they are installed |
| `--plan` | Open the interactive plan picker. Needs a terminal, and cannot be combined with `--json` |

A list is separated by commas or spaces. A phase is named by its short or directory name. A step
must name a script that exists in that phase, and naming a step adds its phase to the selection.
A `--rebuild` name may hold only letters, digits, `.`, `_`, `+` and `-`; anything else is refused
before the build starts, since the name is written into the plan file verbatim. Repeated names
count once.

`--phases` and `--steps` cannot be combined with `--restore` or `--continue-from`; `--rebuild` on
its own can. The plan in force is written to `build/.devplan.json`, replacing the previous file
atomically, and the plan picker opens on the plan stored there. A plan file that does not parse
completely reads as no plan.

Three rules make a plan safe:

1. **A plan that narrows execution writes no snapshots unless `--snapshot` is given.** Re-running an
   early phase on a later tree would otherwise file that tree under the earlier phase's name, so a
   narrowing plan turns off writing that the picker's `S` turned on, and says so in a notice;
   `--snapshot` on the command line is the one way to write them anyway. A rebuild list on its own
   does not narrow, so it keeps whatever writing was chosen.
2. **Only named ports are forced.** `kpkg install -f` is passed for the ports the plan names and no
   others; their dependencies keep the usual skip-if-installed behaviour. Forcing everything would
   rebuild the whole tree. A named port is forced in every selected phase whose resolved install
   order holds it, so `--phases 20_selfhost,30_foundation --rebuild zlib` builds `zlib` twice;
   select only the phase that lists the port to build it once.
3. **"Already done" guards stand down only for named steps.** `KDOS_REPLAY=1` is exported for the
   steps a plan names and forwarded into the chroot, and the marker guards described in
   [Guards in the early phases](#guards-in-the-early-phases) let those steps run again. Every other
   step sees `KDOS_REPLAY=0`.

A `--rebuild` name that no selected phase reached is reported at the end of the run
(`NOT rebuilt (not reached by the selected phases)`), rather than silently doing nothing.

### The plan picker

`--plan`, or `P` in the startup picker, opens a full-screen plan editor. It lists every phase with
a mark for whether it runs (`[x]`, `[ ]`, or `[~]` when only some of its scripts are selected) and,
for a script phase, how many of its steps are selected.

| Key | Does |
|---|---|
| `↑` `↓` or `j` `k` | Move |
| `Space` | Toggle the phase or script under the cursor |
| `→` `←` or `l` `h` | Show or hide a script phase's steps |
| `A`, `N` | Select all, or none |
| `/` | Open the port list to choose ports to force-rebuild |
| `Enter` | Run the plan; refused while no phase is selected |
| `Q`, `Esc` | Leave without running |

The port list filters as you type, `Space` toggles a port, `Backspace` deletes one character of the
filter, and `Enter` or `Esc` returns to the plan. It holds every port in `ports/core`, `src/system`
and `src/art`, found by the same walker `kpkg` resolves names with, plus every name in any phase's
list. That last source is what puts the `src/desktop` and `src/daemons` recipes on it, since each is
named in `50_desktop`'s list, and it keeps a listed name that has no recipe visible. A name is
listed once, and a list entry records its phase beside it. The list holds up to 4096 names; this
repository gives it about 2,100. A tree the walker refuses, because a name is filed twice or a recipe sits
below its shelf, leaves the list empty and says why on the status line.

## kdosbuild

`kdosbuild` is the orchestrator. Its source is `src/devtools/kdosbuild/`. It is a C program that
links the KDOS libraries `libkbase`, `libkbuild`, `libkpkg`, `libktui` and `libkcolor` and nothing
but the C library besides; `libkpkg` is there because the picker's port list is walked by `kpkg`'s
own code, and because the host-side question of which ports are installed and current, and the
dependency edges of the [level graph](#building-a-package-phase-by-level), are `kpkg`'s own
functions. `script/kdosbuild.sh` compiles it with `$CC` (default `cc`) to `build/.kdosbuild` (or
`$KDOSBUILD_BIN`), then runs it with `--script-dir script` and your arguments. It compiles only when
the binary is missing or `build/.kdosbuild.sum` (`$KDOSBUILD_BIN.sum`) does not match a SHA-256 over
the compiler's name, the flags, the compiler's target triplet and version line, and every `.c` and
`.h` file under `src/devtools/kdosbuild/` and the five libraries. The sum is written only after a
compile succeeds, so a failed compile is retried on the next run, and a binary another compiler
built (the host's glibc one or the image's musl one) is replaced rather than run. An unchanged
orchestrator starts in a few hundredths of a second instead of about ten.

The build runs as the container's root, so everything it writes is root's. When the run ends,
whether it succeeded or not, `script/kdosbuild.sh` hands every top-level entry of `build/` back to
`HOST_UID:HOST_GID` except `build/fs` and a `build/podman`, if one exists. A `build/podman` would
be a podman container store, and podman refuses a store whose owner is not the user running it; no
build step creates one. `build/fs` stays root's because it is the target root filesystem: `chown`
clears setuid bits and rewrites owners, and the next build's squashfs would ship a system with no
working setuid programs and `/etc/shadow` owned by the desktop user. See [Where the build puts things](developing.md#where-the-build-puts-things).

`make snapshots` runs `script/kdosbuild.sh --list` directly on your machine, so it needs a C
compiler there; `make build` runs the script inside the container.

### Flags

| Flag | Effect |
|---|---|
| `--fresh` | Skip the startup picker and run every phase on the existing tree. Steps and ports already done are skipped; see [The startup picker](#the-startup-picker) |
| `--restore PHASE` | Restore a snapshot and continue after it. `PHASE` is a short name, directory name, 1-based index, or `latest` |
| `--continue-from PHASE` | Resume at `PHASE` on the existing tree, with no restore. `PHASE` takes the same forms as `--restore` |
| `--full-snapshots` | Archive every snapshot path whole in this build, never as a layer; see [Layers and full snapshots](#layers-and-full-snapshots) |
| `--snapshot` | Write a snapshot of each phase as it completes, a narrowing plan's included. Without it no snapshot is written. With the full-screen interface, this is what the picker opens on |
| `--plan` | Open the plan picker and run the plan on this tree |
| `--phases LIST` | Run only these phases |
| `--steps LIST` | Run only these scripts, `PHASE:script.sh` |
| `--rebuild LIST` | Force-rebuild these ports |
| `--port-jobs N` | Build up to `N` ports of a package phase at once, by dependency level, `N` from 1 to 8. The default, 1, runs one port at a time; see [Building a package phase by level](#building-a-package-phase-by-level) |
| `--plain` | No full-screen interface: plain lines |
| `--json` | No full-screen interface: one JSON object per event. With `--list`, the snapshot inventory as one object |
| `--list` | List snapshots and exit |
| `--delete PHASE` | Delete one phase's snapshot and exit. `PHASE` takes the same forms as `--restore`. A snapshot that others layer on is held until the last of them goes, and the message names them: `deleted 41_system; kept as the base of 42_graphics…50_desktop` |
| `--build-dir DIR` | The build directory. Defaults to `$KDOS_BUILD_DIR`, then `build` |
| `--script-dir DIR` | The script directory, whose `phases/` holds the phases. Defaults to `script`. The repository root is taken as its parent, which is where the chroot wrapper and every path handed into the chroot are resolved from, so it is never `script/phases` |
| `--selftest` | Run the view-geometry, log-classifier, retry, jobserver and timings assertions and exit. Must be the first argument |
| `--preview SCREEN WxH TIER` | Draw one screen offscreen and print it. Must be the first argument; see [Diagnostics with no build](#diagnostics-with-no-build) |
| `-h`, `--help` | Print usage |

`--list` prints one row per usable snapshot (phase, time, the size of its own archives, commit with
`*` when it differs from the checkout or was dirty, step count, and kind: `full`, or `on
40_lang` for a layer) and, under it, each archive's compressed and raw size and member count, and
for a layer how many paths it removes and what restoring it reads (`restore reads 1.8G from 10
archives`). Below the table come an `unusable` section naming each snapshot whose chain does not
resolve and the base it misses, a `held` section with each held snapshot's id, size and the
snapshots that need it, and `on disk: X (held Y)`. With `--json`, the inventory is one object,
`{"commit": ..., "snapshots": [...], "held": [...], "bytes_on_disk": ..., "count": ...}`: each
snapshot carries its `id` and `usable`, and each of its `entries` its `kind`, `base`, `removed`,
`chain` (the phase directories its restore reads, base first, a held one as
`<phase>@<id>`) and `restore_bytes`; each held snapshot carries its `id`, `phase`, `needed_by` and
`entries`. An empty inventory is that object with empty arrays rather than a message.

**Exit status:** 0 when the build finished; 1 when a step failed, a restore failed during the run,
or an unfinished restore blocks the build; 2 for a usage error (an unknown argument or phase,
`--restore` with `--continue-from`, `--phases` or `--steps` with either of them, an invalid step or
rebuild name, `--plan` without a terminal or with `--json`), for a phase that cannot run (see
[Package lists](#package-lists)) or no phase at all, or for a `--restore` refused before the run
starts (the target has no snapshot, its snapshot's chain does not resolve, it declares no
snapshot paths, a declared path has no source, or `latest` with no snapshot at all).

`KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY`, which `make build` sets from your checkout because `.git`
is not mounted into the container, are recorded in each snapshot so the picker can show which
commit it came from. Run outside the container, `kdosbuild` asks `git` instead. The same two, with
`KDOS_GIT_DATE`, are what `70_image/045_identity.sh` stamps into the image's `/etc/os-release`;
that step does not ask `git`, so an image built without `make build` is unstamped.

### How the program is organised

| File | Owns |
|---|---|
| `main.c` | The command line, restore validation and the headless run loop |
| `manager.c` | The execution order, phase expansion, the level graph and the jobserver of `--port-jobs`, the step runner, the retry of a failed phase, which steps' times are measurements, and the `--selftest` assertions over the retry, run over a three-phase tree in a temporary directory, and over the jobserver |
| `timings.c` | `build/snapshots/timings.json`: the merge every recorded step goes through, and its `--selftest` assertions over fixture files |
| `snapshot.c` | Writing and extracting archives |
| `stats.c` | The sampler behind the build screen's graphs and figures: CPU busy time, load average, memory and swap in use, free space on `build/`, lines of output a second, and the size and file count of `build/fs`; and the size, time and count formatting the screens share |
| `tui.c` | The full-screen interface: the startup picker, the plan and port pickers, the build screen |
| `view.c` | The layout and log-classification decisions, the HUD's panel geometry, the `--selftest` assertions, and the preview fixture |
| `report.c` | The plain and structured output, from one traversal |

The build is a single loop with no threads: check each running step, take those samples, draw, wait
for a key with a deadline. The steps of a level run as separate processes, one slot each, and the
loop drains them all in turn; nothing in the program itself runs concurrently, so nothing has to
guard against drawing while something else changes state. The one expensive sample, the size and file count of
`build/fs`, is taken by a forked child that writes one line back through a pipe, so a slow walk
cannot stall the screen. A step's output is read once per turn of the loop from a pipe of 1 MiB
(where the kernel allows it; 64 KiB otherwise), one pipe per running step, and that capacity is what
a step may write per turn before it waits, so a step that prints fast is not held to the loop's pace.

The screen's writes time out after two seconds. A snapshot drains `tar`'s output in the same loop
that redraws the screen, so a terminal that stops reading (a paused pager, a stalled `ssh`) would
otherwise stall the snapshot; with the timeout it costs a dropped frame instead. The snapshot
screen is redrawn at most ten times a second, however often the snapshot reports progress, and
reads keys on every report, so `Q` cancels at once.

### The build screen

The build screen is a header, the step tree on the left, the selected step's log on the right, a
row of boxed measurement panels above the key row, and the keys.

**The header** names the current phase and, under it, a bar for the phase's steps, the steps done
of the phase's total, and three elapsed times: the running step's (`step`; under `--port-jobs`, how
many steps are running), the phase's (`phase`, the wall-clock span of its steps) and the run's
(`total`, from the start of the build until it stops; a retry starts it counting again). There is
no estimate of the time left. A narrow terminal gives up the step's time first, then the phase's,
then the run's, before the bar goes below ten columns. Each step's own duration is beside it in the
tree, and selecting a phase shows, below its details, a table of every phase's time: a finished
phase its duration, the running one its time so far marked `running`, a skipped one `skipped`, and
one that has not started `-`.

**The measurement panels** are boxes six rows tall from 26 rows of terminal, each titled in its top
border — `CPU`, `Memory`, `Output` and `Build` — with the graph four rows high inside, giving it
sixteen levels, and its captions to the graph's right. From 16 rows the panels become one unboxed
row; below that they are gone. Each graph is on a fixed scale whose ends are its box's top and bottom
borders, so a height means the same quantity on every frame. A graph has no gridlines, because a
dotted rule across it reads as data. It moves one sample a second, the newest at its right edge, two
samples to a character cell:

| Graph | Scale | Beside it |
|---|---|---|
| CPU | 0–100% of every CPU busy (user, nice, system, interrupt and steal time, from `/proc/stat`) | Busy now, the peak over the graph's width, the CPU count and the one-minute load average |
| Memory | 0 to all of `MemTotal`, in use being `MemTotal − MemAvailable` | In use, the total, swap in use and free space on `build/` |
| Output | Lines a second from the running steps, logarithmic from 1 to 100,000, about three levels to each power of ten | Lines a second now, the peak, `scale log` and the lines written so far |

A graph is drawn in the accent colour, in the warning colour above 85%, and memory in the error
colour above 95%; swap in use is in the warning colour while it is still growing. A rich terminal
draws the graphs in braille, eight dots a cell; the Linux console's font has no braille, so there a
cell is a full block, a medium shade or a light shade by how much of it is filled, and the ascii
tier uses `#`, `:` and `.`. The `Build` panel to the right, when there is room, holds the size and file count of
`build/fs` (taken every 20 seconds by a walk that gives up after 15, and marked `partial` when it
did), a strip with one cell per finished step coloured and sized by its duration in fixed bands
(under 10 seconds, a minute, 10 minutes and an hour, and longer) with a legend for the bands, and the
latest notice. As the terminal narrows the `Build` panel goes first, then the output graph, then
memory.

**The terminal outside the frame.** When a step fails, and when the run ends, completed or stopped,
the build rings the terminal bell and sends a desktop notification, once per event, in both forms
terminals take, `OSC 777;notify` and `OSC 9`. The terminal's title follows the build, at most once a
second: `kdosbuild 41_system 312/750 2h13m`, `kdosbuild FAILED rust 31_compilers`, or `kdosbuild done
19h42m`. The title the terminal had is saved on xterm's title stack at the start and put back at the
end; a terminal without that stack is left titled `kdosbuild`. The Linux console gets the bell and
nothing else.

### Keys

| Key | Does |
|---|---|
| `↑` `↓` or `j` `k` | Select a step |
| `Space` | Fold or unfold a phase |
| `F` | Toggle following the running step |
| `PgUp`, `PgDn` | Scroll the log ten lines |
| `Home`, `End` | Select the first or last step |
| `S` | Queue a partial snapshot of the current phase |
| `/` | Search the selected step's log. Hits are marked, not filtered, because a build log is read for the lines around a hit. `Enter` keeps the search, `Esc` clears it |
| `n`, `N` | Next or previous search hit |
| `E` | Jump to the first line classified as an error |
| `O` | Open the step's log in `$PAGER` (default `less`), started directly and never through a shell |
| `T` | Cycle through the colour schemes |
| `R` | After a step has failed and the run has stopped: [retry its phase](#retrying-a-failed-phase) |
| `Q` | Stop: every running step's process group gets `SIGTERM` at once, then `SIGKILL` after five seconds, and nothing after it runs. Press again to kill them at once and quit |

A click selects a step, and a second click on a selected phase's fold marker folds it.

When a step fails, a failure panel opens. While it is up it takes every key except `Q`:

| Key | Does |
|---|---|
| `R` | Retry the failed step's phase. Offered once nothing is running: a failed level first lets its running ports finish |
| `O` | Open the failing step's log in the pager |
| `C` | Copy the log's path to the clipboard. On the Linux console the copy is not possible, and the notice says so |
| `Esc`, `Enter` | Close the panel, leaving the failed step selected |

After a successful build a completion panel appears; `Esc` or `Enter` closes it and leaves the tree
and logs browsable.

### Retrying a failed phase

`R` runs the failed step's phase again without leaving the orchestrator, so a fix made while the
failure panel is up is picked up by the same run:

1. **`kpkg` first.** For a phase after `10_bootstrap`, `10_bootstrap/120_kpkg.sh` runs as an
   ordinary step at the head of the phase, writing the log of `10_bootstrap`'s own run of it. Its
   mark holds a hash of every source `kpkg` is compiled from, so it does nothing unless one changed,
   and then compiles `kpkg` into the tree again: a fix to `kpkg` or its libraries reaches the rest of
   the run. `00_cross` and `10_bootstrap` are retried without it, since the compiler `kpkg` is built
   with may not exist yet and `10_bootstrap` runs the script in its own place. A tree with no such
   script says so in a notice and goes straight on.
2. **The phase from its list as it reads now.** The phase's rows are dropped. A package phase is
   resolved again with `kpkgdepends`, so an edited recipe, phase list or `depends =` line is used;
   its ports installed and current are skipped as on any pass, and under `--port-jobs` the packages a
   drained level built are taken from the cache. A script phase reads its scripts again and runs
   from the one that failed: a script that finished keeps its result, and one added since runs.
3. **To the end of the run.** The phases before it keep what they did, the phases after it are still
   pending, and the build carries on through every phase it was started with.

`R` is offered in the failure panel and the key row once the run has stopped: under `--port-jobs`
the ports still running beside a failed one finish first. The headless reporters have no retry;
there, and after quitting, `--continue-from` with the failed phase does the same from the command
line, without the `kpkg` step.

### Headless and structured output

When standard output is not a terminal, or `TERM` is `dumb`, or `--plain` or `--json` is given,
the build prints plain lines instead of the full-screen interface. The test is made inside the
container, and `make build` gives the container a terminal whenever the shell it runs from has one
on standard input, so `make build > log` from an interactive shell still writes the full-screen
interface into `log`; `make build BUILD_ARGS=--plain > log` or `make build </dev/null > log` writes
plain lines. A build started with no terminal at all works too. With no terminal
on standard input there is no startup picker either, so a headless `make build` with no flags runs
every phase on the existing tree.

```sh
kdosbuild --json          # one object per event: build, phase, step, snapshot, notice, restore, result
kdosbuild --list --json   # the snapshot inventory
```

The plain and structured reporters are two implementations of one interface, driven by the same
traversal, so they cannot disagree about what ran. Each step is opened when it starts and closed when
it ends, once each; under `--port-jobs` the opens and closes of a level interleave, so a consumer
pairs a `running` event with its result by phase and step name, never by position. The plain
reporter prints a step's title again on its result line when another line came between. At the end
the plain reporter lists every phase's time (`skipped`, or `-` for one that never started) before
`BUILD COMPLETE in <time>` or `BUILD FAILED at <step> after <time>`, and the `result` event carries
the same as `phases`, one `{"phase", "status", "seconds"}` per phase in build order, `seconds` being
`null` for a phase skipped or never started. Errors go to standard error and stay out of the
structured stream.

Structured output is one JSON object per line rather than one document, because a build can be
killed at any moment and the point of a machine-readable log is being able to read the end of one
that died. The opening event carries no total step count: a package phase only knows its steps
once it starts. `testing/selftest.sh` drives a synthetic two-phase build through this output to
check the engine itself; see [Testing](testing.md).

### Diagnostics with no build

```sh
kdosbuild --selftest                        # the view-geometry, log-classifier, retry, jobserver and timings assertions
kdosbuild --preview <screen> <WxH> <tier>   # one screen, as text
```

`--preview` draws one screen offscreen and prints its cells as plain text:

| Argument | Values |
|---|---|
| Screen | `build`, `activity`, `failure`, `pinned`, `complete`, `startup`, `plan`, `packages` |
| Size | Columns and rows, for example `132x43` |
| Tier | The [glyph tier](../03-architecture/design-language.md#the-glyph-tiers): `rich` (braille and eighth-block glyphs), `vt` (the 512-glyph console font), `ascii` |

It is the way to check a layout without running a build or sitting at a terminal of that size. The
preview data is chosen to break layouts rather than to look plausible: a multi-terabyte total, a
nine-digit file count, hour-scale elapsed times, graphs crossing their warning levels, a notice too
long for one row, a port name longer than any pane. It writes nothing to disk. Only the spinner
varies between runs, because it is picked from the clock. The phases are fixed data too, not the
ones under `script/phases/`: the `startup` screen lists the thirteen phases, and the `build`
screen stands up three. Check the `vt` and `ascii` tiers in
particular for glyphs the console font lacks. Each screen's drawing is a single function that both
the live loop and the preview call, so the preview shows exactly what the build draws.

Run it from a binary your own machine's compiler built. `make snapshots` (or
`script/kdosbuild.sh --list`) compiles `build/.kdosbuild` with the host compiler:

```sh
make snapshots
build/.kdosbuild --preview failure 100x30 vt
```

The next `make build` finds the image's compiler in place of yours and replaces `build/.kdosbuild`
with one compiled in the Alpine build image, against musl. A glibc machine cannot run that binary,
and the shell's error (`No such file or directory`, or `required file not found`) does not mention
the missing musl loader. To keep a host copy beside the build's, compile it to another path:

```sh
KDOSBUILD_BIN=build/.kdosbuild-host script/kdosbuild.sh --list
```

## libkbuild

`libkbuild` is the half of the orchestrator that reads the build tree and decides; creating
archives and running phases stay in `kdosbuild`. Its source is `src/libs/libkbuild/`, and it is
described with the other libraries in [The C libraries](c-libraries.md).

| File | Owns |
|---|---|
| `kb_phase.c` | Phase discovery under `script/phases/`, the checks that refuse a phase that cannot run, the metadata block, and the snapshot path rules |
| `kb_plan.c` | A phase's list, from `packages.txt` or `packages.d/`, and its order run (`kbuild_packages_order_run`); a phase's `PORT_REPO` mapped onto the host by the rule `testing/phaseclosure.py` uses (`kbuild_phase_repos`); plan narrowing; the port list behind the picker, walked through `libkpkg`; and `build/.devplan.json` with its own strict reader |
| `kb_snap.c` | The snapshot inventory, held snapshots included; the chains that decide which archives restore each path, and which held snapshots nothing needs; the index file and the diff that decides what a layer holds; restore selection; the interrupted-restore marker; and mount detection |
| `kb_json.c` | A read-only JSON parser for snapshot manifests and the restore marker |

The JSON parser refuses anything that does not parse completely, including truncation, trailing
junk or a trailing comma, and the plan reader holds the plan file to the same standard. Every
caller treats a parse failure as "absent", so a lenient parser would turn a corrupt manifest into a
confident wrong answer, or a truncated plan into a narrowed build.

Two further rules, each preventing a silent wrong answer:

- **Files are read to their real end, never to their reported size.** Files under `/proc` report a
  size of zero, so a size-bounded read of `/proc/mounts` would find no mounts, and a snapshot would
  run over a live bind mount.
- **An empty structure means absent.** The interrupted-restore marker is tested for content: `{}`
  names no target and is not a marker. A caller that tested for the file alone would see an
  interrupted restore forever.

## See also

- [How KDOS is built](how-kdos-is-built.md) — the build told end to end, from clone to ISO
- [Developing](developing.md) — the commands, the first build and the iteration loops
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#self-hosting) — why the build bootstraps its own toolchain rather than using a host's
- [Packaging](../03-architecture/packaging.md) — what `kpkg` does inside a phase, and how ports are shelved
- [Writing ports](writing-ports.md) — the recipes the package phases install
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port by shelf, with the phase that installs it
- [Build troubleshooting](build-troubleshooting.md) — reading a failed step
- [Testing](testing.md) — what the orchestrator's own tests prove
- [The C libraries](c-libraries.md) — `libkbuild` among the rest

<!-- book-nav -->
---

*Part V — Building and developing, chapter 32.* Previous: [31. Developing](developing.md) · [Contents](../README.md) · Next: [33. Writing ports](writing-ports.md)
