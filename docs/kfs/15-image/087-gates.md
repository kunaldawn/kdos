# The image gates

Every package passed the package lint when it was built. Some faults are visible only in the
finished tree, where every package sits beside every other: a library a program loads from a
package it never declared, a script whose interpreter is not installed, a [desktop entry]{.idx} whose
program is missing, a link into a package that was swept, a QML module the base left out.
`script/phases/70_image/087_gates.sh` runs four checks over the tree that is about to become the
image, and stops the phase on their findings, before the initramfs and the ISO are written; the
first check stops it only for what it finds under the four directories that hold manuals,
documentation and translations.

## The step

<!-- kfs:include script/phases/70_image/087_gates.sh lines=50-79 pin=bf626b89516f -->

The first three checks are Python programs in `testing/`, which `script/chroot/exec.sh` binds
read-only at `/kdos/testing` in the chroot; the fourth is `script/lib/imageset.py`, the script
that chose the base. They read the phase lists and recipes of the repository, so the
step changes into `/kdos` first. Each is given `--root /`, the root being built.

Every check runs before the step decides. A finding in the first does not hide the others, so one
failed build prints every problem the tree has, not just the first.

## The four checks

`testing/unowned.py`
: Lists every file and symlink that no package's [manifest]{.idx} and no path of `fs/` provides. Such a
  file is removed by no package removal and replaced by no update: it was written outside a
  package's staging tree, by a tool run against the live root, or lost from a manifest. Mount
  points, scratch, homes, kpkg's state, kpkg and kinstall themselves, and what is generated over
  every package (the shared indexes, the launcher table, the source offer, the initramfs and its
  module list) are subtracted. The report covers the whole tree, and a finding stops the phase
  only under `usr/share/locale`, `usr/share/man`, `usr/share/doc` or `usr/share/info`, the four
  `--fail-under` directories, where it prints a `FAIL unowned` line for each path.
  [020_cleanup.sh](020-cleanup.md) has already removed every unowned file there that kpkg's
  [size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy)
  prunes from a package, so what is left is a manual or catalogue that would ship and that no
  removal or update would ever touch. Any other unowned file is reported and passes.

`testing/linkcheck.py`
: Reads every file the package manifests name, and fails on three kinds of finding. An ELF
  file's `DT_NEEDED`, resolved the way musl's loader resolves it, lands in a package outside the
  consumer's transitive `depends`: a dependency only the build order supplied, which a removal, a
  parallel build or an update can take away. A script's `#!` interpreter, or an ELF file's
  interpreter, does not resolve in the root. Or one process would map both `libcrypto.so.3` and
  `libcrypto.so.4`, whose symbols musl would bind to whichever was loaded first. It also prints
  warnings that do not fail.

`testing/entrycheck.py`
: Fails on a symlink that resolves to nothing; a desktop entry's or D-Bus service's `Exec=` whose
  program is not in the root; a visible desktop entry whose `Icon=` no icon theme provides; and a
  udev rule's `RUN`, `PROGRAM` or `IMPORT{program}` whose program does not exist. Each of these
  does nothing at run time without saying why: the menu shows a launcher that starts nothing, the
  bus activates nothing, the menu draws a blank, and udev logs the failure at debug level.

`script/lib/imageset.py --verify`
: Runs under `KDOS_IMAGE=base` only. Reads the run-time needs each kept package's install
  recorded in `/var/lib/kpkg/db/.needs/<name>` (its sonames, `#!` interpreters, QML and Python
  imports and its recipe's `runtime =` words; see
  [Run-time needs](../../kdos/03-architecture/packaging.md#run-time-needs)) and resolves each in
  the stage. An edge nothing in the stage provides is looked up in `--full /kdos/build/fs`, the
  tree the stage was trimmed from, and one a package there provides prints
  `FAIL <package>: <edge>: only <provider> provides it, and the image drops it`. A package with no
  record (installed before records were kept) is counted as unmeasured, and an edge nothing in
  either tree provides is counted; neither fails. Under `KDOS_IMAGE=full` nothing is dropped and
  the step says so.

Under `KDOS_IMAGE=base` the tree is the [image stage](017-stage.md), trimmed to the base, so the
last three checks are also the proof that the base's closure is whole: a library, an interpreter,
a program, a QML module or a Python package a base package needs that went with the packages
`017_stage.sh` removed is a finding, and the fix is a `depends =` or `runtime =` the recipe lacks
or a name to add to `ports/base`. Only the fourth sees a QML or a Python import, and only for a
package installed since its needs were recorded.

The exact rules of each, their exemptions and how to run them by hand are in
[Files nothing owns](../../kdos/05-developer/testing.md#files-nothing-owns),
[Run-time references](../../kdos/05-developer/testing.md#run-time-references) and
[Links, launchers and rules](../../kdos/05-developer/testing.md#links-launchers-and-rules).

## When it stops the phase

`unowned.py` prints a `FAIL unowned` line for each path it stops on, `linkcheck.py` a `FAIL` line
for each finding, and `entrycheck.py` a line naming the kind of finding (`link`, `exec`, `icon` or
`udev`) and a count of each at the end, and `imageset.py` a `FAIL` line for each edge met only
outside the base and an `imageset: verify:` count line. All four are in the step's log,
`build/logs/70_image/<NNNN>_gates.sh.log` (see
[Step logs](../../kdos/05-developer/build-system.md#step-logs)). The fix is usually in a recipe: a missing `depends`, a package that
installs a link to a file it does not ship, a desktop entry naming the wrong program, a build
that installs a manual outside its package. Rebuild
the port, then run the image phase again; nothing earlier needs to run:

```bash
make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"
```

The first three checks can be run on your machine over `build/fs` without a build, as
`make unowned`, `make linkcheck` and `make entrycheck`, and the fourth as
`python3 script/lib/imageset.py --root build/fs --verify`, which fails a kept package whose edge
only a dropped package of `build/fs` provides; for packages with no record, measure them first
with `kpkg needs --scan` and pass the directory with `--needs`
([Run-time needs](../../kdos/03-architecture/packaging.md#run-time-needs)). `build/fs` holds every package, so under
`KDOS_IMAGE=base` they check the whole tree and not the base; only this step checks the stage,
which exists only while a step runs. `build/fs` belongs to root, so run them from a container or
as root; see
[The other harnesses](../../kdos/05-developer/testing.md#the-other-harnesses).

## Why it runs here

After the trim, the orphan sweep and every index, so it checks exactly what the image carries, and before
`090_initramfs.sh` and `110_iso.sh`, so a tree that fails is never written into an image. The files
those later steps write into the root, the initramfs and its module list and the source offer
from `105_ports.sh`, are generated, and `unowned.py` subtracts them.

## What it leaves

Nothing. The step only reads the tree.

## See also

- [The other harnesses](../../kdos/05-developer/testing.md#the-other-harnesses): every tree check and what it proves
- [The package lint](../../kdos/03-architecture/packaging.md#the-package-lint): the checks made on each package as it is built
- [A build that failed](../../kdos/05-developer/build-troubleshooting.md): reading a failed step

<!-- book-nav -->
---

*Part XV. The image, chapter 377.* Previous: [376. The software catalogue](086-swcatalog.md) · [Contents](../README.md) · Next: [378. The image ledger](088-ledger.md)
