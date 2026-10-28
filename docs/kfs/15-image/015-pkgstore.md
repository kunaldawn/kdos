# The package store

`script/phases/70_image/015_pkgstore.sh` keeps the [package store]{.idx} under a size limit. The
store, `build/pkgstore`, holds every package a build made under a key that names its recipe, its
environment and the exact bytes of its dependencies, so that a later build can install the stored
package instead of compiling the port again. It is off by default. When it is on, this step deletes
the entries used least recently until the store fits under `KDOS_PKG_STORE_MAX`.

## The step

<!-- kfs:include script/phases/70_image/015_pkgstore.sh lines=19-27 pin=0059c4b5c36d -->

`make build KDOS_PKG_STORE=1` turns the store on, and `KDOS_PKG_STORE=check` turns it on in check
mode, which builds every port anyway and compares the result with the stored package. For either,
`script/chroot/exec.sh` sets `KPKG_STORE` to `/kdos/build/pkgstore`; otherwise it sets it empty,
and the step prints that the store is off and exits.

`kpkg store gc` orders the entries by the modification time of each entry's `META` file, which kpkg
touches whenever it reuses the entry or a build makes the same package again, and deletes the
oldest first until the store's total size is at most the limit. An entry directory with no `META`,
which is a store write that never finished, is deleted whatever the size. The limit is
`KDOS_PKG_STORE_MAX`, `60G` unless the command line says otherwise. It is a whole number of bytes
with an optional `K`, `M`, `G` or `T` suffix, each a power of 1024:

```bash
make build KDOS_PKG_STORE=1 KDOS_PKG_STORE_MAX=30G
```

## Why it runs here

By this step every port the build names has been installed, either built or reused from the
store, so every entry this build touched is newer than every entry it did not. Evicting at this
point removes what the current tree has no use for, such as the packages of recipes that have
since changed, and keeps the whole of what the next build of the same tree would ask for, as long
as that fits under the limit. A limit smaller than one build's packages evicts some of this
build's own entries, and the next build compiles those ports again.

Skipping the step only lets the store grow: every changed recipe adds an entry and nothing
removes one. `make clean` removes the whole store; `make cleanbuild` keeps it, with the snapshots
and the compiler cache.

## What it leaves

Nothing on the image. The store is under `build/` on your machine and is never copied into the
root.

## See also

- [The package store](../../kdos/03-architecture/packaging.md#the-package-store): the key, check mode and eviction
- [The package store](../../kdos/05-developer/how-kdos-is-built.md#the-package-store): where the store sits in a build
- [Caches and the package store](../01-preparing/build-tree.md#caches-and-the-package-store): the directory on your machine

<!-- book-nav -->
---

*Part XV. The image, chapter 342.* Previous: [341. The binary host](010-binhost.md) · [Contents](../README.md) · Next: [343. Cleaning up the root](020-cleanup.md)
