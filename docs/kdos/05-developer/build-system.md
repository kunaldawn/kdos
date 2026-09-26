# The build system

This page explains how a KDOS build works underneath `make build`: what a phase is, how the later
phases run inside the target root filesystem, and the two mechanisms, snapshots and build plans,
that let a small change avoid a full rebuild. It also documents `kdosbuild`, the program that runs
the build, including its flags, keys and machine-readable output.

It is for contributors who want to understand a build, resume one, narrow one, or change the build
machinery itself. If you only want the commands, start with [Developing](developing.md), which covers
fetching sources, the first build and the quick rebuild recipes; come back here when you need to know
why they work.

## The shape of a build

`make build` starts a container from the repository's `Dockerfile` with no network, and runs
`script/kdosbuild.sh` inside it. That script compiles the orchestrator, `kdosbuild`, and runs it. The
orchestrator then runs the phases under `script/` in order. The result is a root filesystem at
`build/fs` and an ISO at `build/iso-build/kdos.iso`.

Two terms used throughout:

- A **port** is a recipe for building one package: a `kpkgbuild` metadata file and a `build.sh`
  beside it. Upstream software lives under `ports/core/`; KDOS's own programs have recipes under
  `src/packages/` and `src/desktop/`. See [Writing ports](writing-ports.md).
- A **phase** is one stage of the build, a numbered directory under `script/`.

## Phases

A phase is a directory under `script/` whose name starts with a number. The orchestrator discovers
them by name and runs them in sorted order.

