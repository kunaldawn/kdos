# The build system

KDOS is built by a sequence of phases, orchestrated by a C program that runs inside a container.
This page describes that machinery: what a phase is, how the later ones run inside the target root
filesystem, and the two mechanisms — snapshots and build plans — that keep an incremental change
from costing a full build.

For the day-to-day commands, see [Developing](developing.md).

## Phases

A phase is a directory under `script/`, discovered by name and run in sorted order.

| Directory | Title | Runs in | Snapshots |
|---|---|---|---|
| `00_toolchain` | Cross Toolchain | Host | `cross fs mark` |
| `01_phase1` | Base Userland | Host | `cross fs mark` |
| `02_phase2` | Self-Hosting Bootstrap | Chroot | `fs` |
| `03_phase3` | Toolchain & Core Libraries | Chroot | `fs` |
| `04_phase4` | Userland & GUI Sliver | Chroot | `fs` |
| `05_desktop` | Desktop | Chroot | `fs` |
| `05_phase5` | Kernel | Chroot | `fs` |
| `06_packaging` | Packaging | Chroot | `fs iso_root iso-build initramfs initramfs.cpio.gz` |

`05_desktop` sorts before `05_phase5`, and that ordering is deliberate rather than an accident of
naming: the desktop is ordinary userland, and the kernel is the last thing built before packaging.

## How a phase runs

A phase is either a set of numbered shell scripts or a `packages.txt`, and the orchestrator handles
each differently:

| Form | What happens |
|---|---|
| `00_*.sh`, `01_*.sh`, … | Each script runs in order, its output logged to `build/logs/<phase>/` |
| `packages.txt` | Port names. `kpkgdepends` resolves the order, then each port is installed |

Five phases carry a `packages.txt` — `02_phase2`, `03_phase3`, `04_phase4`, `05_desktop` and
`05_phase5`. The rest are scripts, and a phase may have both.

Anything a chroot command prints is parsed. The dependency resolver writes the install order to
standard output and nothing else, so diagnostics from inside the chroot go to a log file instead.
The orchestrator reads standard output only and validates every token against a strict pattern, so
noise fails loudly rather than being installed as a package.

## The phase environment

Each phase has a `script/<name>.env.sh` carrying its compiler settings and a metadata block:

```bash
# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* ..."
```

These files are parsed, never sourced. Several of them end by removing a build work directory,
which at source time would hit the build container's filesystem rather than the target's. The
parser therefore accepts only `export NAME=VALUE` lines, honours only five keys — `CHROOT` and the
four `KDOS_*` above — and requires a literal value. There is no expansion, and an unterminated
quote reads as empty.

### `env -i` means every variable must be named

The chroot is entered with a cleared environment, so a variable a chroot step reads must be named
on that command line. Three are forwarded:

| Variable | Read by |
|---|---|
| `KDOS_REPLAY` | A step whose mark-file guard must stand down because the developer picked it |
| `KDOS_ISO_SOURCES` | Packaging, to put the sources on the medium |
| `KDOS_PACK_KDOS` | Packaging, to build this root filesystem as a base pack |

A knob passed into the container but not forwarded here reaches every host step and no chroot one.
Packaging is a chroot phase, so an opt-in packaging flag that is not forwarded produces an ordinary
image and says nothing about why. A new opt-in packaging flag is two edits: the `Makefile` passes
it into the container, and `script/chroot_exec.sh` names it on the `env -i` line.

The phase environments also set the compiler by name rather than letting configuration scripts
discover it, set `KPKG_STRICT_RECIPE=1` so the build rebuilds a port whose recipe changed, and
carry the five lines that make packages reproducible. See
[Packaging](../03-architecture/packaging.md).

## The chroot

Later phases run inside the target root filesystem. The entry script:

1. Requires root.
2. Bind-mounts `/dev`, and mounts a fresh `proc`, `sysfs` and two tmpfs at `/proc`, `/sys`, `/tmp`
   and `/run`.
3. Bind-mounts the repository at `/kdos`, `build/` at `/kdos/build`, `ports/` at `/ports`, and
   `script/` and `src/` under `/kdos`.
4. Enters with `env -i`, a fixed search path, and the three forwarded variables.
5. Changes to `/kdos` so relative paths in scripts mean what they say.

The search path appends the local binary directory rather than prepending it. KDOS's own tools
install there, and leaving it out entirely makes a packaging step fail with a command not found —
but prepending would let a local binary shadow a system one during a port's configuration, which is
a harder bug to see.

