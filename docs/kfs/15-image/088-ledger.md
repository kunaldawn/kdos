# The image ledger

A release ISO is one release asset of at most 1,900 MiB, and [110_iso.sh](110-iso.md#the-size-gate)
fails a base image that passes it. A build that stops there has to say what the image spends its
bytes on, and why each of those packages is on it at all. `script/phases/70_image/088_ledger.sh`
writes that account, the [image ledger]{.idx}: every package the finished tree holds, its size, and
the dependency path from a `ports/base` seed that put it there. This page shows the step, the
format of the ledger, and how to ask the same question of one port by hand.

**Runs in:** the stage, `build/image-root`, under `KDOS_IMAGE=base`; `build/fs` under
`KDOS_IMAGE=full`\
**Reads:** `ports/base`, `ports/buildonly` and the recipes, from `/kdos`; the root's package
database and every file its manifests name\
**Writes:** `build/image-ledger`, outside the root

## The step

<!-- kfs:include script/phases/70_image/088_ledger.sh lines=31-39 pin=247d758288ea -->

The step runs after [087_gates.sh](087-gates.md), so it measures the tree the trim, the sweeps, the
indexes and the gates left, which is the tree `110_iso.sh` squashes. It writes the ledger whole on
every run, through a temporary file renamed over the old one, and prints its first twenty rows and
its total into the step's log.

## The ledger

[`script/lib/imageset.py`]{.idx} `--ledger` reads every package in the root's database and every
file its [manifest]{.idx} names, and prints one row per package, largest first. The base of a
full build tree begins and ends:

```text
# bytes          MiB  package                         why
  2092154343    1995.2  clang                           firefox-esr > clang
   596766065     569.1  linux-firmware                  linux-firmware
   339313897     323.6  gcc                             gcc
   309126794     294.8  plasma-workspace-wallpapers     kde-plasma > plasma-workspace-wallpapers
   ...
total 9535467742 bytes, 9093.7 MiB, in 1004 packages
```

The bytes are the files' sizes, each inode counted once: a file two packages share by hard link is
counted for the first of them in name order, so the total is the bytes of package files the image
compresses. Files no package owns, the `fs/` tree and the caches the steps generate, are not in
it; [087_gates.sh](087-gates.md) reports those.

The last column is the shortest dependency path from a seed to the package, the same edges the
[image stage](017-stage.md#the-removal) closed over: each recipe's first `depends =` line, not
followed into a port `ports/buildonly` lists unless `ports/base` names it. A package that is
itself a seed has a path of one name. A row whose path is `-` is a package outside the base, which
only a `KDOS_IMAGE=full` image carries.

The path is a reason, not the only one: a port two seeds reach is listed under the shorter path. To
take a port off the medium, every path to it has to go, and `--why` after an edit shows the next
one.

## One port by hand

`--why` answers the question for one port, with no package database, so it runs on your machine:

```bash
python3 script/lib/imageset.py --why kio-extras
```

```text
plasma-workspace > kio-extras
```

It exits 1 and prints `<port>: not in the base` for a port no path reaches. `--ledger --root
build/fs` prints a ledger of the whole build tree, where every package outside the base has `-`;
`build/fs` belongs to root, so a file whose mode keeps you out is skipped, and the run needs root
for an exact total.

## See also

- [The image stage](017-stage.md): the closure the paths are taken from
- [The ISO](110-iso.md#the-size-gate): the release size gate that prints the ledger
- [The base image](../../kdos/03-architecture/packaging.md#the-base-image): the three files the set is made from
- [The image ledger and the size gate](../../kdos/05-developer/build-system.md#the-image-ledger-and-the-size-gate): the step in the build system

<!-- book-nav -->
---

*Part XV. The image, chapter 378.* Previous: [377. The image gates](087-gates.md) · [Contents](../README.md) · Next: [379. The initramfs](090-initramfs.md)
