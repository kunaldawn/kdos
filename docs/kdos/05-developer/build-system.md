# The build system

How KDOS is built: the phases, the orchestrator that runs them, the chroot the later phases run
in, and the two mechanisms — snapshots and build plans — that keep an incremental change from
costing a full build.

For the commands, see [Developing](developing.md). This page is the machinery behind them.

## Phases

The build is a sequence of phase directories under `script/`, discovered by name and run in sorted
order.

| Directory | Title | Kind |
|---|---|---|
| `00_toolchain` | Cross Toolchain | Host |
| `01_phase1` | Base Userland | Host |
| `02_phase2` | Self-Hosting Bootstrap | **Chroot** |
| `03_phase3` | Toolchain & Core Libraries | **Chroot** |
| `04_phase4` | Userland & GUI Sliver | **Chroot** |
| `05_desktop` | Desktop | **Chroot** |
| `05_phase5` | Kernel | **Chroot** |
| `06_packaging` | Packaging | **Chroot** |

`05_desktop` sorts before `05_phase5` and that ordering is deliberate rather than an accident of
naming: the desktop is ordinary userland, and the kernel is the last thing built before packaging.

## How a phase runs

A phase is **either** a set of numbered shell scripts **or** a `packages.txt`, and the
orchestrator handles each differently:

| Form | What happens |
|---|---|
| `00_*.sh`, `01_*.sh`, … | Each script is run in order, its output logged to `build/logs/<phase>/` |
| `packages.txt` | Port names. `kpkgdepends` resolves the order, then each is installed |

Five phases carry a `packages.txt`; the rest are scripts. A phase may have both.

**Anything a chroot command prints is parsed.** The dependency resolver writes the install order to
standard output and **nothing else**, so diagnostics from inside the chroot go to a log file
instead. The orchestrator reads standard output only and **validates every token** against a strict
pattern, so noise fails loudly rather than being installed as a package.

## The phase environment

Each phase has a `script/<name>.env.sh` carrying its compiler settings and a metadata block:

```bash
# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* ..."
```

**These files are parsed, never sourced.** Several of them end by removing a build work directory,
which at source time would hit the **build container's** filesystem rather than the target's. So:

- Only `export NAME=VALUE` lines are read.
- Only five keys are honoured: `CHROOT`, and the four `KDOS_*` above.
- A value must be a **literal**. No expansion, and an unterminated quote reads as empty.

### `env -i` means every variable must be named

The chroot is entered with a cleared environment, so a variable a chroot step reads **must be
named on that command line**. Three are forwarded:

| Variable | Read by |
|---|---|
| `KDOS_REPLAY` | A step whose mark-file guard must stand down because the developer picked it |
| `KDOS_ISO_SOURCES` | Packaging, to put the sources on the medium |
| `KDOS_PACK_KDOS` | Packaging, to build this root filesystem as a base pack |

A knob passed into the container and not forwarded here reaches every **host** step and no chroot
one — and packaging is a chroot phase, so an opt-in packaging flag that is not forwarded produces
an ordinary image and says nothing about why. **A new opt-in packaging flag is two edits, not
one.**

The phase environments also set the compiler **by name** rather than letting configuration scripts
discover it, `KPKG_STRICT_RECIPE=1` so the build rebuilds a port whose recipe changed, and the five
lines that make packages reproducible. See [Packaging](../03-architecture/packaging.md).

## The chroot

Later phases run inside the target root filesystem. The entry script:

1. Requires root.
2. Bind-mounts `/dev`, `/proc`, `/sys`, `/tmp` and `/run`.
3. Bind-mounts the **repository** at `/kdos`, `build/` at `/kdos/build`, `ports/` at `/ports`, and
   `script/` and `src/` under `/kdos`.
4. Enters with `env -i`, a fixed search path, and the three forwarded variables.
5. Changes to `/kdos` so relative paths in scripts mean what they say.

The search path **appends** the local binary directory rather than prepending it: KDOS's own tools
install there, and leaving it out entirely makes a packaging step fail with a command not found —
but prepending would let a local binary shadow a system one during a port's configuration, which is
a different and much harder bug to see.

The repository is mounted read-only by the outer container except for `build/`, so a build cannot
modify its own sources.

## Snapshots

Every completed phase is archived to `build/snapshots/<phase>/`.

**What is snapshotted is declared by the phase**, in the metadata block above. Paths are relative
to `build/`, with exclusion patterns for the things that should not travel — work directories,
pseudo-filesystems, and the bind mounts.

**A declared path is either kept or reported as rejected — never quietly repaired.** Snapshot and
restore delete and re-extract those paths as root, so an absolute path, an empty one, a bare dot,
or anything containing a parent-directory component is **refused**. A name that merely begins with
dots is a real name and is allowed; only a whole parent component counts.