The repository is mounted read-only by the outer container except for `build/`, so a build cannot
modify its own sources.

## Snapshots

Every completed phase is archived to `build/snapshots/<phase>/`.

What is snapshotted is declared by the phase, in the metadata block above. Paths are relative to
`build/`, with exclusion patterns for the things that should not travel: work directories,
pseudo-filesystems and the bind mounts. Each snapshot directory holds one compressed archive per
declared path, plus a manifest and timings. A phase with no declared paths is never snapshotted.

Budget roughly 84 GB for a complete set — a few hundred megabytes for the early phases and 59 GB
for packaging alone, which carries `iso_root` and `iso-build`. `snap_create` writes its archive
beside the old one, so the free-space guard refuses a phase whose previous snapshot plus a fifth of
it does not fit. The full clean removes snapshots; the build clean keeps them.

A declared path is either kept or reported as rejected, never quietly repaired. Snapshot and
restore delete and re-extract those paths as root, so an absolute path, an empty one, a bare dot,
or anything containing a parent-directory component is refused. A name that merely begins with dots
is a real name and is allowed; only a whole parent component counts.

### Restoring

```sh
make build                                       # opens a picker
make build BUILD_ARGS="--restore phase2"         # restore, continue at the next phase
make build BUILD_ARGS="--continue-from phase3"   # resume on the CURRENT tree, no restore
make snapshots                                   # list them
```

Restore selection is layered and newest-wins: each declared path comes from the newest snapshot at
or below the target phase, so a phase that declares only part of the tree does not lose the rest.

Restoring a phase whose earlier snapshots are missing is refused, because the fallback would build
against a root filesystem that never had the target's packages.

A manifest that does not parse, carries no entries, or names an archive that is not on disk reads
as absent, never as partial. A half-read manifest that looks complete is what loses a tree.

An interrupted restore leaves a marker, and both snapshotting and the next build refuse to run
until it is resolved. An empty structure is not a marker: the reader must distinguish "no marker"
from "a marker with nothing in it", or every caller that tests the structure for truth treats the
restore as interrupted for ever.

Pressing the snapshot key during a build takes a partial snapshot. Restoring one re-runs that
phase, which is safe because installed packages are skipped, and the picker marks it as partial.

## Build plans

Snapshots answer "go back". A plan answers "re-run only this": it restores nothing and narrows what
the next run executes on the tree already in place.

```sh
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild mesa,networkmanager"
make build BUILD_ARGS=--plan     # interactive; search the port list
```

Three properties make that safe:

- Snapshots are suppressed whenever a plan narrows execution. Re-running an early phase on a later
  tree would otherwise file that tree under the earlier phase's name. A rebuild list alone does not
  narrow, so it keeps them.
- Forcing is passed only for the ports the plan named. Forcing resolves against an empty database
  and rebuilds what is named; dependencies keep the ordinary skip-if-installed behaviour. Passing
  it blanketly would rebuild every package in the tree.
- Mark-file guards stand down only for a step that was picked. Many scripts begin by checking a
  marker so they do not repeat expensive work; `KDOS_REPLAY` is exported for explicitly named steps
  and forwarded into the chroot.

A rebuild request that no selected phase ever reached is reported, rather than silently doing
nothing.

The picker's port list is every directory holding a `kpkgbuild` under `ports/core` and
`src/packages`, plus every name in any phase's `packages.txt` — which is what puts the
`src/desktop` recipes on it, each already named in `script/05_desktop/packages.txt`. A name is
listed once: the first repository wins, and a packages.txt entry stamps the phase onto a port
already there. The list grows to fit; it is never capped, because a port missing from it is a port
the picker cannot ask to rebuild.

## kdosbuild

The orchestrator lives in `src/build/kdosbuild/`. It is a C program linking `libkbase`,
`libkbuild`, `libktui` and `libkcolor` and nothing else, compiled on demand in a couple of seconds
by `script/kdosbuild.sh`.

| File | Owns |
|---|---|
| `main.c` | The command line |
| `manager.c` | The execution order and the step runner |
| `snapshot.c` | Writing and extracting archives |
| `stats.c` | Timing history, the estimate, the telemetry sampler |
| `tui.c` | The full-screen interface |
| `view.c` | The parts of the screens that are decisions — layout, log classification — plus the preview fixture |
| `report.c` | The headless output, plain and structured, from one traversal |

