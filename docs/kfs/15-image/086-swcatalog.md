# The software catalogue

Discover lists the ports a machine has installed from an AppStream catalogue,
[`/usr/share/swcatalog/xml/kdos-ports.xml`]{.idx}, together with kpkg's database.
`script/phases/70_image/086_swcatalog.sh` writes it over the finished tree with
[`kdos-discover-catalog`]{.idx}, from the [kdos-discover](#port-kdos-discover) port. On a running
system the same program rewrites the file after every install Discover makes; the build installs
with `kpkg` directly and never runs it, so without this step the image would carry no catalogue and
Discover would list no installed port. Under `KDOS_IMAGE=base` the finished tree is the
[image stage](017-stage.md), so the catalogue names what the base installed; the ports a machine
can add are in the binhost's own catalogue, which [010_binhost.sh](010-binhost.md#the-catalogue)
writes.

## Writing the catalogue

<!-- kfs:include script/phases/70_image/086_swcatalog.sh lines=41-48 pin=91e83ce0331a -->

Without `kdos-discover-catalog` the step says so and exits 0: a tree built without the desktop
phase has no Discover to read the file.

`--root /` reads kpkg's manifests under `/var/lib/kpkg/db`, which name every file each package
installed. A package that ships a metainfo file under `/usr/share/metainfo` gives that component;
a desktop entry no metainfo of the same package launches becomes a component of its own, and one
that is `NoDisplay` or `Hidden` is skipped. Every component carries the package that owns it and
that package's version and release. The whole file is written each run, to a temporary name
renamed into place.

## Checking it

<!-- kfs:include script/phases/70_image/086_swcatalog.sh lines=50-55 pin=28ea25f35492 -->

A catalogue with no component means the manifests or the metainfo were not read, and stops the
phase. The log ends with the number of components.

## Taking excluded Debian applications out of the pool

The step's last command writes [`/usr/share/swcatalog/xml/kdos-debian-exclude.xml`]{.idx}: one
`remove-component` merge for each desktop application of debian-dep11's catalogue whose package
`/usr/share/kdos/discover/debian-exclude` names, except one whose id `kdos-ports.xml` also has: a
merge removes every component of its id, the host's own application included. libappstream applies the merges once every
catalogue has loaded, so no AppStream pool on the machine holds those applications, and KRunner's
software results, which read the pool and nothing else, do not offer one that Discover's Debian
lane would refuse to build. The writer fails, and the phase with it, when no DEP-11 catalogue is
installed or the list names nothing the catalogue has.

## Why it runs here

After [040_orphans.sh](040-orphans.md), so a removed package is not offered as installed, and
before [087_gates.sh](087-gates.md), `090_initramfs.sh` and `110_iso.sh`.

## What it leaves

`/usr/share/swcatalog/xml/kdos-ports.xml` and `kdos-debian-exclude.xml` on the image, files no
package owns, which the unowned report subtracts as generated.

## See also

- [Host software](../../kdos/04-programs/discover.md#host-software): how Discover reads the catalogue

<!-- book-nav -->
---

*Part XV. The image, chapter 376.* Previous: [375. The font cache](085-fontcache.md) · [Contents](../README.md) · Next: [377. The image gates](087-gates.md)
