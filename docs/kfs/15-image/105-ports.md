# The recipe tree and the source offer

Every KDOS image carries the tree it was built from, at `/ports`, as recipes only. A shipped
machine updates by building from that tree, and kpkg fetches each source over the network when a
port is built, so none of the fetched tarballs has to be on the medium.
`script/phases/70_image/105_ports.sh` stages the tree, stamps it with the commit it came from,
installs the key a newer tree must be signed with, and writes [`/usr/share/kdos/SOURCES`]{.idx}, the
image's [source offer]{.idx}: where the source of everything on the image can be had.

## Staging the tree

<!-- kfs:include script/phases/70_image/105_ports.sh lines=49-63 pin=6a0ab4689b2b -->

In the chroot, `/ports` is a bind mount of the repository's `ports/` directory, which holds every
fetched tarball beside its recipe and which the first squash of the root leaves out. It cannot be
filtered where it is, so the tree is staged as a copy in `build/image-ports/ports`, and
`110_iso.sh` appends that to `system.sfs` as `/ports`. The staging directory is emptied first, and
`umask 022` keeps the group and other write bits off everything the step creates. `ports/shelves`, the
list of shelves, and `ports/sources.idx`, the index of the source archive, are required.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=65-88 pin=19db40289f52 -->

For each port under `/ports/core/<shelf>/<name>`, every file is copied except two kinds of source:

- a file the recipe hashes, named in a `sha256 = <hash>  <file>` line, which is how a recipe names
  its sources (see [Checksums](../../kdos/05-developer/writing-ports.md#checksums));
- any archive, by its extension, whether or not a recipe names it. This catches a tarball that an
  earlier version of the recipe named and the present one does not, which would otherwise ride
  along.

Files and directories whose names begin with a dot are skipped. What is left is the recipe,
`build.sh`, patches, `postinstall.sh`, the port's `kfs.md` note and any other file the recipe
ships. No recipe at all under `/ports/core` stops the phase.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=90-98 pin=ffe47f64821a -->

`src/` is copied whole to `/ports/src`, because the programs of KDOS's own are built from it and no
archive holds their sources. Its areas stay side by side, since a desktop program's `build.sh`
reaches the libraries as `$PORT_SRC/../../libs`, and `/etc/kpkg.conf` names the areas that hold
ports in its `PORT_REPO`.

The two books, `docs/kdos` with `docs/screenshots`, and `docs/kfs` with its renderer
`script/book`, are copied to the same relative places under `/ports`. The ports `kdos-doc` and
`kdos-kfs` reach them through links in their port directories (`../../../docs/kdos`,
`../../../docs/kfs`, `../../../script/book`), and the recipe hash follows those links. A link left
dangling would drop out of the hash, so the tree on the machine would hash the port differently
from the tree its package was built from, and kpkg would take the installed package for stale. A
missing directory stops the phase. `kdos-kfs` builds only in a full checkout all the same, because
its renderer also reads `ports/core` and `script/phases`.

Dot-files and `__pycache__` directories are removed from the copies, and every mode is made
readable by all.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=100-108 pin=b0eac1a92403 -->

A file over 16 MB under the copied ports is a source the filters above missed, and stops the phase,
naming up to five of them. The log ends with the number of ports, files and programs of our own,
and the size of the staged tree.

## The stamp and the key

<!-- kfs:include script/phases/70_image/105_ports.sh lines=122-141 pin=f377f0227aab -->

`/ports/TREE` names the commit the tree is and that commit's time, in the format
`ports/publish --release` writes into a published tree. `kdos update fetch` replaces `/ports` only
with a signed tree whose `TREE` is newer, so an image without the stamp would take any signed
tree, an older one included. Without `KDOS_GIT_COMMIT` there is no stamp.

[`/etc/kdos/keys/tree`]{.idx} is the keyring `kdos update fetch` checks a published tree against. The step
empties it of keys and installs the public half of the release key, `build/keys/kdos-release.pub`,
the key `ports/publish --release` signs with. The key lives in `build/keys/` on the machine that
builds, never in git. A build without it ships an empty ring, and that image refuses every fetched
tree rather than trusting an unsigned one.

## The source offer

<!-- kfs:include script/phases/70_image/105_ports.sh lines=151-171 pin=959efcc4c4c7 -->

The step reads every installed package's recipe, under `/ports/core` or `/kdos/src`, and collects
the hash of every source it names. `ports/sources.idx` says which `sources-<N>` release of the
archive holds each hash, so the releases the image's sources are in are those the index gives for
any of the collected hashes. The hashes the index does not hold are counted.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=173-199 pin=2ea75f8df3d3 -->

`SOURCES` names the repository, the commit (with `-dirty` when the tree had unrecorded changes,
`unrecorded` without a commit), its date, and a `release` line for each archive release that
holds a source of an installed package. The recipes and the programs of our own are the tree at
that commit, a copy of which is `/ports`. A source the index does not hold is reported as a
warning: `ports/publish` has not run for it, and a machine fetching it goes to the upstream URL its
recipe names. If no installed package's source is in the index at all, the phase stops.

## What it leaves

On the image, `/etc/kdos/keys/tree/` with the release key or empty, and
`/usr/share/kdos/SOURCES`. On your machine, `build/image-ports/ports/`, which becomes `/ports` on
the image when `110_iso.sh` appends it to `system.sfs`:

```text
/ports/core/<shelf>/<name>/   each port's recipe files
/ports/shelves
/ports/sources.idx
/ports/TREE
/ports/src/                   the programs of KDOS's own
/ports/docs/kdos/  /ports/docs/screenshots/  /ports/docs/kfs/
/ports/script/book/
```

## See also

- [Packaging: 70_image](../../kdos/05-developer/how-kdos-is-built.md#packaging-70_image): the same step in the build overview
- [Updating a machine](../../kdos/03-architecture/packaging.md#updating-a-machine): how `/ports` is used and replaced
- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update): `fetch` and `apply`
- [Sources and the archive](../01-preparing/sources.md): `sources.idx` and the `sources-<N>` releases
- [The recipe hash](../../kdos/03-architecture/packaging.md#e--the-recipe-hash): what the links feed into

<!-- book-nav -->
---

*Part XV. The image, chapter 356.* Previous: [355. The pack store and the base pack](100-packs.md) · [Contents](../README.md) · Next: [357. The ISO](110-iso.md)