A phase with no declared paths is never snapshotted.

Each snapshot directory holds one compressed archive per declared path, plus a manifest and
timings.

### Restoring

```sh
make build                                       # opens a picker
make build BUILD_ARGS="--restore phase2"         # restore, continue at the next phase
make build BUILD_ARGS="--continue-from phase3"   # resume on the CURRENT tree, no restore
make snapshots                                   # list them
```

**Restore selection is layered and newest-wins.** Each declared path comes from the newest snapshot
at or below the target phase, so a phase that only declares part of the tree does not lose the rest.

**Restoring a phase whose earlier snapshots are missing is refused**, because the fallback would
build against a root filesystem that never had the target's packages.

**A manifest that does not parse, carries no entries, or names an archive that is not on disk reads
as absent, never as partial.** A half-read manifest that looks complete is what loses a tree.

An interrupted restore leaves a marker, and both snapshotting and the next build refuse to run
until it is resolved. **An empty marker is not a marker** — a previous implementation returned an
empty structure that every caller tested for truth.

Pressing the snapshot key during a build takes a **partial** snapshot; restoring one re-runs that
phase, which is safe because installed packages are skipped, and the picker marks it as partial.

Budget a few gigabytes per phase. The full clean removes snapshots; the build clean keeps them.

## Build plans

Snapshots answer "go back". A **plan** answers "re-run just this" — it restores nothing and
narrows what the next run executes on the tree you already have.

```sh
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild mesa,networkmanager"
make build BUILD_ARGS=--plan     # interactive; search the port list
```

Three things make that safe:

- **Snapshots are suppressed whenever a plan narrows execution.** Re-running an early phase on a
  later tree would otherwise file that tree under the earlier phase's name. A rebuild list alone
  does not narrow, so it keeps them.
- **Forcing is passed only for the ports the plan named.** Forcing resolves against an empty
  database and rebuilds what is named; dependencies keep the ordinary skip-if-installed behaviour.
  Passing it blanketly would rebuild every package in the tree.
- **Mark-file guards stand down only for a step you picked.** Many scripts begin by checking a
  marker so they do not repeat expensive work; the replay variable is exported for explicitly named
  steps and forwarded into the chroot.

A rebuild request that no selected phase ever reached is **reported**, rather than silently doing
nothing.

## kdosbuild

The orchestrator: `src/build/kdosbuild/`, a C program that links only the base and build
libraries, compiled on demand in a couple of seconds by the wrapper script.

| File | Owns |
|---|---|
| `main.c` | The command line |
| `manager.c` | The execution order and the step runner |
| `snapshot.c` | Writing and extracting archives |
| `stats.c` | Timing history, the estimate, the telemetry sampler |
| `tui.c` | The four screens |
| `view.c` | The parts of the screens that are **decisions** — layout, log classification — plus the preview fixture |
| `report.c` | The headless output, plain and structured, from one traversal |

**The build is the main loop.** There are no threads: pump the running child, pump the sampler,
draw, wait for a key with a deadline. Nothing has to be careful about drawing concurrently with a
caller, because nothing can.

### The screens

`↑↓` or `jk` select, `Space` folds a group, `F` toggles follow, `S` queues a partial snapshot, `Q`
stops — twice to force.

Five more are worth knowing because nothing else announces them:

| Key | Does |
|---|---|
| `/` | Search the selected step's log — **marks** rather than filters, because a build log is read for the context around a hit |
| `n` / `N` | Walk the marks |
| `E` | Jump to the first line classified as an error |
| `O` | Open the step's log in your pager, by argument vector, never a shell |
| `T` | Cycle the accents live |

Cycling accents needs **both** a palette reload and a full invalidation — a palette change alone
leaves every untouched cell wearing the old colours.

### Headless and structured output

A non-terminal output or a dumb terminal gets plain lines instead of the full-screen interface. A
build with its output redirected therefore works, and a build with no terminal at all can start —
which is also what makes the engine testable without a pseudo-terminal.

```sh
kdosbuild --json          # one object per event: build, phase, step, snapshot, notice, restore, result
kdosbuild --list --json   # the snapshot inventory
```

**Both are the same traversal.** The plain reporter and the structured one are two implementations
of one interface, and the runner does not know which it has — so the two views cannot disagree
about what ran.

Structured output is one object per line rather than one document, because a build can be killed at
any moment and the reason to have a machine-readable log is reading the tail of one that died.
There is deliberately **no total step count** in the opening event: a package phase expands only
when it is entered.

### Diagnostics with no build

```sh
kdosbuild --selftest                    # the layout and log-classifier assertions
kdosbuild --preview <screen> <WxH> <tier>
```

