# The image gates

Every package passed the package lint when it was built. Some faults are visible only in the
finished tree, where every package sits beside every other: a library a program loads from a
package it never declared, a script whose interpreter is not installed, a [desktop entry]{.idx} whose
program is missing, a link into a package that was swept. `script/phases/70_image/087_gates.sh`
runs three checks over the tree that is about to become the image, and stops the phase on their
findings, before the initramfs and the ISO are written; the first check stops it only for what it
finds under the documentation directories.

## The step

<!-- kfs:include script/phases/70_image/087_gates.sh lines=40-61 pin=dd4efaab0e4c -->

The checks are Python programs in `testing/`, which `script/chroot/exec.sh` binds read-only at
`/kdos/testing` in the chroot. They read the phase lists and recipes of the repository, so the
step changes into `/kdos` first. Each is given `--root /`, the root being built.

Every check runs before the step decides. A finding in the first does not hide the others, so one
failed build prints every problem the tree has, not just the first.

## The three checks

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

The exact rules of each, their exemptions and how to run them by hand are in
[Files nothing owns](../../kdos/05-developer/testing.md#files-nothing-owns),
[Run-time references](../../kdos/05-developer/testing.md#run-time-references) and
[Links, launchers and rules](../../kdos/05-developer/testing.md#links-launchers-and-rules).

## When it stops the phase

`unowned.py` prints a `FAIL unowned` line for each path it stops on, `linkcheck.py` a `FAIL` line
for each finding, and `entrycheck.py` a line naming the kind of finding (`link`, `exec`, `icon` or
`udev`) and a count of each at the end. All three are in the step's log under
`build/logs/70_image/`. The fix is usually in a recipe: a missing `depends`, a package that
installs a link to a file it does not ship, a desktop entry naming the wrong program, a build
that installs a manual outside its package. Rebuild
the port, then run the image phase again; nothing earlier needs to run:

```bash
make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"
```

The same three checks can be run on your machine over `build/fs` without a build, as
`make unowned`, `make linkcheck` and `make entrycheck`. `build/fs` belongs to root, so run them
from a container or as root; see
[The other harnesses](../../kdos/05-developer/testing.md#the-other-harnesses).

## Why it runs here

After the orphan sweep and every index, so it checks exactly what the image carries, and before
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

*Part XV. The image, chapter 352.* Previous: [351. The font cache](085-fontcache.md) · [Contents](../README.md) · Next: [353. The initramfs](090-initramfs.md)
