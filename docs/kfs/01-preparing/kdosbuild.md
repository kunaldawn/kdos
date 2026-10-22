# kdosbuild

`kdosbuild` is the program that runs the build: it finds the phases, turns each into steps, runs the
steps in order inside the container or the [chroot]{.idx}, logs every one, and archives each phase as a
[snapshot]{.idx} when asked to. This chapter shows how `make build` compiles and starts it, how it finds
and runs the phases, and the commands that start a whole build, resume one, or narrow it to a
single phase, step or port.

## Compiling the orchestrator

kdosbuild is not shipped as a binary. Inside the container, `make build` runs
`script/kdosbuild.sh`, which compiles it from its source and the KDOS libraries it links, and then
runs it:

<!-- kfs:include script/kdosbuild.sh lines=22-36 pin=93570f0489ba -->

`OUT` is where the program goes, `build/.kdosbuild` unless `KDOSBUILD_BIN` says otherwise. `LIBS`
are the five libraries it is built from, each compiled in with its sources rather than linked as a
library, so the program needs nothing but the C library to run. `libkpkg` is among them because
kdosbuild asks kpkg's own code, on the host side, whether a port is installed and current.

<!-- kfs:include script/kdosbuild.sh lines=70-90 pin=5d7f28d44218 -->

The hash in `SUM` covers the compiler's name, the flags, the compiler's target and version, and every
`.c` and `.h` file of the program and its libraries. The program is compiled again only when that
hash differs from the one in `build/.kdosbuild.sum`, and the sum is written only after a compile
succeeds. An unchanged orchestrator therefore starts at once, and a binary compiled by a different
compiler, such as one built on your machine against glibc, is replaced rather than run. The last
line starts it with `--script-dir script` and every argument you gave in `BUILD_ARGS`.