`--preview` draws one screen offscreen and dumps the cell buffer as plain text. The screens are the
build, activity, startup, plan and package views; the tiers are the three glyph tiers.

**It is the only way to see a layout without a two-hour build and a terminal**, and it exists
because several geometry defects in this interface were found by hand arithmetic and none of them
was visible to the compiler.

It also forced each screen's drawing half out of its event loop into a single draw function — the
loop calls that and nothing else, because a second drawing path would be a second thing to keep in
agreement with the one people look at.

**The preview fixture is chosen to break layouts rather than to look plausible**: a multi-terabyte
total, a nine-digit file count, an hour-scale estimate, a port name longer than any pane. The one
thing not reproducible between runs is the spinner, which is picked from the clock.

**Read the low-tier output specifically** for glyphs the console font lacks.

## libkbuild

The **deciding** half of the orchestrator: it reads and it chooses, while creating archives and
running phases is the orchestrator's.

| File | Owns |
|---|---|
| `kb_phase.c` | Phase discovery and the metadata block |
| `kb_plan.c` | Plan narrowing, and the port discovery behind the picker |
| `kb_snap.c` | The snapshot inventory, layered restore, the interrupt marker, and mount detection |
| `kb_json.c` | A read-only structured-data scanner for exactly those files |

**The scanner refuses anything that does not parse whole** — truncated, trailing junk, a trailing
comma — because every caller treats a parse failure as *absent*, and a lenient parser turns a
corrupt manifest into a confident wrong answer.

Two bugs the extraction surfaced, both silent:

- **Reading a file stopped at its reported size.** Files under the process filesystem report a size
  of zero, so the mount list read back as empty and the "is anything mounted here" question
  answered *no* — which is the answer that lets a snapshot run over a live bind mount. It reads to
  real end-of-file now.
- **An empty marker was treated as a marker**, so an empty structure meant an interrupted restore
  forever.

## Syncing `fs/`

`fs/` is copied verbatim into the target root filesystem. **A file deleted from `fs/` must
disappear from the tree**, and a plain recursive copy overwrites but never removes — so a dropped
path lingers forever, and the build tree is incremental.

So the sync **records every path `fs/` provided** in a manifest, and on the next sync deletes any
path present in the old manifest that `fs/` does not provide this time.

Two traps in that mechanism:

- **The manifest is written after the copy**, so a package that later owns the same path is not the
  manifest's to remove.
- **It must not use a listing option that only the full-featured tool has.** The build image's
  compact one rejects it, which wrote an **empty** manifest that silently protected nothing.

**The account files are merged, not overwritten.** Package install hooks add service users long
after the early phase runs, and a plain copy on a re-sync would delete them. Repository entries win;
runtime-added entries are appended back.

The user-directory step needs the same treatment for the trees it copies into the home directory,
and clears them before copying.

## Sweeping orphaned packages

**A port deleted from the tree leaves its package installed forever**, unless something removes it.
`fs/` has had a manifest guard for a while; packages needed one too.

A packaging step removes every installed package with **no recipe in any port repository**, and
`testing/preflight.sh` reports the same thing in seconds instead of at the end of a two-hour build.

Neither is fatal on failure: an orphan with a damaged manifest must not stop the ISO from being
rolled.

## Fetching and baking, in containers

Two host-side helpers re-execute themselves inside images of their own, so a clone needs no
language toolchains and no elevated privileges:

**The source fetcher's image carries the language toolchains at the versions this tree pins**,
passed in as build arguments. That pinning is the point: a package manager newer than the one that
will compile the port can write a lock file the target's refuses. It runs as the **calling user**,
because every file it writes is committed — and its home and caches are pointed at a build
directory, because that user has no account inside the image and the tools will not run without a
writable home.

**The pack bake's image carries the container engine, the filesystem tool, Python and a compiler**,
and runs privileged as root — which is what the filesystem tool needs to preserve container layer
attributes, and what the engine's store needs to write real deletion markers. **Its store must be a
bind-mounted host directory**: the outer daemon's own root is an overlay, the kernel refuses an
overlay layer on an overlay, and the engine would fall back to a driver that publishes no layer
directory — which is exactly what the bake reads.

Both hand their results back to the calling user at the end. `build/` is excluded from that, being
the container store and root's by design.

## See also

- [Developing](developing.md) — the commands and the iteration loops
- [Packaging](../03-architecture/packaging.md) — what `kpkg` does inside a phase
- [Writing ports](writing-ports.md) — the recipes the package phases install
- [Testing](testing.md) — what the orchestrator's own tests prove
- [The C libraries](c-libraries.md) — `libkbuild` among the rest
