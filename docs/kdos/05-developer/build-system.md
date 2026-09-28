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
   repository's `Dockerfile` (Alpine 3.23 with GCC, GNU make, bison, flex, tar, zstd and Python) and
   runs it with no network, eight CPUs and `--privileged`. It asks for a terminal only when it has
   one, so a build whose output is redirected to a file still starts.
2. **`script/kdosbuild.sh`**, inside that container, compiles the orchestrator from source and runs
   it.
3. **`kdosbuild`**, the orchestrator, discovers the phases under `script/` and runs them in order,
   writing logs, snapshots and timing history as it goes.
4. **The phases** do the work: the first two in the container itself, the rest inside the target
   root filesystem through `script/chroot_exec.sh`.

The container sees the repository at `/workspace`, with `src/`, `fs/`, `script/` and `ports/`
mounted read-only and only `build/` writable, so a build cannot modify its own sources. The
`Makefile` passes six variables in: `HOST_UID` and `HOST_GID` (see [kdosbuild](#kdosbuild)),
`KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY` (recorded in each snapshot), and the two opt-in packaging
flags `KDOS_ISO_SOURCES` and `KDOS_PACK_KDOS`. `make build` downloads nothing; every source has to
be in place beforehand, which is what `make fetch` does (see
[Fetching sources in a container](#fetching-sources-in-a-container)).

Three terms are used throughout:

- A **port** is a recipe for building one package: a `kpkgbuild` metadata file with a `build.sh`
  beside it. Upstream software has its recipes under `ports/core/`; KDOS's own programs have theirs
  under `src/packages/` and `src/desktop/`. See [Writing ports](writing-ports.md).
- A **phase** is one stage of the build: a directory under `script/` whose name starts with a
  number and an underscore.
- The **target tree** is `build/fs`, the root filesystem the build assembles and every phase from
  `02_phase2` on runs inside.

Other terms, such as chroot, cross toolchain, pack and glyph tier, are defined in the
[Glossary](../06-reference/glossary.md).

## Phases

The orchestrator lists `script/`, keeps the directories whose names match a number followed by an
underscore, and runs them in sorted name order. Each phase either holds numbered shell scripts or a
`packages.txt` naming the ports it installs. `util/`, which holds helper scripts such as `port.sh`
and `psf2limine.py`, and `hooks/`, which holds the git
[pre-push hook](writing-ports.md#the-pre-push-hook), do not match and are not phases.

| Directory | Title | Runs in | Contents | Snapshots |
|---|---|---|---|---|
| `00_toolchain` | Cross Toolchain | Container | 2 scripts: cross binutils and gcc for `x86_64-kdos-linux-musl` (musl is the C library KDOS is built on) | `cross fs mark` |
| `01_phase1` | Base Userland | Container | 16 scripts, `00_file_system.sh` to `13_kinstall.sh`: the `fs/` overlay, kernel headers, musl, libstdc++, ncurses, xz, gzip, tar, toybox, readline, bash, binutils, gcc, make, kpkg, kinstall | `cross fs mark` |
| `02_phase2` | Self-Hosting Bootstrap | Chroot | `packages.txt`, 8 ports: tar, musl, zlib, binutils, diffutils, m4, gawk and gcc, rebuilt inside the chroot | `fs` |
| `03_phase3` | Toolchain & Core Libraries | Chroot | `packages.txt`, 98 ports: compilers, build systems, interpreters, base libraries | `fs` |
| `04_phase4` | Userland & GUI Sliver | Chroot | `packages.txt`, 1,687 ports: system tools, services, firmware, the network stack, fonts, the Wayland base, Xwayland, the container layer, codecs, the application toolkits and the natively ported applications built on them, and KDOS's own theme, icons, splash and tools | `fs` |
| `05_desktop` | Desktop | Chroot | `packages.txt`, 22 ports: wlroots, `kdos-comp`, `kdos-shell`, `kdos-term`, `kdos-lock`, `kdos-res`, the daemons, the pack tools, fcitx5 and its engines, the portals, `kdos-record` | `fs` |
| `05_phase5` | Kernel | Chroot | `packages.txt`, 1 port: `linux` | `fs` |
| `06_packaging` | Packaging | Chroot | 10 scripts: see [The packaging steps](#the-packaging-steps) | `fs iso_root iso-build initramfs initramfs.cpio.gz` |

"Container" means the build container itself, running as root; "Chroot" means inside `build/fs`,
described in [The chroot](#the-chroot). A phase runs in the chroot when its environment file sets
`CHROOT=1`. Port counts are the non-comment lines of each `packages.txt`.

The first two phases build a cross toolchain and use it to cross-compile a minimal userland into
`build/fs`, including `kpkg`, the package manager, and `kinstall`, the installer. Both are compiled
directly from `src/packages/kdos-kpkg` and `src/packages/kdos-installer` rather than installed as
ports, because nothing can be installed as a port until `kpkg` exists. `kdos-kpkg` has no recipe
at all, and `kdos-installer`'s recipe is named in no `packages.txt`. The rest of `src/packages` is
built in `04_phase4`. `05_desktop` names `kdos-pack` again beside the pack daemon, but phase 4 has
already built it, with `erofs-utils`, as a dependency of `kdos-tools`, so the desktop phase skips
it. From phase 2 on, the build is self-hosting: phases 2 to 5 are lists of ports that `kpkg` builds
inside `build/fs` with the compilers phase 2 rebuilt there, and packaging runs its scripts in the
same chroot.

`05_desktop` sorts before `05_phase5` by name (`d` before `p`), so the desktop is built before the
kernel. The desktop is a phase of its own for two reasons. It is the only phase whose package
search path includes `src/desktop`, which holds the compositor, the shell and the daemons that no
earlier phase could build. And it gives the 1,687-port phase 4 a snapshot of its own below the
desktop, so work on a desktop program restores phase 4 and re-runs only what the desktop adds: the
ports its list names and the dependencies no earlier phase installs, such as `libinput`, `libwacom`
and `mtdev`, which only `wlroots` pulls in. The kernel is the last port built before packaging.

### Port lists, groups and the dependency closure

A `packages.txt` names only the ports a phase wants; each port's `depends =` line pulls in the rest,
and `kpkg` installs dependencies first. The five lists hold 1,815 names between them, 1,775 of them
distinct: 38 ports are named in more than one list. All 8 phase-2 ports appear again in `03_phase3`,
2 of those also in `04_phase4`, 29 further phase-3 ports appear again in `04_phase4`, and
`xcb-util-wm` is named in both `04_phase4` and `05_desktop`. A port already installed from the same
recipe is skipped when a later list names it again. A reading of every recipe's `depends =` lines
reaches 1,996 of the 2,001 ports under `ports/core` from the lists. [The ports
catalogue](../06-reference/ports-catalogue.md) lists every port by phase and list group, and those
named more than once. The comment headings inside a list, such as "Core Services" or "Modern CLI
tools" in phase 4, divide it into list groups for the reader; the orchestrator ignores them. List
groups, and the recipe's own `group =` key, are explained in
[Packaging](../03-architecture/packaging.md#phases-package-lists-and-groups).

Where `kpkg` looks for a recipe is set per phase by `PORT_REPO` in the phase environment:

| Phase | Port repositories |
|---|---|
| `02_phase2`, `03_phase3` | `/ports/core` (the `kpkg` default) |
| `04_phase4`, `05_phase5` | `/ports/core /kdos/src/packages` |
| `05_desktop` | `/ports/core /kdos/src/packages /kdos/src/desktop` |

A name found in more than one repository is taken from the first.

## How a phase runs

When the orchestrator reaches a phase, it expands the phase into steps and runs them one at a time.
How the steps are made depends on what the phase holds.

**A script phase** has one step per `*.sh` file in its directory, in sorted order (a name starting
with a dot is ignored). The step runs `bash <script>` in the container, or
`script/chroot_exec.sh bash <script>` for a chroot phase. The script sources its own phase
environment with a `source script/<phase>.env.sh` line near its top; the one exception is
`06_packaging/01_packs.sh`, which needs nothing from it.

**A package phase** is resolved first. The orchestrator runs `kpkgdepends`, the `kpkg` tool that
prints an install order, through the same wrapper:

```sh
source script/phase4.env.sh && export PKGDB_DIR=/dev/null && kpkgdepends <every name in packages.txt>
```

Pointing `PKGDB_DIR` at `/dev/null` makes the package database look empty, so `kpkgdepends` prints
the full install order for the list and its whole dependency closure, not only what is missing.
Each name in that order becomes a step of its own:

```sh
source script/phase4.env.sh && export KPKG_OVERWRITE=1 && kpkg install <port>
```

`kpkg` skips a port that is already installed from the same recipe, so a re-run of a phase installs
only what is new or changed. The phase environments set `KPKG_STRICT_RECIPE=1`, which makes "the
same recipe" mean the same recipe hash rather than merely an entry in the database: a port whose
`kpkgbuild`, `build.sh`, `postinstall.sh` or patches changed is rebuilt without being named. See
[Deciding what to rebuild](../03-architecture/packaging.md#deciding-what-to-rebuild).

`KPKG_OVERWRITE=1` lets a package take over a path another package already owns. The userland
overlaps on purpose (toybox and GNU sed, findutils, gawk and coreutils ship some of the same
commands), and the rule is that whoever comes last in the dependency order owns the path. It
rebuilds nothing. It is passed in the environment rather than as `--overwrite` because the `kpkg`
in a tree restored from an early snapshot may predate that option and would take it for a port
name; an unknown environment variable is ignored by every version.

The orchestrator reads what `kpkgdepends` prints, so that output has to be clean. `kpkgdepends`
writes the install order to standard output and nothing else, the orchestrator reads its standard
error separately, and `script/chroot_exec.sh` sends its own diagnostics to `build/logs/chroot.log`.
Every token read back must look like a package name (it starts with a letter or digit and holds
only letters, digits, `.`, `_`, `+` and `-`), so stray output fails the phase instead of being
installed as a package. Resolution also fails when `kpkgdepends` exits non-zero, prints nothing, or
resolves more than 4096 packages, the most one package phase may hold and the most `kpkg`'s
resolver returns; a phase that silently built only the first 4096 would report success. A resolution
failure is written to `build/logs/<phase>/expansion.log`.

A phase whose `packages.txt` names no ports, or whose plan selects none of its scripts, finishes at
once.

### Running a step

Each step runs in a child process with standard input from `/dev/null` and standard output and
standard error merged into one pipe. The child is placed in its own session, so that stopping a
step reaches the whole process tree under it (`make` and every compiler it started), not only the
`bash` at its top. A stop sends `SIGTERM` to the group, then `SIGKILL` after five seconds, and
after ten seconds the orchestrator stops waiting for the step, records it as failed with status
143, and ends the run.

A step that exits non-zero stops the build: the step and its phase are marked failed and nothing
after it runs. A step that cannot be started at all, because the pipe or the fork is refused on a
machine out of file descriptors or memory, fails the same way with return code 999 and a named
notice.

The orchestrator exports `KDOS_REPLAY` to every step: `1` for a step a build plan named explicitly,
`0` otherwise (see [Build plans](#build-plans)).

### Guards in the early phases

Every script in `00_toolchain` and `01_phase1` except `00_file_system.sh` starts with a guard: if
its marker file under `build/mark/toolchain/` or `build/mark/phase1/` exists, it exits 0 at once.
The expensive early work is therefore done once per tree, and a re-run of these phases costs
seconds. `12_kpkg.sh` stores a hash of every source file `kpkg` is compiled from in its marker
instead of an empty file, so a change to `kpkg` or its libraries rebuilds it on the next run. Every
guard stands down when `KDOS_REPLAY=1`. `00_file_system.sh` has no guard and syncs `fs/` on every
run.

### Step logs

Every step writes its complete output to its own file:

```text
build/logs/<phase directory>/<NNNN>_<name>.log
```

`NNNN` is the step's position within its phase, counted from zero. For a script step, `<name>` is
the script's file name with its numeric prefix removed; for a package step it is the port name
followed by `.install`. For example:

```text
build/logs/01_phase1/0000_file_system.sh.log
build/logs/04_phase4/0123_mesa.install.log
```

The file is verbatim, escape sequences included. The full-screen interface keeps only the last
2000 lines of each step in memory; the file keeps everything. Three further files sit under
`build/logs/`:

| File | Holds |
|---|---|
| `snapshots.log` | Every notice the build shows (snapshots taken, skipped or failed, restores, plans, stop requests), with a time stamp, so a notice survives the interface that displayed it |
| `chroot.log` | Mount warnings and errors from `script/chroot_exec.sh` |
| `<phase>/expansion.log` | Why a package phase could not be resolved |

Reading a failed step's log is covered in [Build troubleshooting](build-troubleshooting.md).

## The phase environment

Each phase has an environment file named after the part of its directory name after the number:
`03_phase3` reads `script/phase3.env.sh`, `05_desktop` reads `script/desktop.env.sh`,
`06_packaging` reads `script/packaging.env.sh`. The file carries the phase's compiler settings and,
at the top, a metadata block for the orchestrator:

```bash
# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export CHROOT=1
```

The orchestrator **parses** these files and never sources them. Several of them go on to remove a
work directory (`rm -rf /var/cache/kpkg/work` in the chroot phases, `rm -rf $BUILD_DIR/tmp` in the
first two), and sourcing them in the orchestrator's own process would run that removal against the
build container instead of the target. The parser therefore:

- reads only lines of the form `export NAME=VALUE`;
- honours only five keys: `CHROOT`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC`, `KDOS_SNAPSHOT_PATHS` and
  `KDOS_SNAPSHOT_EXCLUDE`;
- takes the value as a literal with no expansion. A quoted value ends at its closing quote, and an
  unterminated quote reads as empty. An unquoted value ends at whitespace or a `#`.

`CHROOT=1` selects the chroot wrapper; any other value, or no line, runs the phase in the
container. A phase with no environment file, or no title in it, is titled after its short name with
underscores and hyphens turned into spaces. The snapshot keys are described in
[Snapshots](#snapshots).

The rest of each environment file is ordinary shell that the phase's steps do source. The files for
`03_phase3` onwards name the compiler outright (`CC=gcc`, `CXX=g++`) rather than letting configure
scripts pick one, since some of them prefer clang once it is installed. Every file sets
`MAKEFLAGS=-j12`; every file except `toolchain.env.sh` sets `KPKG_STRICT_RECIPE=1`; phases 4, 5 and
the desktop set `PORT_REPO` (see the table above); and every file carries the settings that make
packages reproducible (`SOURCE_DATE_EPOCH`, `TZ`, `LC_ALL`, `-ffile-prefix-map`,
`--build-id=sha1`), described in
[Reproducible packages](../03-architecture/packaging.md#reproducible-packages). `packaging.env.sh`
also sets `KDOS_ACCENT`, the colour scheme the image ships in.

### `env -i` means every variable must be named

The chroot is entered with an empty environment (`env -i`). Only these variables reach a step
inside it:

| Variable | Value | Read by |
|---|---|---|
| `HOME` | `/root` | Everything |
| `TERM` | The caller's | Everything |
| `PATH` | `/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin` | Everything |
| `KDOS_REPLAY` | `0` or `1` | A step whose "already done" guard must stand down because a plan named it (see [Build plans](#build-plans)) |
| `KDOS_ISO_SOURCES` | `0` or `1` | `06_packaging/02_iso.sh`, to copy the sources onto the ISO (see [The packaging steps](#the-packaging-steps)) |
| `KDOS_PACK_KDOS` | `0` or `1` | `06_packaging/01_packs.sh`, to pack this root filesystem as the base pack `kdos`, and `02_iso.sh`, to put that pack on the ISO |

A variable the `Makefile` passes into the build container but `script/chroot_exec.sh` does not
name reaches the container phases and none of the chroot ones. Packaging is a chroot phase, so an
opt-in packaging flag that is not forwarded silently produces an ordinary image. Adding such a flag
is therefore two edits: the `Makefile` passes it into the container with `-e`, and
`script/chroot_exec.sh` names it on the `env -i` line.

`/usr/local/bin`, where KDOS's own tools such as `kdos` and `kdos-appbox` install, comes last in
the search path. Packaging calls those tools by name, so the directory has to be on the path:
without it, `00_launchers.sh` finds no `kdos-appbox`, skips itself as not installed, and the image
ships launchers for packs it does not carry. Putting it first would let a binary there shadow a
system one while a port is being configured, which is a much harder failure to spot.

## The chroot

The phases from `02_phase2` on run inside the target root filesystem, `build/fs`.
`script/chroot_exec.sh` enters it once for every command the orchestrator runs there (every
`kpkgdepends`, every `kpkg install`, every packaging script):

1. It requires root, and requires `build/fs` to exist.
2. It unmounts anything a previous, killed run left mounted under `build/fs`, deepest first, retrying
   a busy mount up to three times and finally detaching it lazily.
3. It bind-mounts `/dev`, and mounts a fresh `proc` at `/proc`, `sysfs` at `/sys` and a `tmpfs` at
   each of `/tmp` and `/run`.
4. It bind-mounts the repository at `/kdos`, `build/` at `/kdos/build`, `ports/` at `/ports`, and
   `script/` and `src/` under `/kdos`. The repository bind mount does not carry the container's
   own mounts beneath it, so each directory a step needs is mounted explicitly; `fs/` is not among
   them.
5. It enters with `env -i` and the variables above, and changes to `/kdos`, so a relative path such
   as `script/phase3.env.sh` means the same inside as outside.
6. It unmounts everything again when the command exits.

Because `/tmp` and `/run` are fresh for every command, nothing a step leaves there survives into the
next one.

`script/chroot_enter.sh` is the interactive counterpart, for inspecting a tree by hand as root. It
mounts only `/dev`, `/proc`, `/sys`, `/tmp` and `/run`, copies the host's `/etc/resolv.conf` into
the tree, and starts a login shell there (or runs the command it is given). It does not mount the
repository, and it is not used by the build.

## Syncing `fs/`

`fs/` holds the files KDOS adds to the root filesystem directly: configuration, scripts, skeleton
home directories. `01_phase1/00_file_system.sh` copies it into `build/fs` on every run, after
laying out the directory skeleton, the merged-`/usr` links (`/bin`, `/sbin`, `/lib`, `/lib64`) and
`/var/run` and `/var/lock` as links into `/run`.

A plain recursive copy overwrites but never deletes, so a file removed from `fs/` would linger in an
incremental build tree. The step therefore records every path `fs/` provided in
`/var/lib/kdos/fs-manifest`, and on the next sync deletes any path in the old manifest that `fs/`
does not provide any more. Two details keep that safe:

- The manifest lists only files and symbolic links, and it is written after the copy, so a path a
  package later installs over is not the manifest's to remove.
- The manifest is built without `find -printf`, a GNU extension. The build image's `find` is
  BusyBox's, which does not support it, and an empty manifest would protect nothing.

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

Home directories are created later, by `06_packaging/00_user.sh`, for each ordinary user (UID 1000
to 65533) with a home directory in `/etc/passwd`. It clears the generated trees that `/etc/skel`
owns outright (`.icons`, `.themes`, `.config/kdos-comp`, `.config/cosmic`, `.config/kdos-con`,
`.local/share/applications` and `.local/share/color-schemes`) before copying `/etc/skel` in, so
something `/etc/skel` stops providing does not linger in a home. An edit made by hand to one of
those trees in a home under `build/fs` is lost on the next packaging run. The same step creates the
XDG user directories, the mail, calendar and contact stores, and sets `.msmtprc` and `.mbsyncrc` to
mode 600.

## Sweeping orphaned packages

A port deleted from `ports/` leaves its package installed unless something removes it. `fs/` has a
manifest; packages have `06_packaging/00_orphans.sh`, which removes every installed package that
has no recipe in `ports/core`, `src/packages`, `src/desktop` or `src/libs`. `testing/preflight.sh`
reports the same orphans in a few minutes, before a build. A package that fails to uninstall is reported
and left in place rather than stopping the build, so one damaged package cannot prevent the ISO
from being produced.

## The packaging steps

`06_packaging` runs these scripts in sorted order inside the chroot. Sorted order is also the
sequencing: the sweeps and the theme run before the homes are made from `/etc/skel`, and the indexes
are built over the finished tree before the initramfs and ISO carry it into the image.

| Step | Does |
|---|---|
| `00_cleanup.sh` | Removes build caches, `/tmp` and `/var/tmp` contents, the `kpkg` work directory and built-package cache, and any podman container store left in `/home/kdos/.local/share/containers` (applications are built on the machine that wants them). Replaces Python bytecode (see below) |
| `00_launchers.sh` | Reconciles the generated application launchers in `/etc/skel` with the packs the image carries, through `kdos-appbox genlaunchers` |
| `00_orphans.sh` | Removes installed packages that have no recipe in the tree |
| `00_theme.sh` | Checks that `KDOS_ACCENT` is libkcolor's compiled default, then seeds that theme into `/etc/skel` with `kdos theme` |
| `00_udev_hwdb.sh` | Compiles `/etc/udev/hwdb.bin` |
| `00_user.sh` | Creates the home directory of each ordinary user (UID 1000–65533) from `/etc/skel`, first clearing the generated trees skel owns outright (see [Syncing `fs/`](#syncing-fs)) |
| `00_whatis.sh` | Builds the manual-page index for `apropos` and `whatis` |
| `01_initramfs.sh` | Builds the initramfs (the small RAM filesystem the kernel boots into first) in `build/initramfs` and `build/initramfs.cpio.gz` |
| `01_packs.sh` | Creates the pack store directories, and with `KDOS_PACK_KDOS=1` the base pack `kdos` in `build/kdos-base` |
| `02_iso.sh` | Squashes the tree into `system.sfs`, a compressed read-only squashfs image, assembles `iso_root` with the kernel, the initramfs and Limine (the bootloader), and writes `build/iso-build/kdos.iso`. With `KDOS_PACK_KDOS=1` it also copies the base pack onto the ISO9660 filesystem at `/packs/kdos.kpack` |

The ISO carries no applications. `01_packs.sh` always creates `/var/lib/kdos/packs/staging` (mode
01777) and `/var/lib/kdos/packs/mnt`; the only pack a medium can carry is the opt-in base pack
`kdos`, beside `system.sfs` rather than inside it. Applications are built by podman on the machine
that wants them. See [Packs and boxes](../03-architecture/packs-and-boxes.md). The boot path the
initramfs and Limine set up is described in [Boot and init](../03-architecture/boot-and-init.md).

With `KDOS_ISO_SOURCES=1`, `02_iso.sh` copies `src/` and `script/` from the chroot's `/kdos` onto
the ISO9660 filesystem under `/sources`, beside `system.sfs` rather than inside it, with a `SOURCES`
stamp whose port count is taken from `/ports/core`. It also copies `/kdos/ports`, leaving out the
port caches (`.srccache`, `.kpkg-meta`, `.portup-tools` and the update cache), but the chroot
mounts `ports/` only at `/ports`. The repository bind mount at `/kdos` does not carry the
container's own mounts beneath it, so under `make build` `/kdos/ports` is an empty directory and the
copy of it is empty too. The `Makefile`, the `Dockerfile` and `CLAUDE.md`, which the step copies
when they exist, are not copied either: `make build` mounts only `build/`, `src/`, `fs/`, `script/`
and `ports/` into the container, so `/kdos` has no such files. `fs/` is not in the step's list at
all.

## Databases stamped into the image

Two tools read a compiled database that no single package installs, because each is built from
every package's files. `kpkg` keeps both current on every install and removal (they are
[shared indexes](writing-ports.md#shared-indexes)). `00_udev_hwdb.sh` and `00_whatis.sh` rebuild
them from scratch over the finished tree and check the result, so the image never carries an index
left half-built by an interrupted build.

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

Both steps run after the orphan sweep, so a removed package's rules and pages are not indexed, and
before the initramfs and ISO steps. Neither trusts an exit status: `udevadm hwdb --update` exits 0
having compiled nothing when it finds no sources, and `makewhatis` exits non-zero for one
unreadable page while still writing a complete database. The hardware step counts its source files
and checks that the file it wrote is not empty; the manual step fails unless at least one
`mandoc.db` was written.

**Python bytecode.** `00_cleanup.sh` deletes every `__pycache__`, `.pyc` and `.pyo` the build
left, then compiles `/usr/lib/python3*` again with `--invalidation-mode checked-hash`, and fails if
no bytecode was written. Hash-checked bytecode is the same bytes from the same tree, so the image
is reproducible, and the interpreter checks the hash on import, so a source file a later upgrade
replaces is recompiled rather than served stale. Deleting without recompiling would make every
Python program compile everything it imports on every start, because `/usr` is read-only to it.

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

A full build takes many hours. Every completed phase is therefore archived to
`build/snapshots/<phase>/`, so that a later build can start from it instead of from the beginning.

Each phase declares what its snapshot holds. `KDOS_SNAPSHOT_PATHS` lists paths relative to
`build/`; `KDOS_SNAPSHOT_EXCLUDE` lists `tar` exclusion patterns that keep out work directories,
the pseudo-filesystems and the chroot's bind mounts. The first two phases archive `cross`, `fs` and
`mark`, since the cross toolchain and the markers are part of their result; the package phases
archive `fs`; packaging adds the ISO tree, the ISO and the initramfs. A phase that declares no paths
is never snapshotted, and a declared path that does not exist yet is left out.

A snapshot directory holds one archive per path and a `manifest.json`:

| Codec | Archive | Used when |
|---|---|---|
| `zstd` | `<path>.tar.zst`, compressed with `zstd -3` on all cores | `zstd` is installed, as it is in the build image |
| `gzip` | `<path>.tar.gz`, compressed with `gzip -1` | No `zstd` |
| `none` | `<path>.tar` | Neither |

`tar` is run with `--numeric-owner`, `--one-file-system`, `--sparse`, `--xattrs` and `--acls`, each
only when the `tar` in use supports it. The manifest records the phase, the commit and whether the
checkout was dirty (from `KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY`), the phase's duration and the
snapshot's own, how many of the phase's steps had finished, whether the snapshot is complete, the
codec, and each archive's raw size, compressed size and file count.
`build/snapshots/timings.json`, beside the snapshots, keeps the step and phase timing history that
the build screen's time estimate uses.

**Writing.** Each new archive is written to a `.tmp` file beside the old one. Only when every archive
of the snapshot is complete are they renamed into place and the manifest rewritten, so a snapshot
interrupted part-way leaves the previous one intact and deletes its own partial files. Archives
for paths the phase does not declare are removed afterwards. `tar` exiting 1, its
status for warnings such as a file changing while it was read, is not treated as a failure.

**Disk space.** Because the new archives sit beside the old ones until they are complete, a
snapshot is refused unless the free space on `build/` holds the previous snapshot's compressed size
plus a fifth. For a phase's first snapshot, a third of the raw size stands in for the compressed
size; measuring the raw size is bounded to two minutes. A complete set has measured at roughly
84 GB on one full build, of which the packaging snapshot alone is about 59 GB, because it carries
`iso_root` and `iso-build` beside `fs`.
`make cleanbuild` empties `build/` but keeps `build/snapshots`; `make clean` removes the snapshots
too. Both keep `build/keys`, so a signing key kept there survives a clean; the build itself
neither writes nor reads it.

**Safety rules.** Snapshot and restore delete and re-extract the declared paths as root, so a
declared path is either accepted as written or rejected, never adjusted. An absolute path, a path
starting with `~`, an empty one, a bare `.`, or any path with a `..` component is refused and
reported as a notice. A name that merely begins with dots is allowed, and a trailing `/` is
removed. A snapshot is also refused while anything is still mounted under `build/fs`: the
orchestrator first releases leftover chroot mounts, lazily if it has to, and reports any it cannot.

### The startup picker

```sh
make build                                       # opens the picker
make build BUILD_ARGS=--fresh                    # skip it; run every phase on the existing tree
make build BUILD_ARGS="--restore phase2"         # restore, continue at the next phase
make build BUILD_ARGS="--continue-from phase3"   # resume on the CURRENT tree, no restore
make snapshots                                   # list them
```

When the build has a terminal on both standard input and standard output, and none of `--fresh`,
`--restore`, `--continue-from`, `--plan` or a command-line plan has already decided, the build
opens a picker before running anything. It answers two questions:

- **What to restore.** Row 0 is *start fresh*. Below it is one row per phase that has a snapshot,
  showing when it was taken, its size, its commit, its step count and the phase's duration. A
  commit marked `*` differs from the current one or was dirty; a step count such as `12/40!` marks a
  partial snapshot. The selection opens on the last phase, in build order, that has a snapshot, and
  the line beneath the list names what the selected row restores and which phase the build
  continues from.
- **Whether to write snapshots during this build.** `S` toggles it, and the footer shows
  `writing: on|off`. `--no-snapshot` sets what the picker opens on. Writing off is shown as a
  warning, because a run with writing off that fails in its last phase has nothing to resume from.

| Key | Does |
|---|---|
| `↑` `↓` or `j` `k` | Select a row |
| `Enter`, or a second click on the selected row | Start: fresh, or from the selected snapshot |
| `S` | Toggle snapshot writing |
| `P` | Open the [plan picker](#the-plan-picker) instead |
| `D` | Delete the selected snapshot |
| `Q`, `Esc` | Quit without building |

The picker opens even when no snapshot exists: that is the from-scratch run, where writing
snapshots costs tens of gigabytes and a large part of the time, and the choice matters most.

`--restore`, `--continue-from` and `--delete` accept a phase by its short name (`phase2`), its
directory name (`02_phase2`), its 1-based position, or `latest`, the last phase in build order that
has a snapshot.

*Start fresh* and `--fresh` run every phase on the target tree already in `build/fs`; they do not
empty it. A toolchain or phase-1 script whose marker exists exits at once, and `kpkg` skips each
port already installed from the same recipe. A build from nothing needs `build/fs`, `build/mark`
and `build/cross` emptied first; see [Building from scratch](developing.md#building-from-scratch).

`--continue-from` marks every phase before the one named as skipped and runs the rest on the
current tree. The skipped phases are neither re-run nor re-snapshotted, so a later tree is never
filed under an earlier phase's name.

### How a restore works

A restore replaces the declared paths under `build/` with the contents of the snapshots and marks
the phases they cover as done:

- **Newest wins, per path.** Each path comes from the newest snapshot at or below the target phase,
  so a phase that declares only part of the tree does not lose the rest. Restoring `03_phase3`
  takes `fs` from phase 3 and `cross` and `mark` from phase 1.
- **Nothing may be missing.** A restore is refused when the target phase has no snapshot, or when a
  path that the target or any earlier phase declares has no snapshot at or below the target to come
  from (for example `cross`, once both the toolchain and phase-1 snapshots are deleted). A result
  missing a component would be a tree that never had it.
- **Damaged means absent.** A manifest that does not parse, has no `entries` array, or names an
  archive that is not on disk is treated as no snapshot at all, never as a partial one.
- **Everything is checked first.** Every path and archive is validated before anything is deleted,
  so a rejected restore leaves `build/` untouched.
- **Interrupted restores block.** A restore writes `build/.restore-in-progress`, naming its target,
  before it deletes anything, and removes it when the last archive is extracted. While the marker
  exists, snapshotting and the next build both refuse to run, and the picker shows the interrupted
  restore; restore a snapshot again or run `make cleanbuild`.

Extraction keeps numeric owners, extended attributes and ACLs, and, when run as root, the recorded
owners and the setuid and setgid bits.

**Partial snapshots.** Pressing `S` during a build queues a snapshot of the phase in progress, taken
after the current step finishes, even when snapshot writing is off. It is recorded as partial, with
the number of steps finished. Restoring a partial snapshot re-runs its phase rather than continuing
after it, which is safe because `kpkg` skips the packages already installed.

## Build plans

A snapshot answers "go back to this point". A build plan answers "re-run only this": it restores
nothing, and narrows what the next run executes on the tree already in place.

```sh
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild mesa,networkmanager"
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

1. **Snapshots are suppressed while a plan narrows execution.** Re-running an early phase on a later
   tree would otherwise file that tree under the earlier phase's name. The suppression applies
   whatever the snapshot toggle says, and `--snapshot` overrides it. A rebuild list on its own does
   not narrow, so it keeps them.
2. **Only named ports are forced.** `kpkg install -f` is passed for the ports the plan names and no
   others; their dependencies keep the usual skip-if-installed behaviour. Forcing everything would
   rebuild the whole tree. A named port is forced in every selected phase whose resolved install
   order holds it, so `--phases 03_phase3,04_phase4 --rebuild zlib` builds `zlib` twice; select
   only the phase that lists the port to build it once.
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
filter, and `Enter` or `Esc` returns to the plan. It holds every directory with a `kpkgbuild` under
`ports/core` and `src/packages`, plus every name in any phase's `packages.txt`; that last source is
what puts the `src/desktop` recipes on it, since each is named in `script/05_desktop/packages.txt`,
and it keeps a listed name that has no recipe visible. A name is listed once, and a `packages.txt`
entry records its phase beside it. The list holds up to 4096 names; this repository gives it 2,025.

## kdosbuild

`kdosbuild` is the orchestrator. Its source is `src/build/kdosbuild/`. It is a C program that links
the KDOS libraries `libkbase`, `libkbuild`, `libktui` and `libkcolor` and nothing but the C library
besides. `script/kdosbuild.sh` compiles it before every run with `$CC` (default `cc`), to
`build/.kdosbuild` (or `$KDOSBUILD_BIN`), then runs it with `--script-dir script` and your
arguments.

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
| `--no-snapshot` | Do not write snapshots in this build. With the full-screen interface, this is what the picker opens on |
| `--snapshot` | Write snapshots even for a narrowing plan |
| `--plan` | Open the plan picker and run the plan on this tree |
| `--phases LIST` | Run only these phases |
| `--steps LIST` | Run only these scripts, `PHASE:script.sh` |
| `--rebuild LIST` | Force-rebuild these ports |
| `--plain` | No full-screen interface: plain lines |
| `--json` | No full-screen interface: one JSON object per event. With `--list`, the snapshot inventory as one object |
| `--list` | List snapshots and exit |
| `--delete PHASE` | Delete one phase's snapshot and exit. `PHASE` takes the same forms as `--restore` |
| `--build-dir DIR` | The build directory. Defaults to `$KDOS_BUILD_DIR`, then `build` |
| `--script-dir DIR` | The phase directory. Defaults to `script` |
| `--selftest` | Run the layout and log-classifier assertions and exit. Must be the first argument |
| `--preview SCREEN WxH TIER` | Draw one screen offscreen and print it. Must be the first argument; see [Diagnostics with no build](#diagnostics-with-no-build) |
| `-h`, `--help` | Print usage |

`--list` prints one row per snapshot (phase, time, total size, commit with `*` when it differs from
the checkout or was dirty, and step count) and, under it, each archive's compressed and raw size
and file count. With `--json`, the inventory is one object, `{"commit": ..., "snapshots": [...]}`,
and an empty inventory is that object with an empty `snapshots` array rather than a message.

**Exit status:** 0 when the build finished; 1 when a step failed, a restore failed during the run,
or an unfinished restore blocks the build; 2 for a usage error (an unknown argument or phase,
`--restore` with `--continue-from`, `--phases` or `--steps` with either of them, an invalid step or
rebuild name, `--plan` without a terminal or with `--json`) or a `--restore` refused before the run
starts (the target has no snapshot, a declared path has no source, or `latest` with no snapshot at all).

`KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY`, which `make build` sets from your checkout because `.git`
is not mounted into the container, are recorded in each snapshot so the picker can show which
commit it came from. Run outside the container, `kdosbuild` asks `git` instead.

### How the program is organised

| File | Owns |
|---|---|
| `main.c` | The command line, restore validation and the headless run loop |
| `manager.c` | The execution order, phase expansion and the step runner |
| `snapshot.c` | Writing and extracting archives |
| `stats.c` | Timing history, the time estimate, and the sampler behind the build screen's gauges: load average, memory in use, free space on `build/`, and the size and file count of `build/fs` |
| `tui.c` | The full-screen interface: the startup picker, the plan and port pickers, the build screen |
| `view.c` | The layout and log-classification decisions, the `--selftest` assertions, and the preview fixture |
| `report.c` | The plain and structured output, from one traversal |

The build is a single loop with no threads: check the running step, take those samples, draw, wait
for a key with a deadline. Because nothing runs concurrently, nothing has to guard against drawing
while something else changes state. The one expensive sample, the size and file count of
`build/fs`, is taken by a forked child that writes one line back through a pipe, so a slow walk
cannot stall the screen.

The screen's writes time out after two seconds. A snapshot drains `tar`'s output in the same loop
that redraws the screen, so a terminal that stops reading (a paused pager, a stalled `ssh`) would
otherwise stall the snapshot; with the timeout it costs a dropped frame instead.

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
| `Q` | Stop: the running step's process group gets `SIGTERM` at once, then `SIGKILL` after five seconds, and nothing after it runs. Press again to kill it at once and quit |

A click selects a step, and a second click on a selected phase's fold marker folds it.

When a step fails, a failure panel opens. While it is up it takes every key except `Q`:

| Key | Does |
|---|---|
| `O` | Open the failing step's log in the pager |
| `C` | Copy the log's path to the clipboard. On the Linux console the copy is not possible, and the notice says so |
| `Esc`, `Enter` | Close the panel, leaving the failed step selected |

After a successful build a completion panel appears; `Esc` or `Enter` closes it and leaves the tree
and logs browsable.

### Headless and structured output

When standard output is not a terminal, or `TERM` is `dumb`, or `--plain` or `--json` is given,
the build prints plain lines instead of the full-screen interface. A build with its output
redirected to a file works, and so does a build started with no terminal at all. With no terminal
on standard input there is no startup picker either, so a headless `make build` with no flags runs
every phase on the existing tree.

```sh
kdosbuild --json          # one object per event: build, phase, step, snapshot, notice, restore, result
kdosbuild --list --json   # the snapshot inventory
```

The plain and structured reporters are two implementations of one interface, driven by the same
traversal, so they cannot disagree about what ran. Errors go to standard error and stay out of the
structured stream.

Structured output is one JSON object per line rather than one document, because a build can be
killed at any moment and the point of a machine-readable log is being able to read the end of one
that died. The opening event carries no total step count: a package phase only knows its steps
once it starts. `testing/selftest.sh` drives a synthetic two-phase build through this output to
check the engine itself; see [Testing](testing.md).

### Diagnostics with no build

```sh
kdosbuild --selftest                        # the layout and log-classifier assertions
kdosbuild --preview <screen> <WxH> <tier>   # one screen, as text
```

`--preview` draws one screen offscreen and prints its cells as plain text:

| Argument | Values |
|---|---|
| Screen | `build`, `activity`, `failure`, `pinned`, `complete`, `startup`, `plan`, `packages` |
| Size | Columns and rows, for example `132x43` |
| Tier | The [glyph tier](../06-reference/glossary.md): `rich` (eighth-block glyphs), `vt` (the 512-glyph console font), `ascii` |

It is the way to check a layout without running a build or sitting at a terminal of that size. The
preview data is chosen to break layouts rather than to look plausible: a multi-terabyte total, a
nine-digit file count, an hour-long estimate, a port name longer than any pane. Only the spinner
varies between runs, because it is picked from the clock. The phases are fixed data too, not the
ones under `script/`: the `startup` screen lists a seven-phase table that has no `05_desktop` row,
and the `build` screen stands up three phases. Check the `vt` and `ascii` tiers in
particular for glyphs the console font lacks. Each screen's drawing is a single function that both
the live loop and the preview call, so the preview shows exactly what the build draws.

Run it from a binary your own machine's compiler built. `make snapshots` (or
`script/kdosbuild.sh --list`) compiles `build/.kdosbuild` with the host compiler:

```sh
make snapshots
build/.kdosbuild --preview failure 100x30 vt
```

The next `make build` replaces `build/.kdosbuild` with one compiled in the Alpine build image,
against musl. A glibc machine cannot run that binary, and the shell's error (`No such file or
directory`, or `required file not found`) does not mention the missing musl loader. To keep a host
copy beside the build's, compile it to another path:

```sh
KDOSBUILD_BIN=build/.kdosbuild-host script/kdosbuild.sh --list
```

## libkbuild

`libkbuild` is the half of the orchestrator that reads the build tree and decides; creating
archives and running phases stay in `kdosbuild`. Its source is `src/libs/libkbuild/`, and it is
described with the other libraries in [The C libraries](c-libraries.md).

| File | Owns |
|---|---|
| `kb_phase.c` | Phase discovery, the metadata block, and the snapshot path rules |
| `kb_plan.c` | Plan narrowing, the port list behind the picker, and `build/.devplan.json` with its own strict reader |
| `kb_snap.c` | The snapshot inventory, layered restore selection, the interrupted-restore marker, and mount detection |
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
- [Packaging](../03-architecture/packaging.md) — what `kpkg` does inside a phase, and how ports are grouped
- [Writing ports](writing-ports.md) — the recipes the package phases install
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port each phase installs, by group
- [Build troubleshooting](build-troubleshooting.md) — reading a failed step
- [Testing](testing.md) — what the orchestrator's own tests prove
- [The C libraries](c-libraries.md) — `libkbuild` among the rest

<!-- book-nav -->
---

*Part V — Building and developing, chapter 32.* Previous: [31. Developing](developing.md) · [Contents](../README.md) · Next: [33. Writing ports](writing-ports.md)
