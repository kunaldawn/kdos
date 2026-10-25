# The image

<!-- kfs:phase 70_image -->
<!-- kfs:order 010-binhost 015-pkgstore 020-cleanup 030-launchers 040-orphans 045-identity 050-theme 060-udev-hwdb 070-user 080-whatis 085-fontcache 087-gates 090-initramfs 091-initramfs-init 100-packs 105-ports 110-iso -->

The last phase, `70_image`, makes no packages. It takes the root the package phases built in
`build/fs`, trims what only the build needed, writes the launchers, identity, theme, homes and
indexes the shipped system expects, checks the result, and then builds the initramfs, the packs,
the recipe tree and the ISO. Each of its step scripts has a chapter here, in the order the phase
runs them. This page covers what every step has in common: where it runs, the environment it
gets, why the phase keeps no snapshot, and the order the steps depend on.

## Where the steps run

`70_image` is a script phase, like [Part III](../03-bootstrap/intro.md), and a chroot phase, like
every package phase. kdosbuild runs each script in sorted order, in the container, as

```bash
script/chroot/exec.sh bash script/phases/70_image/<step>.sh
```

so the step itself runs in the chroot, as root, in `/kdos`, with `build/fs` as `/` (see
[Where a command runs](../00-front/conventions.md#where-a-command-runs)). A path such as
[`/etc/skel`]{.idx} on these pages is therefore `build/fs/etc/skel` on your machine. The steps that write
outside the root write under `/kdos/build`, which is `build/` on your machine.

No step of this phase has a mark. Each runs in full every time the phase runs, so a step that
writes a file first removes the copy an earlier run wrote: the stamp in [`/etc/os-release`]{.idx}, the
hardware database, the manual index, the font cache, the staged recipe tree and the base pack are
each made from nothing rather than updated.

## No snapshot

<!-- kfs:include script/phases/70_image/phase.env lines=15-23 pin=d9a7e476a26d -->

`KDOS_SNAPSHOT_PATHS` is empty, so kdosbuild archives nothing when this phase ends, and there is
no `70_image` restore point to pick. The phase is re-run on the tree it finds. Two things follow.

The phase changes `build/fs` itself, not a copy of it. The cleanup deletes the build's caches, the
orphan sweep uninstalls packages, and the homes are written from `/etc/skel`. A later
`--continue-from` or a port rebuild carries on from that tree, not from the one the package
phases left.

Running only this phase makes a new image from the tree as it stands, which is all a change to
packaging alone needs. On your machine:

```bash
make build BUILD_ARGS="--phases 70_image"
```

After one port's rebuild it is `--phases <its phase>,70_image --rebuild <port>`, as
[Narrowing a build](../01-preparing/kdosbuild.md#narrowing-a-build) shows.

## The environment

The rest of [`phase.env`]{.idx} is short:

<!-- kfs:include script/phases/70_image/phase.env lines=25-38 pin=fbccbb28caec -->

[KDOS_ACCENT]{.idx} names the [accent]{.idx} the medium ships in. `050_theme.sh` seeds that accent's theme
into `/etc/skel`, and `110_iso.sh` draws the boot menu in it. The same default is compiled into
libkcolor as `KCOL_DEFAULT_ID`, which every program of KDOS's own falls back to when no accent is
configured. A shell script cannot read a C macro, so the name is written twice, once here and once
in `src/libs/libkcolor/kcolor.h`, and [050_theme.sh](050-theme.md#the-accent-check) stops the build
when the two differ. To change the default accent, change both. The accents are listed in
[Theming](../../kdos/02-user-guide/theming.md#the-eight-accents).

[`chroot.env`]{.idx}, and the `common.env` it sources, supply what every chroot phase has, among it
`SOURCE_DATE_EPOCH`, which `085_fontcache.sh` clamps the font directories to, and `KPKG_SKIP_INDEX=man`, which is why the
manual index is written by `080_whatis.sh` and not by each install. The phase builds no port, so
it drops the pkg-config search path and the answered C++ configure probe that `chroot.env` sets
for the package phases.

The chroot is entered with a cleared environment, so a `make build` variable reaches a step only
when `script/chroot/exec.sh` names it (see
[env -i means every variable must be named](../../kdos/05-developer/build-system.md#env--i-means-every-variable-must-be-named)). The
ones this phase reads:

| Variable | Read by | Effect |
|:--|:--|:--|
| `KDOS_MAKE_BINHOST=1` | `010_binhost.sh` | Writes a signed [binary host]{.idx} to `build/binhost/` |
| `KDOS_PKG_STORE`, `KDOS_PKG_STORE_MAX` | `015_pkgstore.sh` | Evicts the package store down to the size given |
| `KDOS_GIT_COMMIT`, `KDOS_GIT_DIRTY`, `KDOS_GIT_DATE`, `KDOS_GIT_TIME` | `045_identity.sh`, `105_ports.sh` | Stamp the commit into `/etc/os-release`, `/ports/TREE` and `/usr/share/kdos/SOURCES` |
| `KDOS_PACK_KDOS=1` | `100_packs.sh`, `110_iso.sh` | Packs the root as the base pack `kdos` and puts it on the medium |
| `KDOS_ISO_SOURCES=1`, `KDOS_ISO_COMP` | `110_iso.sh` | Copy the tree onto the medium; choose the codec of `system.sfs` |

The `Makefile` works out the `KDOS_GIT_*` values from your clone. A build started any other way
has none of them, and the steps that read them leave their stamps out, or write `unrecorded`,
rather than invent one.

Every step sources `phase.env` itself, except `100_packs.sh`, which sources nothing and runs with
only what `exec.sh` passes; see [The pack store and the base pack](100-packs.md).

## The order of the steps

The step numbers are the only sequencing, and these orderings matter:

- `010_binhost.sh` runs before `020_cleanup.sh`, which empties the package cache the binhost is
  copied from.
- `030_launchers.sh` and `050_theme.sh` write into `/etc/skel`, and run before `070_user.sh`, which
  copies `/etc/skel` into every home. A skel changed after that step reaches no home on this image.
- `040_orphans.sh` runs before the three indexes, `060_udev_hwdb.sh`, `080_whatis.sh` and
  `085_fontcache.sh`, so a swept package's rules, manual pages and fonts are not indexed.
- `045_identity.sh` runs before `100_packs.sh`, so the base pack carries the stamped
  `/etc/os-release`.
- `087_gates.sh` runs after the sweep and every index, so it checks the tree the image carries, and
  before `090_initramfs.sh` and `110_iso.sh`, so a tree that fails it is never written into an
  image.
- `105_ports.sh` stages the recipe tree that `110_iso.sh` appends to `system.sfs` as `/ports`.

## The gates

Several steps check their own result and stop the phase when it is wrong, because the tool they
ran would have exited 0 either way. [087_gates.sh](087-gates.md) is the [image gate]{.idx} over the
whole tree; the others are local to their step:

| Step | Stops the phase when |
|:--|:--|
| `010_binhost.sh` | The flag is on and no package was kept, or the index does not verify |
| `020_cleanup.sh` | A Python module a package owns has no bytecode |
| `030_launchers.sh` | No launcher table was written, or launchers survive a reconcile that found no packs |
| `040_orphans.sh` | No recipe is found under `/ports/core`, or more than half the installed packages would be removed |
| `050_theme.sh` | `KDOS_ACCENT` is not libkcolor's default, a seeded file is missing or empty, or `glib-compile-schemas` writes no `gschemas.compiled` |
| `060_udev_hwdb.sh` | There is no `*.hwdb` source, or no trie was written |
| `080_whatis.sh` | No manual root exists, or no `mandoc.db` was written |
| `085_fontcache.sh` | `/usr/share/fonts` is missing, no cache file was written, or fontconfig rejects it |
| `087_gates.sh` | `testing/linkcheck.py` or `testing/entrycheck.py` reports a finding, or `testing/unowned.py` finds a file no package owns under `/usr/share/locale`, `man`, `doc` or `info` |
| `100_packs.sh` | With `KDOS_PACK_KDOS=1`: `mkfs.erofs` does not honour the excludes |
| `105_ports.sh` | `ports/shelves`, `ports/sources.idx`, a recipe under `/ports/core` or one of the book directories is missing, a file over 16 MB is in the recipe tree, or no installed package's source is in `sources.idx` |

A step whose tool is not installed at all (`kdos-appbox`, `udevadm`, `makewhatis`, `fc-cache`)
prints a line saying so and exits 0, so a tree built without that package still makes an image.

## What the phase leaves

Most steps change `build/fs`, which `110_iso.sh` squashes into `system.sfs`; each chapter says
where its step's output lands. Outside the root, under `build/`:

| Path | Written by |
|:--|:--|
| `build/binhost/`, `build/binhost-key/` | `010_binhost.sh`, with `KDOS_MAKE_BINHOST=1` |
| `build/initramfs/`, `build/initramfs.cpio.gz` | `090_initramfs.sh` |
| `build/kdos-base/kdos.kpack` | `100_packs.sh`, with `KDOS_PACK_KDOS=1` |
| `build/image-ports/` | `105_ports.sh` |
| `build/iso-build/kdos.iso` | `110_iso.sh` |

## See also

- [Packaging: 70_image](../../kdos/05-developer/how-kdos-is-built.md#packaging-70_image): the phase in the whole journey
- [The packaging steps](../../kdos/05-developer/build-system.md#the-packaging-steps): every step in one table
- [Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image): the three indexes
- [Boot and init](../../kdos/03-architecture/boot-and-init.md): the initramfs and the A/B slots this phase writes
- [Packs and boxes](../../kdos/03-architecture/packs-and-boxes.md): the pack store and the base pack

<!-- book-nav -->
---

*Part XV. The image.* Previous: [338. Installing the kernel](../14-kernel/installing.md) · [Contents](../README.md) · Next: [341. The binary host](010-binhost.md)
