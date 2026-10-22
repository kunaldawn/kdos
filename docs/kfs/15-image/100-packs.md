# The pack store and the base pack

A [pack]{.idx} is a single file holding a read-only EROFS image and its metadata, which
[`kdos-packd`]{.idx} installs into the pack store under [`/var/lib/kdos/packs`]{.idx}, and a box can be composed
from packs. The medium carries no application packs: an application is built into container
images on the machine that asks for it, or imported as packs exported on another machine. `script/phases/70_image/100_packs.sh` therefore bakes nothing by default. It
creates the two directories the pack daemon needs, and with `KDOS_PACK_KDOS=1` it packs the root
itself as the base pack `kdos`, a clean KDOS to build and test ports in from inside a running one.

Unlike every other step of the phase, this one does not source [`phase.env`]{.idx}. It runs with only the
variables `script/chroot/exec.sh` passes, and the two it reads beyond those have defaults in the
script, noted below.

## The store directories

<!-- kfs:include script/phases/70_image/100_packs.sh lines=30-33 pin=a711dc612448 -->

`staging` is where an import is copied before the daemon verifies it, and `mnt` holds the daemon's
mounts, one `mnt/<id>` per pack, read-only, `nosuid` and `nodev`. Staging is mode 01777,
world-writable with the sticky bit, because it is the one place an unprivileged write may land, and
the only argument `kdos-packd`'s `install` accepts is a file name inside it. The daemon also sets
that mode when it starts; a first boot whose staging were 0755 would refuse an import until the
daemon had run once.

## Removing an old base pack

<!-- kfs:include script/phases/70_image/100_packs.sh lines=85-85 pin=678eec0856f6 -->

`build/` survives from build to build, and the flag does not. With the flag off, a base pack left
by an earlier run with it on is deleted, so `110_iso.sh` cannot find one that was not asked for.

## The base pack

`kdos-box create ports base=pack:kdos` makes a box whose root is this image's root, so a port can
be built and tested on a running KDOS without touching the machine itself. It is opt-in because it
is a second compression of the whole root on every build, for a base most machines never use. On
your machine:

```bash
make build KDOS_PACK_KDOS=1 BUILD_ARGS="--phases 70_image"
```

