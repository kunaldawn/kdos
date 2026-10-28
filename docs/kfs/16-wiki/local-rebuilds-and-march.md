# Local rebuilds and kdos march

A KDOS machine carries its own recipes and the compilers that build them, so any package can be
built again on the machine itself. This page covers three sizes of that: rebuilding one port as
it ships, or from a recipe you changed; measuring whether building a port for this processor's
instruction-set level makes it faster, with `kdos march`; and running the whole KDOS build from a
developer medium with `kdos rebuild`. Updating to newer recipes is [Updating](updating.md), and
the commands of [`kpkg`]{.idx} itself are on [kpkg](kpkg.md).

## Installation

- [kdos-tools](#port-kdos-tools) installs `kdos march` and `kdos rebuild`, both subcommands of the
  `kdos` command.
- `kpkg`, [`kpkgbuild`]{.idx} and `kpkgdepends` are installed by the bootstrap; see
  [kpkg](../03-bootstrap/120-kpkg.md).
- A port builds with the compilers and tools the image carries. `kdos rebuild` refuses to start
  without `cc`, `make`, `bash`, `tar` and `xz`, and names the first of `mksquashfs`
  ([squashfs-tools](#port-squashfs-tools)), `xorriso` ([libisoburn](#port-libisoburn)) and
  `mkfs.fat` ([dosfstools](#port-dosfstools)) that is missing, because without all three no ISO
  comes out. The image carries all eight.
- `kdos rebuild` needs a medium built with `make build KDOS_ISO_SOURCES=1`, which carries the
  source tree under `/mnt/iso/sources`, or a checkout of the repository on the machine. The
  ordinary image carries neither: its medium holds only `EFI`, `boot`, `boot.catalog` and
  `system.sfs`, and `/ports` is recipes, not a tree `kdos rebuild` accepts.

None of these is a service.

## Configuration

### Where a port is found

`kpkg` looks for a port in the directories `PORT_REPO` names, in order, and the first that holds
the name wins. The default, from `/etc/kpkg.conf` (`src/system/kdos-kpkg/kpkg.conf` in the tree),
which a `PORT_REPO` in the environment replaces:

<!-- kfs:include src/system/kdos-kpkg/kpkg.conf lines=11-13 pin=9c67adf101ce -->

A directory of your own named first in `PORT_REPO` therefore shadows a port of `/ports`, without
touching `/ports`, which every `kdos update fetch` replaces whole.

### The build's flags

The tree's own build exports its compiler and linker flags from `script/env/chroot.env`, which
`/ports` does not carry. These are the C, C++, Fortran and link lines:

<!-- kfs:include script/env/chroot.env lines=69-73 pin=74e867687239 -->

On a running machine nothing sets them. A port rebuilt there gets the environment of the shell
that ran `kpkg`, and with no `CFLAGS` each build system uses its own defaults. The rest of
[`chroot.env`]{.idx} (the compiler names, `CMAKE_BUILD_TYPE`, the Go and Cargo settings, the locale) is
missing too, and so is `script/env/common.env`, which holds the settings that make a package the
same from one build to the next; see
[Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages).

### A benchmark line

`kdos march` measures only a port whose recipe has a `bench =` line, and runs its `bench_setup =`
line once per build, untimed, to make the fixtures. `lz4`'s, from
`ports/core/archiver/lz4/kpkgbuild`:

<!-- kfs:include ports/core/archiver/lz4/kpkgbuild lines=19-20 pin=e77f5c669a61 -->

The ports that carry one can be listed from `/ports`:

```bash
grep -l '^bench ' /ports/core/*/*/kpkgbuild
```

### The ledger and its variables

`kdos march run` appends its verdicts to [`/var/lib/kdos/march.ledger`]{.idx}. `KDOS_MARCH_RUNS` (timed runs
per build, default 5), `KDOS_MARCH_LEVEL` (the level to measure against) and `KDOS_MARCH_LEDGER`
(another ledger) change it; see [kdos march](../../kdos/04-programs/kdos-command.md#kdos-march).

### The developer medium

`KDOS_ISO_SOURCES=1` makes the image's last step copy the tree onto the medium beside the system
image, and write a stamp `kdos rebuild` prints before it starts:

<!-- kfs:include script/phases/70_image/110_iso.sh lines=195-218 pin=0b3f5e742938 -->

## Usage

### Rebuilding a port as it ships

1. Fetch its sources and rebuild it. `-f` forces the named port, not its dependencies, and
   `KPKG_FETCH=1` fetches the sources of any dependency that has to be built on the way:

   ```bash
   sudo kpkg fetch pv
   sudo env KPKG_FETCH=1 kpkg install -f pv
   ```

2. Check the installed files against the new package:

   ```bash
   kpkg check pv
   ```

### Rebuilding from a recipe you changed

This works for a port under `/ports/core`. KDOS's own programs under `/ports/src` reach the
libraries beside them by a relative path (`$PORT_SRC/../../libs`), so a copy of one taken out of
that tree does not find them; change those in a checkout of the repository instead.

1. Copy the port into a directory of your own. A repository is searched at `<name>/` and one shelf
   down, so a flat directory will do:

   ```bash
   mkdir -p ~/ports
   cp -r /ports/core/cli/pv ~/ports/
   ```

2. Edit `~/ports/pv/kpkgbuild` or `~/ports/pv/build.sh`. A new version needs a new `sha256 =`
   line, and `kpkg` refuses a source whose hash it does not have. The rules a recipe keeps are in
   [Writing ports](../../kdos/05-developer/writing-ports.md).

3. Build and install it with your directory first in `PORT_REPO`, and the shipped repositories
   after it so that its dependencies are still found:

   ```bash
   sudo env KPKG_FETCH=1 \
       PORT_REPO="$HOME/ports /ports/core /ports/src/system /ports/src/art /ports/src/desktop /ports/src/daemons" \
       kpkg install -f pv
   ```

   With `release = 2` in the copy, `kpkg info pv` then reports `Version: 1.12.0-2`, and the
   package file is deleted from `/var/cache/kpkg/packages` once it is installed.

4. To see whether a change alters the package, before or instead of installing it, keep the
   shipped recipe as the copy's `kpkgbuild`, put the changed one beside it as `kpkgbuild.new`
   (and `build.sh.new` when the script changes too), and run `kpkg verify`, which builds both and
   compares the two packages:

   ```bash
   cp ~/ports/pv/kpkgbuild ~/ports/pv/kpkgbuild.new       # the changed recipe
   cp /ports/core/cli/pv/kpkgbuild ~/ports/pv/kpkgbuild   # the shipped one
   sudo env PORT_REPO="$HOME/ports /ports/core" kpkg verify pv
   ```

   It prints `==> Building with kpkgbuild...`, then `==> Building with kpkgbuild.new...`, and
   ends with one verdict line. A change of `release` alone changes no file in the package, so
   that one ends `==> pv: the two packages are BYTE-IDENTICAL (<hash>...)`.

5. `kpkg verify --repro pv` builds the same recipe twice instead, and requires the two packages to
   be byte-identical; `pv` passes with the same line.

A package installed from your own recipe is replaced by the next `kdos update apply` whose tree
pins a newer version or release than the one installed. Until then `kdos update check` does not
count it as behind: with `pv` at `1.12.0-2` against a tree's `1.12.0-1`, it reports `0 behind`.

### Building with the tree's flags

To build a port with the same compiler flags the tree's build uses, pass the values from
[the excerpt above](#the-builds-flags) in the build's environment:

```bash
sudo env KPKG_FETCH=1 CFLAGS='<the CFLAGS value>' CXXFLAGS='<the CXXFLAGS value>' \
    LDFLAGS='<the LDFLAGS value>' kpkg install -f pv
```

The rest of `chroot.env` and all of `common.env` are still unset, so the package can still differ
from the one the image carries.

### Measuring a port with kdos march

`kdos march` asks whether building a port for this processor's instruction-set level
(`-march=x86-64-v2`, `-v3` or `-v4`) makes it faster here. It builds the port twice with
`kpkgbuild`, once with the default `CFLAGS` and once with the flag added, runs the recipe's
benchmark against both, and records the verdict `kept` only when the win is more than 3 per cent
plus the noise it measured on this machine.

1. See which levels this processor has:

   ```bash
   kdos march probe
   ```

   ```text
     x86-64-v2    yes
     x86-64-v3    yes
     x86-64-v4    missing avx512f missing avx512bw missing avx512dq missing avx512vl

   highest usable: x86-64-v3
   nothing is built with it until `kdos march run <port>` measures a win
   ```

   On a processor with only the baseline level there is nothing to measure, and `run` exits 1.

2. Fetch the port's sources. `/ports` holds none, and the `kpkgbuild` that `kdos march` runs
   downloads only when `KPKG_FETCH=1` is in its environment:

   ```bash
   sudo kpkg fetch lz4
   ```

3. Quieten the machine (close what is running), then measure:

   ```bash
   sudo kdos march run lz4
   ```

   Each build is timed `KDOS_MARCH_RUNS` times and the medians are compared; `sudo` clears the
   environment, so a different count is passed through it, as
   `sudo env KDOS_MARCH_RUNS=9 kdos march run lz4`. A verdict is one of `kept`, `reverted` (the
   win is inside the noise), `unmeasurable` (no `bench =` line, or no result) or `unbuildable`
   (the build with the flag failed). A baseline build that fails records nothing, and the run
   exits 1. Nothing is installed: both packages are written under a temporary directory,
   `/tmp/kdos-march.*` (under `$TMPDIR` when that is set), which the run removes, and the builds
   themselves use `/var/cache/kpkg/work/<name>`. The run ends with one line per port:

   ```text
   measuring against x86-64-v3, 3 runs per build

   lz4: building twice (baseline, then -march=x86-64-v3)...
   lz4: benchmarking 3 runs each...
   lz4                      x86-64-v3  baseline 0.380s  -march=x86-64-v3 0.403s  -5.9% (noise 9.0%) -> reverted
                              this machine's noise floor is 9.0% — close things down or raise KDOS_MARCH_RUNS
   ```

   That run, `sudo env KDOS_MARCH_RUNS=3 kdos march run lz4` on an eight-thread virtual machine,
   took 2 minutes 40 seconds.

4. Read the ledger:

   ```bash
   kdos march report
   ```

   It prints one line per verdict and a count of kept, reverted and unmeasurable ones, and exits 1
   with ``nothing measured yet — `kdos march run <port>` `` when nothing has been measured:

   ```text
   port                     level          baseline  optimised   change
   lz4                      x86-64-v3        0.380s     0.403s    -5.9%  reverted

   0 kept, 1 reverted, 0 unmeasurable
   ```

   The ledger itself holds one line per verdict: `lz4 x86-64-v3 0.3801 0.4026 -5.9 reverted`.

The ledger is a record, and only `kdos march report` reads it: a `kept` verdict changes nothing
about how `kpkg` or `kdos update apply` builds that port. To install the faster build, rebuild the
port with the level added to the flags it was measured with. `kdos march` takes `CFLAGS` from its
own environment, or `-O2 -pipe` when it is unset, as it is under `sudo`, and uses the same value
for `CXXFLAGS`:

```bash
sudo env KPKG_FETCH=1 CFLAGS='-O2 -pipe -march=x86-64-v3' CXXFLAGS='-O2 -pipe -march=x86-64-v3' \
    kpkg install -f lz4
```

A package built for a level runs only on a processor that has it, and dies with `SIGILL` on one
that does not, so a disk moved to an older machine takes those packages with it.

### Rebuilding KDOS from a developer medium

`kdos rebuild` runs the whole KDOS build on a booted machine, with no network, from a tree that
carries every source. No rebuild from a medium has been run through every phase; see
[Known gaps](#known-gaps).

1. On a build machine, in a checkout of the repository, make a developer medium:

   ```bash
   make fetch
   make build KDOS_ISO_SOURCES=1
   ```

2. Write `build/iso-build/kdos.iso` to a USB stick and boot it.

3. Mount a real disk with at least 25 GB free, not a tmpfs and not an overlay, for example at
   `/mnt/disk`.

4. Check first, then run. The build enters a chroot for most of its phases, which needs root, so
   `kdos rebuild` without root stops before it copies anything; only `--dry-run`, which changes
   nothing, runs as any user:

   ```bash
   sudo kdos rebuild --dry-run /mnt/disk/work
   sudo kdos rebuild /mnt/disk/work
   ```

`kdos rebuild` looks for the tree in `$KDOS_SOURCES` (passed through `sudo` as
`sudo env KDOS_SOURCES=<dir> kdos rebuild ...`), then `/mnt/iso/sources`, `/kdos` and the
current directory, and takes the first that has `script/kdosbuild.sh`, `ports/core`, `fs/etc` and
`src/devtools/kdosbuild`. It copies the tree to `<work-dir>/kdos` (a second run with the same work
directory reuses the copy), compiles the build orchestrator into `<work-dir>/kdosbuild`, and runs
it with `--fresh`. `--iso-only` runs only the image phase, `70_image`. The job count is
`KDOS_JOBS` when you set it, passed through `sudo env` in the same way; otherwise one job per
thread, but no more than one per 2 GiB of memory. See
[kdos rebuild](../../kdos/04-programs/kdos-command.md#kdos-rebuild).

### On the live medium and on an installed system

On the live medium `/var/cache/kpkg` is on the overlay in RAM, so a large build there compiles in
memory, and what it installs is gone at power-off without a
[persistence store](live-persistence.md). On a 4 GB machine `df -h /` shows that overlay as
2.0 GB, which every source, work tree and package of a build shares. `kdos rebuild` refuses a work
directory on the live root for that reason, and still refuses it with a persistence store, because
`/` is then an overlay on the store. On an installed system the same commands write to the disk and
stay.

## Tips

- A failed build leaves `/var/cache/kpkg/work/<name>/`, the unpacked source as the build left it.
  The next build of the port empties it.
- `kdos march run` takes several ports at once: `sudo kdos march run lz4 zip bzip2`.
- `kdos march decide <baseline> <optimised> <noise%>` applies the decision rule to three numbers
  and exits 0 for `kept`; it is there for testing the rule. `kdos march decide 100 90 1` prints
  `+10.0% kept`, and `kdos march decide 100 99 1` prints `+1.0% reverted`.
- `kdos rebuild` on the same work directory a second time reuses the copy, so an edit made in
  `<work-dir>/kdos` is what the next run builds.

## Troubleshooting

A port that builds in the tree fails on the machine
: The machine's build has none of the tree's environment: no `CFLAGS`, `CXXFLAGS` or `LDFLAGS`,
  and none of `script/env/common.env`. Build it with the flags, as in
  [Building with the tree's flags](#building-with-the-trees-flags), and compare.

`No sha256 for <file> in the recipe`, or `sha256 MISMATCH`
: A changed recipe names a source it gives the wrong hash for, or none. Put the file's
  `sha256sum` on the recipe's `sha256 =` line. See [kpkg](kpkg.md#troubleshooting).

`kdos: kpkgbuild is not on PATH`
: `kdos march` runs `kpkgbuild`, a link to `/usr/bin/kpkg`. Check that `/usr/bin/kpkgbuild`
  exists.

`kdos march run` reports ``unmeasurable — no `bench =` line in the recipe``
: Only a port with a `bench =` line can be measured. Add one to a copy of the recipe, as in
  [Rebuilding from a recipe you changed](#rebuilding-from-a-recipe-you-changed), and run
  `kdos march` with that copy first in `PORT_REPO`, passed through `sudo` as
  `sudo env PORT_REPO="$HOME/ports /ports/core" kdos march run <port>`.

Every verdict is `reverted`, and the output warns about the noise floor
: The machine's own variation is wider than the win. A floor above 5 per cent is reported with
  that advice; close what is running, or raise `KDOS_MARCH_RUNS` (3 to 31; an even count is
  raised to the next odd one, so the median is one of the runs).

`kdos rebuild` refuses the work directory
: It is on tmpfs, ramfs or an overlay, or has less than 25 GB free. A live medium's root is an
  overlay in RAM: mount a real disk and name a directory on it.

`kdos rebuild` finds no source tree
: It prints `kdos: no KDOS tree found.` and exits 1. The medium was not built with
  `KDOS_ISO_SOURCES=1`; point `KDOS_SOURCES` at a checkout of the repository. A `KDOS_SOURCES`
  that lacks any of the four marks is named first, as `kdos: KDOS_SOURCES=/ports is not a KDOS
  tree`, which is what `/ports` gives.

## Known gaps

- [An offline kdos rebuild from the medium has not been run to the end](../../kdos/06-reference/known-gaps.md#an-offline-kdos-rebuild-from-the-medium-has-not-been-run-to-the-end)
- [The kernel package differs between two builds of the same tree](../../kdos/06-reference/known-gaps.md#the-kernel-package-differs-between-two-builds-of-the-same-tree)
- [Build times and snapshot sizes are measured for part of the build only](../../kdos/06-reference/known-gaps.md#build-times-and-snapshot-sizes-are-measured-for-part-of-the-build-only)

## See also

- [kdos march](../../kdos/04-programs/kdos-command.md#kdos-march) and [kdos rebuild](../../kdos/04-programs/kdos-command.md#kdos-rebuild): every option
- [Tuning for this machine](../../kdos/02-user-guide/administration.md#tuning-for-this-machine) and [kdos rebuild](../../kdos/02-user-guide/administration.md#kdos-rebuild) in the user guide
- [`-march` measured per machine, not chosen for a population](../../kdos/01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population): why levels are measured per machine rather than chosen for the image
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md): the build `kdos rebuild` runs
- [Writing ports](../../kdos/05-developer/writing-ports.md): the rules a changed recipe keeps
- [kpkg](kpkg.md), [Updating](updating.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 406.* Previous: [405. Updating](updating.md) · [Contents](../README.md) · Next: [407. Packs](packs.md)