The build is the main loop. There are no threads: pump the running child, pump the sampler, draw,
wait for a key with a deadline. Nothing has to be careful about drawing concurrently with a caller,
because nothing can.

A step the runner cannot start — the pipe or the fork refused, on a machine out of descriptors or
out of memory — fails exactly like a step that exited non-zero: return code 999, the step and its
phase marked failed, a named notice, and the run stopped. The stamp has to be terminal, because the
cursor only moves past a step that finished or stopped the run; a step left marked running is
started again on the next pump, for ever, leaking its log descriptor each time.

### Keys

`↑↓` or `jk` select, `Space` folds a group, `F` toggles follow, `S` queues a partial snapshot, `Q`
stops — twice to force.

Five more are worth knowing because nothing else announces them:

| Key | Does |
|---|---|
| `/` | Search the selected step's log — it marks rather than filters, because a build log is read for the context around a hit |
| `n` / `N` | Walk the marks |
| `E` | Jump to the first line classified as an error |
| `O` | Open the step's log in the configured pager, by argument vector, never through a shell |
| `T` | Cycle the accents live |

Cycling accents needs both a palette reload and a full invalidation. A palette change alone leaves
every untouched cell wearing the old colours.

The failure panel owns every key except `Q` while it is up, and adds `C`, which copies the failing
step's log path. On the Linux VT the clipboard escape is a no-op, so the notice says whether the
copy actually happened rather than claiming success either way.

### Headless and structured output

A non-terminal output or a dumb terminal gets plain lines instead of the full-screen interface. A
build with its output redirected therefore works, and a build with no terminal at all can start —
which is also what makes the engine testable without a pseudo-terminal.

```sh
kdosbuild --json          # one object per event: build, phase, step, snapshot, notice, restore, result
kdosbuild --list --json   # the snapshot inventory
```

Both are the same traversal. The plain reporter and the structured one are two implementations of
one interface, and the runner does not know which it has, so the two views cannot disagree about
what ran.

Structured output is one object per line rather than one document, because a build can be killed at
any moment and the reason to have a machine-readable log is reading the tail of one that died.
There is deliberately no total step count in the opening event: a package phase expands only when
it is entered.

### Diagnostics with no build

```sh
kdosbuild --selftest                    # the layout and log-classifier assertions
kdosbuild --preview <screen> <WxH> <tier>
```

`--preview` draws one screen offscreen and dumps the cell buffer as plain text. The screens are
`build`, `activity`, `failure`, `pinned`, `complete`, `startup`, `plan` and `packages`; the tiers
are `rich`, `vt` and `ascii`.

It is the only way to see a layout without a whole build and a terminal. Several geometry
defects in this interface were found by hand arithmetic, and none of them was visible to the
compiler.

Preview also forces each screen's drawing half out of its event loop into a single draw function.
The loop calls that and nothing else, because a second drawing path would be a second thing to keep
in agreement with the one people look at.

The preview fixture is chosen to break layouts rather than to look plausible: a multi-terabyte
total, a nine-digit file count, an hour-scale estimate, a port name longer than any pane. The one
thing not reproducible between runs is the spinner, which is picked from the clock.

Read the low-tier output specifically, for glyphs the console font lacks.

## libkbuild

The deciding half of the orchestrator. It reads and it chooses; creating archives and running
phases stays with `kdosbuild`.

| File | Owns |
|---|---|
| `kb_phase.c` | Phase discovery and the metadata block |
| `kb_plan.c` | Plan narrowing, and the port discovery behind the picker |
| `kb_snap.c` | The snapshot inventory, layered restore, the interrupt marker, and mount detection |
| `kb_json.c` | A read-only structured-data scanner for exactly those files |

The scanner refuses anything that does not parse whole — truncated, trailing junk, a trailing comma
— because every caller treats a parse failure as *absent*, and a lenient parser turns a corrupt
manifest into a confident wrong answer.

Two rules the readers here keep, each guarding a silent wrong answer:

- A file is read to real end-of-file, never to its reported size. Files under the process filesystem
  report a size of zero, so a size-bounded read returns an empty mount list — and "is anything
  mounted here" then answers *no*, which is the answer that lets a snapshot run over a live bind
  mount.
- An empty structure is absence, not presence. The interrupt marker is tested for content, because
  a caller testing the structure itself for truth would see an interrupted restore for ever.

## Syncing `fs/`