Before that, the script sets a trap that runs when it exits, whether the build succeeded or not,
and gives every entry of `build/` except `build/fs` back to the user and group `make build` passed
in as `HOST_UID` and `HOST_GID`; [The build tree](build-tree.md#fs-the-root-being-built) says why
`build/fs` stays root's.

## Phases

kdosbuild lists `script/phases/` and keeps every directory whose name is a number followed by an
underscore, in sorted order. From the top of the repository on your machine:

```bash
ls script/phases
```

```text
00_cross  10_bootstrap  20_selfhost  30_foundation  31_compilers  40_lang  41_system
42_graphics  43_toolkits  44_apps  50_desktop  60_kernel  70_image
```

The part of a name after the number is the phase's short name, so `41_system` can also be named
`system` wherever a phase is asked for. The numbers come in bands of ten with gaps, so that a new
phase can be put between two others without renaming anything.

Each phase has a `phase.env` in its directory. kdosbuild reads five settings from it as text, never
running it: the phase's title and one-line description, whether it runs in the chroot
(`CHROOT=1`), and which paths under `build/` its snapshot archives and which it leaves out. The
first two phases run in the container, every later one in the chroot. The rest of the file is shell
that the phase's own steps source, and it sets the compilers, flags and search paths they build
with; see [The phase environment](../../kdos/05-developer/build-system.md#the-phase-environment).

A phase holds one of two things, and that decides how it runs:

**A script phase** holds numbered shell scripts, run one after the other in sorted order as steps.
`00_cross`, `10_bootstrap` and `70_image` are script phases. Each step is
`bash script/phases/<phase>/<script>`, or the same through `script/chroot/exec.sh` in a chroot
phase.

**A package phase** holds a list of ports, either one [`packages.txt`]{.idx} or a directory `packages.d/`
of lists, one per shelf. kdosbuild hands the whole list to `kpkgdepends` inside the chroot, which
prints the install order of every named port together with everything it depends on, and makes
each port in that order a step:

```bash
script/chroot/exec.sh bash -c 'source script/phases/41_system/phase.env && kpkg install pv'
```

Before it runs any of them, kdosbuild asks of each port, on the host, the question kpkg would ask:
is it installed, and does its recipe still hash to the value recorded when it was? A port that is
installed and current never becomes a step; it stays in the phase's list on screen, dimmed, marked
`installed`. This is what makes a second run over a built tree take minutes rather than a day. The
rule is in
[How a phase runs](../../kdos/05-developer/build-system.md#how-a-phase-runs), and what a single
`kpkg install` then does is the subject of
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md).

## Running a build

From the top of the repository on your machine:

```bash
make build
```

On a terminal, kdosbuild first opens the **startup picker**. Its first row is *start fresh*: run
every phase, in order, on the tree as it stands. Below it is one row for each phase that has a
snapshot, showing when it was taken, its size, the commit it came from, and how long the phase
took. Choosing a snapshot row restores it and continues from the phase after it. Its keys, among
them the one that makes this run write snapshots of its own, are listed in
[The startup picker](../../kdos/05-developer/build-system.md#the-startup-picker).

The **build screen** then shows the phases and their steps on the left, the selected step's log on
the right, and graphs of processor, memory and output above the keys.
[The build screen](../../kdos/05-developer/build-system.md#the-build-screen) explains each part.

Without a terminal there is no picker and no screen: kdosbuild prints one plain line per event, so
a build can be sent to a file. Name the starting point on the command line, because nothing can
ask:

```bash
make build BUILD_ARGS=--fresh > build.log 2>&1
```

`--fresh` skips the picker and runs every phase on the existing tree. It does not empty the tree:
each early step whose marker exists exits at once, and each port already installed from the same
recipe is skipped. [Starting from nothing](build-tree.md#starting-from-nothing) shows how to empty
it first.

Run a long build where it will outlive your shell, in `tmux`, `screen` or another terminal
multiplexer. A build started from a shell that exits dies with it.

## Resuming and restoring

A build stopped part-way, by a failure or by you, is resumed on the tree it left behind with
`--continue-from`, naming the phase it stopped in:

```bash
make build BUILD_ARGS="--continue-from 41_system"
```

Every phase before the one named is marked skipped and not run; the named phase and every phase
after it run on the existing tree. Within the named phase, every port already installed and current
is passed over, so the work starts where it stopped.

Name the phase the build stopped in, not an earlier one. Re-running an early phase on a tree that
has gone past it is harmless while no snapshots are written, but with snapshot writing on, that
phase's snapshot would be replaced by an archive of the later tree, filed under the earlier name.

To go back to a snapshot instead, discarding what was built after it:

```bash
make build BUILD_ARGS="--restore 20_selfhost"
```

The tree's declared paths are replaced with the snapshot's contents and the build continues from
the phase after it. `--restore` and `--continue-from` take a phase by its directory name, its short
name, its position counted from 1, or `latest`, the last phase that has a snapshot. How a restore
puts a tree back together from layered archives is in
[How a restore works](../../kdos/05-developer/build-system.md#how-a-restore-works).

## Writing and listing snapshots

A run writes snapshots only when asked to, in the picker or on the command line:

```bash
make build BUILD_ARGS="--fresh --snapshot"
```

Each phase from `00_cross` to `50_desktop` is then archived into `build/snapshots/<phase>/` as it
completes. `60_kernel` and `70_image` take none; they are re-run on whatever tree they find. Check
the free disk space before a full build with snapshots on; the first snapshot holds the whole tree,
compressed, and each later one what that phase added or changed.

To list the snapshots that exist, on your machine:

```bash
make snapshots
```

This compiles and runs kdosbuild directly on your machine with `--list`, so it needs a C compiler
there. `make build BUILD_ARGS="--delete 20_selfhost"` deletes one; a snapshot that later ones are
layered on is kept aside until the last of them is gone.

## Narrowing a build

A **build plan** runs only part of the build, on the tree already in place, restoring nothing. It is
the everyday way to rebuild what you changed:

```bash
make build BUILD_ARGS="--phases 41_system,70_image --rebuild pv"
```

`--phases LIST`
:   Run only these phases.

`--steps LIST`
:   Run only these scripts of a script phase, each written `PHASE:script.sh`.

`--rebuild LIST`
:   Build these ports again even though they are installed and current.

`--plan`
:   Pick the phases, scripts and ports by hand in a full-screen editor.

How each is spelled, what it selects and how a narrowed run treats snapshots are in
[Build plans](../../kdos/05-developer/build-system.md#build-plans) and the
[flags table](../../kdos/05-developer/build-system.md#flags). These are the plans for the common
cases:

| You changed | Run |
|---|---|
| a file under `fs/` | `--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh` |
| one port | `--phases <phase>,70_image --rebuild <port>`, where `<phase>` is the one whose list names the port |
| one program of the desktop | `--phases 50_desktop --rebuild <port>` |
| only the image | `--phases 70_image` |

To find the phase whose list names a port, from the top of the repository:

```bash
grep -rlx pv script/phases/*/packages*
```

```text
script/phases/41_system/packages.d/cli.txt
```

`--port-jobs N` builds up to `N` ports of a package phase at once, from 1 to 8. It groups the ports
by dependency level, builds each level's ports side by side, and then installs the whole level in
order before the next level starts; see
[Building a package phase by level](../../kdos/05-developer/build-system.md#building-a-package-phase-by-level).

## When a step fails

A step that exits with a non-zero status stops the build, and the screen opens a failure panel on
it. The step's full log is at `build/logs/<phase>/<NNNN>_<name>.log`. With the fix made, the
failure panel can run the failed phase again in the same run; see
[Retrying a failed phase](../../kdos/05-developer/build-system.md#retrying-a-failed-phase). From the
command line, and without a terminal, the same is `--continue-from` with the failed phase. What
kdosbuild's exit status means is under its [flags](../../kdos/05-developer/build-system.md#flags),
and the reading of a failed log, case by case, in
[Build troubleshooting](../../kdos/05-developer/build-troubleshooting.md).

## Step timings

kdosbuild writes the time of every step that did its work to `build/snapshots/timings.json`,
keeping the figures of every step a run did not measure, and files each figure under the machine
that measured it: the processor's model name, the CPUs kdosbuild may run on, `MemTotal` and
`KDOS_JOBS`, read inside the container when the run starts. The book's build times are these figures
divided by the time of `00_cross/binutils`; see
[Typographical conventions](../00-front/conventions.md#build-times-and-the-sbu), and for what
counts as measured, [Step timings](../../kdos/05-developer/build-system.md#step-timings).

## See also

- [kdosbuild](../../kdos/05-developer/build-system.md#kdosbuild): every flag, the exit statuses and the program's layout
- [Snapshots](../../kdos/05-developer/build-system.md#snapshots) and [Build plans](../../kdos/05-developer/build-system.md#build-plans): the two ways to run less than everything
- [Rebuilding one thing](../../kdos/05-developer/developing.md#rebuilding-one-thing): the narrow loops for each kind of change
- [Part II](../02-cross/intro.md): the first phase kdosbuild runs

<!-- book-nav -->
---

*Part I. Preparing, chapter 5.* Previous: [4. The build tree](build-tree.md) · [Contents](../README.md) · Next: [Part II. The cross toolchain](../02-cross/intro.md)