It runs only when the flag is set and both [`mkfs.erofs`]{.idx} (from [erofs-utils](#port-erofs-utils))
and [`kdos-pack`]{.idx} are installed; with either missing it is skipped without a word.

### The metadata

<!-- kfs:include script/phases/70_image/100_packs.sh lines=89-98 pin=de9a724350ba -->

`version` is `KDOS_VERSION` when the environment has one and the script's fallback otherwise.
`script/chroot/exec.sh` passes no `KDOS_VERSION`, so a build from `make build` always writes the
fallback.

### Testing the excludes

<!-- kfs:include script/phases/70_image/100_packs.sh lines=104-124 pin=e96fc5daf610 -->

`mkfs.erofs --exclude-path` matches an exact literal path, relative to the tree being packed, and
a path that matches nothing is not an error: `--exclude-path=/kdos`, with its leading slash, is
accepted and excludes nothing. Here that would pack the repository, bound at `/kdos` and
`/ports`, into the image, and fill the disk. So before it packs the root the step packs a small
fixture: two 2 MB directories excluded by path, one of them nested three levels down, and a 2 MB
file inside a directory excluded by the same kind of pattern the real run uses. An image that
honoured all three is a few kilobytes; one over 500,000 bytes, or none at all, stops the phase.

### Packing the root

<!-- kfs:include script/phases/70_image/100_packs.sh lines=147-155 pin=456e1d17e4bf -->

`-zzstd`
: Compress with zstd.

`-b 4096`
: A block size of 4096 bytes.

`-T "${SOURCE_DATE_EPOCH:-1735689600}" --all-time`
: Give every file the same timestamp instead of its real modification time, so the same tree
  gives the same image. The step does not source `phase.env` and `exec.sh` does not pass
  `SOURCE_DATE_EPOCH`, so the value is the script's fallback, which is the same value
  `script/env/common.env` sets.

`--force-uid=1000 --force-gid=1000`
: Record every file as owned by UID and GID 1000. A box runs with `--userns keep-id`, so the
  process inside it is UID 1000, not root. Every other pack is made by `kdos-pack build`, which
  forces the same owner. Packed with the real owners, the root would be root's from `/` down, and
  the box's user could create nothing in it; podman fails at the first file it needs to make. A
  pack is mounted `nosuid`, so the owner grants nothing either way.

`-U "$(kdos-pack uuid kdos)"`
: The filesystem UUID, derived from the pack id the same way `kdos-pack build` derives it.
  `mkfs.erofs` picks a random one by default, and an image that differs on every run gives a
  different `kdos-pack imagehash` every time.

`--exclude-regex='^(proc|sys|dev|tmp|run|mnt|media|var/cache|var/log)/'`
: Leave out everything inside these directories, but keep the directories. A container root
  without `/proc`, `/sys`, `/dev`, `/tmp` or `/run` to mount onto cannot start at all, and podman
  reports a missing mount point for a box that was created without complaint. Software in the box
  expects `var/cache` and `var/log` to exist as well.

`--exclude-path=kdos`, `ports`, `build`
: `/kdos` and `/ports` are the repository's bind mounts in the chroot. They are gigabytes in size,
  and the output itself is written under `/kdos/build`. `/build` is left out as well, as it is
  from `system.sfs`.

`--exclude-path=var/lib/kdos/packs`
: The root's own pack store, `staging` and `mnt` with whatever they hold, is left out of the
  pack.

`--exclude-path=home/kdos/.local/share/containers`
: podman's own store, which a clean tree to build ports in must not carry.

`/`
: The source tree, which in the chroot is `build/fs`. The excludes are written without a leading
  slash because they are matched relative to it.

<!-- kfs:include script/phases/70_image/100_packs.sh lines=165-167 pin=f1550f4c3c9e -->

`kdos-pack assemble` wraps the image made outside `kdos-pack` with the metadata into a `.kpack`,
and the bare image is deleted. The excludes are this step's own, which is why the image is made
here and not by `kdos-pack build`.

## Where the pack goes

The pack is written to `build/kdos-base/kdos.kpack`, outside the root. The pack store is inside the
root, so a pack written there would be squashed into `system.sfs`, and since this pack is itself a
compressed copy of the root, the medium would carry the root twice. With the same flag,
`110_iso.sh` copies it to `/packs/kdos.kpack` on the medium, beside `system.sfs`. `kdos-packd`
scans `/mnt/iso/packs`, where the booted medium appears, for `*.kpack`, so `base=pack:kdos` works
on the live system with nothing installed.

## What it leaves

On the image, `/var/lib/kdos/packs/staging` at mode 01777 and [`/var/lib/kdos/packs/mnt`]{.idx}, both
empty. With `KDOS_PACK_KDOS=1`, `build/kdos-base/` holds `meta` and `kdos.kpack`; without it,
`build/kdos-base/` does not exist.

## See also

- [Where a pack comes from](../../kdos/03-architecture/packs-and-boxes.md#where-a-pack-comes-from): the routes a pack takes onto a machine
- [Building the root-filesystem pack](../../kdos/03-architecture/packs-and-boxes.md#building-the-root-filesystem-pack): the rules the step keeps
- [Create with a box base](../../kdos/04-programs/kdos-appbox.md#create-with-a-box-base): `kdos-box create` and `base=`

<!-- book-nav -->
---

*Part XV. The image, chapter 355.* Previous: [354. The initramfs init](091-initramfs-init.md) · [Contents](../README.md) · Next: [356. The recipe tree and the source offer](105-ports.md)
