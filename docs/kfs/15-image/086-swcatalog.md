# The software catalogue

Discover lists the ports a machine has installed from an AppStream catalogue,
[`/usr/share/swcatalog/xml/kdos-ports.xml`]{.idx}, together with kpkg's database.
`script/phases/70_image/086_swcatalog.sh` writes it over the finished tree with
[`kdos-discover-catalog`]{.idx}, from the [kdos-discover](#port-kdos-discover) port. On a running
system the same program rewrites the file after every install Discover makes; the build installs
with `kpkg` directly and never runs it, so without this step the image would carry no catalogue and
Discover would list no installed port.

## Writing the catalogue

<!-- kfs:include script/phases/70_image/086_swcatalog.sh lines=33-40 pin=91e83ce0331a -->

Without `kdos-discover-catalog` the step says so and exits 0: a tree built without the desktop
phase has no Discover to read the file.

`--root /` reads kpkg's manifests under `/var/lib/kpkg/db`, which name every file each package
installed. A package that ships a metainfo file under `/usr/share/metainfo` gives that component;
a desktop entry no metainfo of the same package launches becomes a component of its own, and one
that is `NoDisplay` or `Hidden` is skipped. Every component carries the package that owns it and
that package's version and release. The whole file is written each run, to a temporary name
renamed into place.

## Checking it

<!-- kfs:include script/phases/70_image/086_swcatalog.sh lines=42-47 pin=28ea25f35492 -->

A catalogue with no component means the manifests or the metainfo were not read, and stops the
phase. The log ends with the number of components.

## Why it runs here

After [040_orphans.sh](040-orphans.md), so a removed package is not offered as installed, and
before [087_gates.sh](087-gates.md), `090_initramfs.sh` and `110_iso.sh`.

## What it leaves

`/usr/share/swcatalog/xml/kdos-ports.xml` on the image, a file no package owns, which the unowned
report subtracts as generated.

## See also

- [Host software](../../kdos/04-programs/discover.md#host-software): how Discover reads the catalogue

<!-- book-nav -->
---

*Part XV. The image, chapter 375.* Previous: [374. The font cache](085-fontcache.md) · [Contents](../README.md) · Next: [376. The image gates](087-gates.md)