| Directory | Title | Runs in | Contents | Snapshots |
|---|---|---|---|---|
| `00_toolchain` | Cross Toolchain | Host | 2 scripts: cross binutils and gcc for `x86_64-kdos-linux-musl` | `cross fs mark` |
| `01_phase1` | Base Userland | Host | 16 scripts, `00_file_system.sh` to `13_kinstall.sh`: the `fs/` overlay, kernel headers, musl, libstdc++, ncurses, xz, gzip, tar, toybox, readline, bash, binutils, gcc, make, kpkg, kinstall | `cross fs mark` |
| `02_phase2` | Self-Hosting Bootstrap | Chroot | `packages.txt`, 8 ports: tar, musl, zlib, binutils, diffutils, m4, gawk, gcc rebuilt inside the chroot | `fs` |
| `03_phase3` | Toolchain & Core Libraries | Chroot | `packages.txt`, 98 ports: compilers, build systems, interpreters, base libraries | `fs` |
| `04_phase4` | Userland & GUI Sliver | Chroot | `packages.txt`, 697 ports: system tools, services, the network stack, the Wayland base, Xwayland, the container layer, codecs, and KDOS's own theme and tools | `fs` |
| `05_desktop` | Desktop | Chroot | `packages.txt`, 22 ports: wlroots, `kdos-comp`, `kdos-shell`, `kdos-term`, the daemons, fcitx5, the portals, `kdos-record` | `fs` |
| `05_phase5` | Kernel | Chroot | `packages.txt`, 1 port: `linux` | `fs` |
| `06_packaging` | Packaging | Chroot | 10 scripts: see [The packaging steps](#the-packaging-steps) | `fs iso_root iso-build initramfs initramfs.cpio.gz` |

"Host" means the build container itself; "Chroot" means inside `build/fs`, described in
[The chroot](#the-chroot).

`05_desktop` sorts before `05_phase5` on purpose: the desktop is ordinary userland, and the kernel is
the last thing built before packaging.

A port list names only the ports a phase wants; each port's `depends =` pulls in the rest. The
five lists name 787 distinct ports between them, and their dependency closure adds most of the
rest of `ports/core`.

## How a phase runs

A phase holds numbered shell scripts, a `packages.txt`, or both:

| Form | What happens |
|---|---|
| `00_*.sh`, `01_*.sh`, … | Each script runs in sorted order, its output logged to `build/logs/<phase>/` |
| `packages.txt` | One port name per line; `#` starts a comment. The orchestrator asks `kpkgdepends` for the install order, then installs each port with `kpkg install` |

For a `packages.txt`, the orchestrator runs `kpkgdepends` against an empty package database
(`PKGDB_DIR=/dev/null`), so the order it prints covers every port in the list and its whole
dependency closure. Each port is then its own step, `kpkg install <port>`, with `KPKG_OVERWRITE=1`.
`kpkg` skips a port that is already installed from the same recipe, so a re-run installs only what is
new or changed.

The orchestrator parses what a chroot command prints. `kpkgdepends` writes the install order to
standard output and nothing else, and `script/chroot_exec.sh` sends its own diagnostics to
`build/logs/chroot.log`. Every token read back is checked against a strict pattern, so stray output
fails the step loudly instead of being installed as a package name.

## The phase environment

Each phase has an environment file named after the part of its directory name after the number:
`03_phase3` reads `script/phase3.env.sh`, `05_desktop` reads `script/desktop.env.sh`. It carries the
phase's compiler settings and a metadata block:

```bash
# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* ..."
export CHROOT=1
```

The orchestrator **parses** these files; it never sources them. Several of them end by removing a
build work directory, and sourcing them in the build container would remove the container's
directory instead of the target's. The parser:

- reads only `export NAME=VALUE` lines;
- honours only five keys: `CHROOT`, `KDOS_PHASE_TITLE`, `KDOS_PHASE_DESC`, `KDOS_SNAPSHOT_PATHS` and
  `KDOS_SNAPSHOT_EXCLUDE`;
- requires a literal value, with no variable expansion. An unterminated quote reads as empty.

A phase with no environment file, or no title in it, is titled after its short name with
underscores turned into spaces.

The rest of each environment file is ordinary shell that the phase's steps do source. It names the
compiler outright rather than letting configure scripts discover one, sets `KPKG_STRICT_RECIPE=1` so
a port whose recipe changed is rebuilt, and carries the settings that make packages reproducible. See
[Packaging](../03-architecture/packaging.md).

### `env -i` means every variable must be named

The chroot is entered with an empty environment (`env -i`). Only these variables reach a step inside
it:

| Variable | Value | Read by |
|---|---|---|
| `HOME` | `/root` | Everything |
| `TERM` | The caller's | Everything |
| `PATH` | `/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin` | Everything |
| `KDOS_REPLAY` | `0` or `1` | A step whose "already done" guard must stand down because you picked it (see [Build plans](#build-plans)) |
| `KDOS_ISO_SOURCES` | `0` or `1` | `06_packaging/02_iso.sh`, to copy `ports/`, `src/` and `script/` onto the ISO under `/sources`, with a `SOURCES` stamp. `fs/`, the `Makefile` and the `Dockerfile` are not mounted into the chroot's `/kdos` under `make build`, so they are not copied |
| `KDOS_PACK_KDOS` | `0` or `1` | `06_packaging/01_packs.sh` and `02_iso.sh`, to pack this root filesystem as the base pack `kdos` |

A variable that the `Makefile` passes into the build container, but that `script/chroot_exec.sh` does
not name, reaches the host phases and none of the chroot ones. Packaging is a chroot phase, so an
opt-in packaging flag that is not forwarded silently produces an ordinary image. Adding such a flag
is therefore two edits: the `Makefile` passes it into the container with `-e`, and
`script/chroot_exec.sh` names it on the `env -i` line.

`/usr/local/bin`, where KDOS's own tools such as `kdos-appbox` install, comes last in the search path.
Packaging calls those tools by name, so it has to be on the path. Putting it first would let a local
binary shadow a system one while a port is being configured, which is a much harder failure to spot.

## The chroot

The later phases run inside the target root filesystem, `build/fs`. `script/chroot_exec.sh` enters
it for each command:

1. Requires root.
2. Unmounts anything a previous, killed run left mounted under `build/fs`, retrying a busy mount and
   finally detaching it lazily.
3. Bind-mounts `/dev`, and mounts a fresh `proc`, `sysfs` and two `tmpfs` at `/proc`, `/sys`, `/tmp`
   and `/run`.
4. Bind-mounts the repository at `/kdos`, `build/` at `/kdos/build`, `ports/` at `/ports`, and
   `script/` and `src/` under `/kdos`.
5. Enters with `env -i` and the variables above, and changes to `/kdos`, so relative paths in
   scripts mean what they say.
6. Unmounts everything again when the command exits.

The outer container mounts `src/`, `fs/`, `script/` and `ports/` read-only; only `build/` is
writable. A build cannot modify its own sources.

## Snapshots

Every completed phase is archived to `build/snapshots/<phase>/`, so a later build can start from it
instead of from the beginning.

What is archived is declared by the phase, in `KDOS_SNAPSHOT_PATHS` and `KDOS_SNAPSHOT_EXCLUDE`.
Paths are relative to `build/`; the exclusions keep out work directories, pseudo-filesystems and bind
mounts. A snapshot directory holds one archive per declared path (`.tar.zst` when `zstd` is
available, otherwise `.tar.gz` or `.tar`), plus `manifest.json`. `build/snapshots/timings.json` keeps
the step timing history that the build screen's estimate uses. A phase with no declared paths is
never snapshotted.

**Disk space.** Budget roughly 84 GB for a complete set: a few hundred megabytes for each early
phase, and about 59 GB for packaging alone, because it carries `iso_root` and `iso-build`. A new
archive is written beside the old one before the old one is replaced, so a snapshot is refused
unless the free space holds the previous snapshot's size plus a fifth (or, for a phase's first
snapshot, a third of the raw size plus a fifth). `make cleanbuild` keeps snapshots; `make clean`
removes them.

**Safety rules.** Snapshot and restore delete and re-extract the declared paths as root, so a
declared path is either accepted as written or rejected, never adjusted. An absolute path, an empty
one, a bare `.`, or any path with a `..` component is refused. A name that merely begins with dots is
allowed. A snapshot is also refused while anything is still mounted under `build/fs`: the
orchestrator releases leftover chroot mounts first and reports any it cannot.

### The startup picker

```sh
make build                                       # opens the picker
make build BUILD_ARGS=--fresh                    # skip it; run every phase on the existing tree
make build BUILD_ARGS="--restore phase2"         # restore, continue at the next phase
make build BUILD_ARGS="--continue-from phase3"   # resume on the CURRENT tree, no restore
make snapshots                                   # list them
```

When the build has a terminal, and none of `--fresh`, `--restore`, `--continue-from` or a
command-line plan has already decided, the build opens a picker before running anything. It asks two
questions:

- **What to restore.** Row 0 is *start fresh*. Below it is one row per phase that has a snapshot,
  with its size, commit, step count and duration. `Enter` starts.
- **Whether to write snapshots during this build.** `S` toggles it; the footer shows
  `writing: on|off`. `--no-snapshot` sets what the picker opens on.

The picker opens even when no snapshot exists. That is the from-scratch run, where writing snapshots
costs tens of gigabytes and a large part of the time, and where turning them off means a failure in
the last phase has nothing to resume from.

`--restore`, `--continue-from` and `--delete` accept a phase by its short name (`phase2`), its
directory name (`02_phase2`), its 1-based position, or `latest`, the newest phase with a snapshot.

*Start fresh* and `--fresh` run every phase on the tree already in `build/`; they do not empty it.
A toolchain or phase-1 script whose marker under `build/mark/` exists exits at once, and `kpkg`
skips each port already installed from the same recipe. A build from nothing needs `build/fs`,
`build/mark` and `build/cross` emptied first; see
[Building from scratch](developing.md#building-from-scratch).

How a restore chooses what to extract:

- **Newest wins, per path.** Each declared path comes from the newest snapshot at or below the target
  phase, so a phase that declares only part of the tree does not lose the rest.
- **Gaps are refused.** Restoring a phase whose earlier snapshots are missing is refused, because the
  result would be a root filesystem that never had the target's packages.
- **Damaged means absent.** A manifest that does not parse, has no entries, or names an archive that
  is not on disk is treated as no snapshot at all, never as a partial one.
- **Interrupted restores block.** A restore writes `build/.restore-in-progress` and removes it when it
  finishes. While the marker exists, snapshotting and the next build both refuse to run; restore a
  snapshot again or run `make cleanbuild`.

A plan that narrows execution turns snapshot writing off regardless of the toggle, unless
`--snapshot` is passed: a snapshot of a partly re-run tree would be filed under a phase whose
contents it does not match.

Pressing `S` during a build queues a partial snapshot of the phase in progress, taken after the
current step. Restoring a partial snapshot re-runs that phase, which is safe because installed
packages are skipped; the picker marks it as partial.

## Build plans

Snapshots answer "go back". A plan answers "re-run only this": it restores nothing, and narrows what
the next run executes on the tree already in place.

```sh
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild mesa,networkmanager"
make build BUILD_ARGS=--plan     # interactive; search the port list
```

| Flag | Selects |
|---|---|
| `--phases LIST` | Only these phases, comma-separated |
| `--steps LIST` | Only these scripts, as `PHASE:script.sh` |
| `--rebuild LIST` | Rebuild these ports even though they are installed |
| `--plan` | Open the interactive plan picker. Needs a terminal, and cannot be combined with `--json` |

`--phases` and `--steps` cannot be combined with `--restore` or `--continue-from`. The plan in force
is saved to `build/.devplan.json`.

Three rules make a plan safe:

1. **Snapshots are suppressed while a plan narrows execution.** Re-running an early phase on a later
   tree would otherwise file that tree under the earlier phase's name. A rebuild list on its own does
   not narrow, so it keeps them.
2. **Only named ports are forced.** `kpkg install -f` is passed for the ports the plan names and no
   others; their dependencies keep the usual skip-if-installed behaviour. Forcing everything would
   rebuild the whole tree.
3. **"Already done" guards stand down only for picked steps.** Many scripts start by checking a
   marker file so they do not repeat expensive work. `KDOS_REPLAY=1` is exported for the steps you
   named and forwarded into the chroot, and those scripts run again.

A `--rebuild` name that no selected phase ever reached is reported, rather than silently doing
nothing.

The picker's port list holds every directory with a `kpkgbuild` under `ports/core` and
`src/packages`, plus every name in any phase's `packages.txt`; that last source is what puts the
`src/desktop` recipes on it, since each is named in `script/05_desktop/packages.txt`. A name is listed
once, from the first place it is found, and a `packages.txt` entry adds its phase to a port already
listed. The list has no size cap, because a port missing from it is a port the picker cannot
rebuild.

## kdosbuild

`kdosbuild` is the orchestrator. Its source is `src/build/kdosbuild/`. It is a C program linking
`libkbase`, `libkbuild`, `libktui` and `libkcolor` and nothing else, and `script/kdosbuild.sh`
compiles it in a couple of seconds, with `$CC` (default `cc`), to `build/.kdosbuild` (or
`$KDOSBUILD_BIN`) before every run. That script then runs it with `--script-dir script` and your
arguments, and when the run ends hands every top-level entry of `build/` except `fs` back to the
calling user. `build/fs` stays root's: see
[Where the build puts things](developing.md#where-the-build-puts-things).

`make snapshots` runs `script/kdosbuild.sh --list` directly on your machine, so it needs a C compiler
there; `make build` runs it inside the container.

### Flags

| Flag | Effect |
|---|---|
| `--fresh` | Skip the startup picker and run every phase on the existing tree. Steps and ports already done are skipped; see [The startup picker](#the-startup-picker) |
| `--restore PHASE` | Restore a snapshot and continue after it. `PHASE` is a short name, directory name, 1-based index, or `latest` |
| `--continue-from PHASE` | Resume at `PHASE` on the existing tree, with no restore. `PHASE` takes the same forms as `--restore` |
| `--no-snapshot` | Do not write snapshots in this build. With the full-screen interface, this is what the picker opens on |
| `--snapshot` | Write snapshots even for a narrowing plan |
| `--plan` | Open the build-plan picker and run the plan on this tree |
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

**Exit status:** 0 when the build finished; 1 when a step failed, a restore failed during the run,
or an unfinished restore blocks the build; 2 for a usage error (an unknown argument or phase,
conflicting flags, `--plan` without a terminal) or a `--restore` refused before the run starts (an
earlier snapshot missing, or `latest` with no snapshot at all).

`KDOS_GIT_COMMIT` and `KDOS_GIT_DIRTY`, which `make build` sets from your checkout, are recorded in
each snapshot so the picker can show which commit it came from.

### How the program is organised

| File | Owns |
|---|---|
| `main.c` | The command line |
| `manager.c` | The execution order and the step runner |
| `snapshot.c` | Writing and extracting archives |
| `stats.c` | Timing history, the estimate, the telemetry sampler |
| `tui.c` | The full-screen interface |
| `view.c` | The layout and log-classification decisions, and the preview fixture |
| `report.c` | The plain and structured output, from one traversal |

The build is a single loop with no threads: check the running step, sample telemetry, draw, wait for
a key with a deadline. Because nothing runs concurrently, nothing has to guard against drawing while
something else changes state.

A step that cannot be started at all (the pipe or the fork is refused, on a machine out of file
descriptors or memory) fails exactly like a step that exited non-zero: return code 999, the step and
its phase marked failed, a named notice, and the run stopped.

The screen's writes time out after two seconds. A terminal that stops reading (a paused pager, a
stalled `ssh`) costs a dropped frame rather than stalling the build and the snapshot it is writing.

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
| `T` | Cycle through the accent palettes |
| `Q` | Stop after the current step; press again to kill it and quit |

When a step fails, a failure panel opens. While it is up it takes every key except `Q`:

| Key | Does |
|---|---|
| `O` | Open the failing step's log in the pager |
| `C` | Copy the log's path to the clipboard. On the Linux console the copy is not possible, and the notice says so |
| `Esc`, `Enter` | Close the panel, leaving the failed step selected |

After a successful build a completion panel appears; `Esc` or `Enter` closes it and leaves the tree
and logs browsable.

### Headless and structured output

When standard output is not a terminal, or `TERM` is `dumb`, or `--plain` or `--json` is given, the
build prints plain lines instead of the full-screen interface. A build with its output redirected to
a file works, and so does a build started with no terminal at all.

```sh
kdosbuild --json          # one object per event: build, phase, step, snapshot, notice, restore, result
kdosbuild --list --json   # the snapshot inventory
```

The plain and structured reporters are two implementations of one interface, driven by the same
traversal, so they cannot disagree about what ran.

Structured output is one JSON object per line rather than one document, because a build can be
killed at any moment and the point of a machine-readable log is being able to read the end of one
that died. The opening event carries no total step count: a package phase only knows its steps once
it starts.

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
| Tier | `rich` (eighth-block glyphs), `vt` (the 512-glyph console font), `ascii` |

Run it on your machine from a binary your machine's compiler built. `make snapshots` (or
`script/kdosbuild.sh --list`) compiles `build/.kdosbuild` with the host compiler:

```sh
make snapshots
build/.kdosbuild --preview failure 100x30 vt
```

The next `make build` replaces `build/.kdosbuild` with one compiled in the Alpine build image, against
musl. A glibc machine cannot run that binary, and the shell's error (`No such file or directory`,
or `required file not found`) does not mention the missing musl loader. To keep a host copy beside the build's, compile
it to another path with `KDOSBUILD_BIN=build/.kdosbuild-host script/kdosbuild.sh --list`.

It is the way to check a layout without running a build or sitting at a terminal of that size. The
preview data is chosen to break layouts rather than to look plausible: a multi-terabyte total, a
nine-digit file count, an hour-long estimate, a port name longer than any pane. Only the spinner
varies between runs, because it is picked from the clock. Check the `vt` and `ascii` tiers in
particular for glyphs the console font lacks.

Each screen's drawing is a single function, and both the live loop and the preview call it, so the
preview always shows exactly what the build draws.

## libkbuild

`libkbuild` is the half of the orchestrator that reads and decides; creating archives and running
phases stay in `kdosbuild`. Its source is `src/libs/libkbuild/`.

| File | Owns |
|---|---|
| `kb_phase.c` | Phase discovery and the metadata block |
| `kb_plan.c` | Plan narrowing, `build/.devplan.json`, and the port list behind the picker |
| `kb_snap.c` | The snapshot inventory, layered restore, the interrupted-restore marker, and mount detection |
| `kb_json.c` | A read-only JSON scanner for exactly those files |

The JSON scanner refuses anything that does not parse completely, including truncation, trailing
junk or a trailing comma. Every caller treats a parse failure as "absent", so a lenient parser would
turn a corrupt manifest into a confident wrong answer.

Two further rules, each preventing a silent wrong answer:

- **Files are read to their real end, never to their reported size.** Files under `/proc` report a
  size of zero, so a size-bounded read of the mount table would find no mounts, and a snapshot would
  run over a live bind mount.
- **An empty structure means absent.** The interrupted-restore marker is tested for content. A caller
  that tested the structure itself would see an interrupted restore forever.

## Syncing `fs/`

`fs/` holds the files KDOS adds to the root filesystem directly: configuration, scripts, skeleton
home directories. `01_phase1/00_file_system.sh` copies it into `build/fs`.

A plain recursive copy overwrites but never deletes, so a file removed from `fs/` would linger in an
incremental build tree. The step therefore records every path `fs/` provided in
`/var/lib/kdos/fs-manifest`, and on the next sync deletes any path in the old manifest that `fs/` no
longer provides. Two details keep that safe:

- The manifest only ever lists files, and it is written after the copy, so a path a package later
  installs over is not the manifest's to remove.
- The manifest is built without listing options that only the full-featured `ls` has. The build
  image's compact `ls` rejects them, and an empty manifest would protect nothing.

`/etc/passwd`, `/etc/group` and `/etc/shadow` are merged, not overwritten: package install hooks add
service accounts long after this step runs, and a plain copy on a later sync would delete them.
Entries from `fs/` win; accounts added at runtime are appended back.

Home directories are created later, by `06_packaging/00_user.sh`, for each ordinary user (UID 1000
to 65533) with a home directory in `/etc/passwd`. It clears the generated trees that `/etc/skel`
owns outright (`.icons`, `.themes`, `.config/kdos-comp`, `.config/cosmic`, `.config/kdos-con`,
`.local/share/applications` and `.local/share/color-schemes`) before copying `/etc/skel` in, so
something `/etc/skel` stops providing does not linger in a home. An edit made by hand to one of those
trees in a home under `build/fs` is lost on the next packaging run.

## Sweeping orphaned packages

A port deleted from `ports/` leaves its package installed unless something removes it. `fs/` has a
manifest; packages have `06_packaging/00_orphans.sh`, which removes every installed package that has
no recipe in `ports/core`, `src/packages`, `src/desktop` or `src/libs`. `testing/preflight.sh`
reports the same orphans in seconds. A package that fails to uninstall is reported and left in place rather than stopping the
build, so one damaged package cannot prevent the ISO from being produced.

## The packaging steps

`06_packaging` runs these scripts in sorted order inside the chroot:

| Step | Does |
|---|---|
| `00_cleanup.sh` | Removes build-only files from the tree, and replaces Python bytecode (see below) |
| `00_launchers.sh` | Reconciles the generated application launchers in `/etc/skel` with the packs the image carries |
| `00_orphans.sh` | Removes installed packages that have no recipe in the tree |
| `00_theme.sh` | Seeds the default theme into `/etc/skel` |
| `00_udev_hwdb.sh` | Compiles `/etc/udev/hwdb.bin` |
| `00_user.sh` | Creates the home directory of each ordinary user (UID 1000–65533) from `/etc/skel`, first clearing the generated trees skel owns outright (see [Syncing `fs/`](#syncing-fs)) |
| `00_whatis.sh` | Builds the manual-page index for `apropos` and `whatis` |
| `01_initramfs.sh` | Builds the initramfs |
| `01_packs.sh` | Creates the pack store directories, and with `KDOS_PACK_KDOS=1` the base pack `kdos` in `build/kdos-base` |
| `02_iso.sh` | Assembles `iso_root` and writes `build/iso-build/kdos.iso` |

The ISO carries no applications. `01_packs.sh` creates only `/var/lib/kdos/packs/staging` and
`/var/lib/kdos/packs/mnt`; applications are built by podman on the machine that wants them. See
[Packs and boxes](../03-architecture/packs-and-boxes.md).

## Databases stamped into the image

Two tools read a compiled database that no single package installs, because each is built from every
package's files. `kpkg` keeps both current on every install and removal (they are
[shared indexes](writing-ports.md#shared-indexes)). `00_udev_hwdb.sh` and `00_whatis.sh` rebuild them
from scratch over the finished tree and check the result, so the image never carries an index left
half-built by an interrupted build.

**The hardware database.** `udevadm hwdb --update` compiles `/etc/udev/hwdb.bin` from the `hwdb.d`
text files eudev ships. Many of eudev's rules import properties from it, and without the file those
imports return nothing and log nothing above debug level. The machine then silently behaves as
hardware with no quirks: laptop key mappings, the `EVDEV_ABS_*` touchpad sizes libinput relies on,
and `ID_VENDOR_FROM_DATABASE` / `ID_MODEL_FROM_DATABASE` are all missing. The file goes in
`/etc/udev`, which libudev reads first, rather than `/usr/lib/udev`: the initramfs copies
`/usr/lib/udev` whole, and about 10 MB of database would sit in RAM on every boot for a stage that
uses none of it.

**The manual index.** `makewhatis` writes a `mandoc.db` into each manual directory on the manual
path. `man` works without one, but `apropos` and `whatis` print nothing and exit 0 without it, which
looks the same as "nothing matches".

Both steps run after the orphan sweep, so a removed package's rules and pages are not indexed, and
before the initramfs and ISO steps; sorted order does the sequencing. Neither trusts an exit status:
`udevadm hwdb --update` exits 0 having written nothing when it finds no sources, and `makewhatis`
exits non-zero for one unreadable page while still writing a complete database. Each step checks the
file it produced instead.

**Python bytecode.** `00_cleanup.sh` deletes every `__pycache__`, `.pyc` and `.pyo` the build left,
then compiles `/usr/lib/python3*` again with `--invalidation-mode checked-hash`. Hash-checked
bytecode is the same bytes from the same tree, so the image is reproducible, and the interpreter
checks the hash on import, so a source file a later upgrade replaces is recompiled rather than served
stale. Deleting without recompiling would make every Python program compile everything it imports on
every start, because `/usr` is read-only to it.

## Fetching sources in a container

`make build` never downloads anything; `make fetch` does, beforehand. Most of that work happens on
your machine with `curl` and `sha256sum` (see
[Where sources come from](developing.md#where-sources-come-from)). The exception is a *vendor
bundle*: a tarball of a Rust, Go, Python or Haskell port's dependencies, generated by that language's
own package manager. `ports/fetch` generates one only when the port directory, the cache and the
source archive all lack it, and it does so inside a container image of its own, `kdos-fetch`, built
from `ports/Containerfile.fetch`.

The image carries Rust, Go, Node.js, GHC and cabal at the versions this tree's recipes pin, passed in
as build arguments. That pinning is the point: a package manager newer than the compiler that will
build the port can write a lock file the older one refuses.

The container runs as your user, because every file it writes lands in your tree and your source
cache. That user has no account inside the image, so its home directory and each toolchain's cache
are pointed at `build/fetch-home`; cargo and go refuse to run without a writable home. A cache
outside the repository (`KDOS_SRCCACHE`) is mounted into the container beside it.

## See also

- [Developing](developing.md) — the commands and the iteration loops
- [Packaging](../03-architecture/packaging.md) — what `kpkg` does inside a phase
- [Writing ports](writing-ports.md) — the recipes the package phases install
- [Build troubleshooting](build-troubleshooting.md) — reading a failed step
- [Testing](testing.md) — what the orchestrator's own tests prove
- [The C libraries](c-libraries.md) — `libkbuild` among the rest