`fs/` is copied verbatim into the target root filesystem. A file deleted from `fs/` must disappear
from the tree, and a plain recursive copy overwrites but never removes — so a dropped path would
linger for the life of an incremental build tree.

The sync therefore records every path `fs/` provided in a manifest, and on the next sync deletes
any path present in the old manifest that `fs/` does not provide this time.

Two traps in that mechanism:

- The manifest is written after the copy, so a package that later owns the same path is not the
  manifest's to remove.
- The manifest must be built without listing options that only the full-featured `ls` has. The
  build image's compact one rejects them and writes an empty manifest, which silently protects
  nothing.

The account files are merged, not overwritten. Package install hooks add service users long after
the early phase runs, and a plain copy on a re-sync would delete them. Repository entries win;
runtime-added entries are appended back.

The user-directory step needs the same treatment for the trees it copies into the home directory,
and clears them before copying.

## Sweeping orphaned packages

A port deleted from the tree leaves its package installed unless something removes it. `fs/` has a
manifest guard; packages need one too.

A packaging step removes every installed package with no recipe in any port repository, and
`testing/preflight.sh` reports the same thing in seconds instead of at the end of a whole build.
Neither is fatal on failure: an orphan with a damaged manifest must not stop the ISO from being
rolled.

## Databases stamped into the image

Two consumers read a compiled database that no package installs, because neither can be built until
every package is in place. `script/06_packaging/00_udev_hwdb.sh` and
`script/06_packaging/00_whatis.sh` build them.

`udevadm hwdb --update` compiles `/etc/udev/hwdb.bin` from the `hwdb.d` text eudev ships. Half of
eudev's rules open with `IMPORT{builtin}="hwdb ..."`, and that import returns nothing at all when
the binary is absent, logging nothing above debug level: the laptop key quirks, the `EVDEV_ABS_*`
touchpad overrides libinput sizes a device from, and `ID_VENDOR_FROM_DATABASE` /
`ID_MODEL_FROM_DATABASE` are simply missing, and the machine reads as hardware with no quirks
rather than as a missing file. The trie goes to `/etc/udev`, which libudev reads first, and not to
`--usr`'s `/lib/udev` — that is `/usr/lib/udev` on a merged `/usr`, and the initramfs step copies
`/usr/lib/udev` wholesale, so ~10 MB of trie would sit in RAM on every boot for a stage that
imports no hwdb property.

`makewhatis` writes a `mandoc.db` into each manual root on the manpath. `man` needs none — it falls
back to walking the filesystem — but `apropos` and `whatis` reach `mansearch()` only and exit 0
printing nothing without one, which is indistinguishable from "nothing matches".

Both steps run after the orphan sweep, so a swept package's rules and pages are not indexed, and
before the initramfs and ISO steps, which carry the tree into the image; lexicographic order does
the sequencing. Neither trusts an exit status: `udevadm hwdb --update` exits 0 having written
nothing when it finds no sources, and `makewhatis` exits non-zero for a single unreadable page
while still writing a complete database. Each step asserts on the file it produced instead.

## Fetching and baking, in containers

Two host-side helpers re-execute themselves inside images of their own, so a clone needs no
language toolchains and no elevated privileges.

The source fetcher's image carries the language toolchains at the versions this tree pins, passed
in as build arguments. That pinning is the point: a package manager newer than the one that will
compile the port can write a lock file the target's refuses. It runs as the calling user, because
every file it writes is committed — and its home and caches are pointed at `build/fetch-home`,
because that user has no account inside the image and the tools will not run without a writable
home.

The pack bake's image carries the container engine, the filesystem tool, Python and a compiler, and
runs privileged as root. That is what the filesystem tool needs to preserve container layer
attributes, and what the engine's store needs to write real deletion markers. Its store must be a
bind-mounted host directory at `build/podman`: the outer daemon's own root is an overlay, the
kernel refuses an overlay layer on an overlay, and the engine would otherwise fall back to a driver
that publishes no layer directory — which is exactly what the bake reads.

Both hand their results back to the calling user at the end. `build/podman` is excluded from that,
being the container store and root's by design.

## See also

- [Developing](developing.md) — the commands and the iteration loops
- [Packaging](../03-architecture/packaging.md) — what `kpkg` does inside a phase
- [Writing ports](writing-ports.md) — the recipes the package phases install
- [Build troubleshooting](build-troubleshooting.md) — reading a failed step
- [Testing](testing.md) — what the orchestrator's own tests prove
- [The C libraries](c-libraries.md) — `libkbuild` among the rest
