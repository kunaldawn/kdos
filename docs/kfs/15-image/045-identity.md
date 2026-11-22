# Stamping the build identity

[`/etc/os-release`]{.idx} comes from `fs/etc/os-release` and names the distribution and its version. It
does not say which tree a particular image was built from.
`script/phases/70_image/045_identity.sh` adds that: the commit the build ran from, as
`os-release`'s own [BUILD_ID]{.idx} field, and that commit's date as `KDOS_BUILD_DATE`. System
Settings → **About this System** shows the build beside the version, from
`/usr/share/kdos/xdg/kcm-about-distrorc`'s `ShowBuild=true`.

## Where the values come from

The `Makefile` works them out from your clone when it starts the build container:

<!-- kfs:include Makefile lines=218-221 pin=91e0519d3113 -->

`KDOS_GIT_COMMIT` is the short hash of `HEAD`, `KDOS_GIT_DIRTY` is `1` when `git status` shows any
change, `KDOS_GIT_DATE` is the commit date as `YYYY-MM-DD`, and `KDOS_GIT_TIME` the commit time in
Unix seconds, which this step and [105_ports.sh](105-ports.md) use. `script/chroot/exec.sh` passes all four into
the chroot.

## The step

<!-- kfs:include script/phases/70_image/045_identity.sh lines=51-77 -->

The phase runs on the tree it finds, so the file may already carry a stamp from an earlier run. The
`grep` drops any `BUILD_ID` and `KDOS_BUILD_DATE` line, and `cat` writes what is left back into
the same file, so its mode and owner are unchanged.

Then the step reads `VERSION_ID` back. On a release build, `KDOS_GIT_RELEASE` (a clean checkout
with one release tag on `HEAD`; see [Entering the chroot](../04-chroot/entering.md)) is set, and the
step stops when that tag without its `v` is not the file's `VERSION_ID`, with
`FATAL: release v0.2.1 but /etc/os-release says VERSION_ID=0.2 — bump fs/etc/os-release, or run
10_bootstrap:000_file_system.sh again if it is bumped`. About this System,
`kdos version`, the boot menus, What's New and `/etc/kdos-release` all name the release from this
line, so an image built from a tag whose file was not bumped would call itself the previous
release everywhere. The root holds the copy `10_bootstrap`'s file-system step made of
`fs/etc/os-release`, so a bump reaches a tree already built only when that step runs again.

Without `KDOS_GIT_COMMIT`, which is a build not started by `make build`
([kdosbuild](../01-preparing/kdosbuild.md) run by hand, for instance), the step stops there, and
the file carries neither line. About this System then shows the image as unstamped; no commit is made
up.

With it, `BUILD_ID` is the commit, with `-dirty` appended when the tree had changes git had not
recorded. A dirty image cannot be reproduced from its commit alone, and the suffix says so.
`KDOS_BUILD_DATE` is written only when the date is known.

The date is the commit's, not the clock's. Two builds of the same commit write the same file, as
the rest of the image is meant to be the same for the same tree; a clock date would make them
differ in this one file.

Then the step writes [`/etc/kdos-release`]{.idx}, the record of which release this root holds:

<!-- kfs:include script/phases/70_image/045_identity.sh lines=79-94 -->

`VERSION` has the one form `kdos update` also writes when it fills a slot, so the two slots of a
machine read alike in System Versions and on the logout screen: a release build gives the release
without its tag's `v` (`0.2.1`), any other build `os-release`'s `VERSION_ID` with `+` and the build
id (`0.2.1+4dc1f7f82-dirty`), the same name `kdos-bootctl` and `kdos-sysupdated` give a root
without a record; `TREE` and `TREE_TIME` are the commit and its
time; `INSTALLED_BY` says the root came from the medium. `kdos update` offers a slot image only
when the image's time is later than `TREE_TIME`, so a root without the record is never offered
one, and `kdos-bootctl` copies the record to the ESP for the boot menu. `kdos update` rewrites it
in each slot it fills. Without `KDOS_GIT_COMMIT` the step removes the file instead.

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

`/etc/os-release` on the image, with the two lines at its end, and `/etc/kdos-release`. The installer copies the root, so an
installed system carries the stamp of the medium it was installed from.

## See also

- [Files used as an interface](../../kdos/06-reference/filesystem-and-ipc.md#files-used-as-an-interface): `/etc/os-release` and its readers
- [Build](../../kdos/06-reference/filesystem-and-ipc.md#build): the `KDOS_GIT_*` variables
- [Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages): what the same tree is meant to reproduce

<!-- book-nav -->
---

*Part XV. The image, chapter 370.* Previous: [369. Sweeping orphaned packages](040-orphans.md) · [Contents](../README.md) · Next: [371. Seeding the theme](050-theme.md)
