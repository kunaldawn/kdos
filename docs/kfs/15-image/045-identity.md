# Stamping the build identity

[`/etc/os-release`]{.idx} comes from `fs/etc/os-release` and names the distribution and its version. It
does not say which tree a particular image was built from.
`script/phases/70_image/045_identity.sh` adds that: the commit the build ran from, as
`os-release`'s own [BUILD_ID]{.idx} field, and that commit's date as `KDOS_BUILD_DATE`. [`kdos-about`]{.idx}
shows both on its Build page.

## Where the values come from

The `Makefile` works them out from your clone when it starts the build container:

<!-- kfs:include Makefile lines=178-181 pin=91e0519d3113 -->

`KDOS_GIT_COMMIT` is the short hash of `HEAD`, `KDOS_GIT_DIRTY` is `1` when `git status` shows any
change, `KDOS_GIT_DATE` is the commit date as `YYYY-MM-DD`, and `KDOS_GIT_TIME` the commit time in
Unix seconds, which [105_ports.sh](105-ports.md) uses. `script/chroot/exec.sh` passes all four into
the chroot.

## The step

<!-- kfs:include script/phases/70_image/045_identity.sh lines=32-48 pin=cdcec53c046e -->

The phase runs on the tree it finds, so the file may already carry a stamp from an earlier run. The
`grep` drops any `BUILD_ID` and `KDOS_BUILD_DATE` line, and `cat` writes what is left back into
the same file, so its mode and owner are unchanged.

Without `KDOS_GIT_COMMIT`, which is a build not started by `make build`
([kdosbuild](../01-preparing/kdosbuild.md) run by hand, for instance), the step stops there, and
the file carries neither line. `kdos-about` then shows the image as `unstamped`; no commit is made
up.

With it, `BUILD_ID` is the commit, with `-dirty` appended when the tree had changes git had not
recorded. A dirty image cannot be reproduced from its commit alone, and the suffix says so.
`KDOS_BUILD_DATE` is written only when the date is known.

The date is the commit's, not the clock's. Two builds of the same commit write the same file, as
the rest of the image is meant to be the same for the same tree; a clock date would make them
differ in this one file.

The end of the file then reads, with the commit's own values in place of the placeholders:

```text
BUILD_ID="<short commit>"
KDOS_BUILD_DATE="<YYYY-MM-DD>"
```

## Why it runs here

The stamp has to be in the root before [100_packs.sh](100-packs.md), so that the base pack `kdos`,
a copy of this root built when `KDOS_PACK_KDOS=1`, carries it, and before `110_iso.sh` squashes the
root into `system.sfs`.

## What it leaves

`/etc/os-release` on the image, with the two lines at its end. The installer copies the root, so an
installed system carries the stamp of the medium it was installed from.

## See also

- [Files used as an interface](../../kdos/06-reference/filesystem-and-ipc.md#files-used-as-an-interface): `/etc/os-release` and its readers
- [Build](../../kdos/06-reference/filesystem-and-ipc.md#build): the `KDOS_GIT_*` variables
- [Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages): what the same tree is meant to reproduce

<!-- book-nav -->
---

*Part XV. The image, chapter 346.* Previous: [345. Sweeping orphaned packages](040-orphans.md) · [Contents](../README.md) · Next: [347. Seeding the theme](050-theme.md)
