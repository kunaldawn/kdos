# The recipe tree and the source offer

Every KDOS image carries the tree it was built from, at `/ports`, as recipes only. A shipped
machine updates by building from that tree, and kpkg fetches each source over the network when a
port is built, so none of the fetched tarballs has to be on the medium.
`script/phases/70_image/105_ports.sh` stages the tree, stamps it with the commit it came from,
installs the key a newer tree must be signed with, writes [`/usr/share/kdos/SOURCES`]{.idx}, the
image's [source offer]{.idx}: where the source of everything on the image can be had, and installs
KDOS's own licence text.

## Staging the tree

<!-- kfs:include script/phases/70_image/105_ports.sh lines=57-76 pin=d96c32dc8c6f -->

In the chroot, `/ports` is a bind mount of the repository's `ports/` directory, which holds every
fetched tarball beside its recipe and which the first squash of the root leaves out. It cannot be
filtered where it is, so the tree is staged as a copy in `build/image-ports/ports`, and
`110_iso.sh` appends that to `system.sfs` as `/ports`. The staging directory is emptied first, and
`umask 022` keeps the group and other write bits off everything the step creates. `ports/shelves`, the
list of shelves, `ports/sources.idx`, the index of the source archive, `ports/groups`, the
[groups]{.idx} of applications that `kdos update install <group>` and the installer offer, and
`ports/RELEASE`, the release notes that `kdos update notes` prints and What's New shows, and
`ports/buildonly`, the build-only ports `kdos update` keeps out of a prebuilt plan, and
`ports/buildheaders`, the build-only ports a build on the machine installs prebuilt because
installed packages' headers include them, are required. The groups are what a machine reads to
install a set of ports by one name; see
[Installing software](../16-wiki/updating.md#installing-software). A `RELEASE` over 16 KiB stops
the phase, because `kdos update notes` reads only the first 16 KiB and would cut the rest off.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=78-104 pin=005ab5a0ade2 -->

For each port under `/ports/core/<shelf>/<name>`, every file is copied except two kinds of source:

- a file the recipe hashes, named in a `sha256 = <hash>  <file>` line, which is how a recipe names
  its sources (see [Checksums](../../kdos/05-developer/writing-ports.md#checksums));
- any archive, by its extension, whether or not a recipe names it. This catches a tarball that an
  earlier version of the recipe named and the present one does not, which would otherwise ride
  along.

A recipe file is never left out, even when a `sha256 =` line names it: `kpkgbuild`, `build.sh`,
`postinstall.sh` and every `.patch` are in the
[recipe hash](../../kdos/03-architecture/packaging.md#e--the-recipe-hash) whatever names them.
Without `libgd`'s `heif-stride.patch`, which its recipe also checksums, a machine would compute a
recipe hash that no binhost package carries and have no patch to build with.

Files and directories whose names begin with a dot are skipped. What is left is the recipe,
`build.sh`, patches, `postinstall.sh`, the port's `kfs.md` note and any other file the recipe
ships. No recipe at all under `/ports/core` stops the phase.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=106-114 pin=ffe47f64821a -->

`src/` is copied whole to `/ports/src`, because the programs of KDOS's own are built from it and no
archive holds their sources. Its areas stay side by side, since a desktop program's `build.sh`
reaches the libraries as `$PORT_SRC/../../libs`, and `/etc/kpkg.conf` names the areas that hold
ports in its `PORT_REPO`.

The two books, `docs/kdos` with `docs/screenshots`, and `docs/kfs` with its renderer
`script/book`, are copied to the same relative places under `/ports`. The ports `kdos-doc` and
`kdos-kfs`, whose recipes are under `src/system`, reach them through links in their port
directories: `kdos-doc` through `kdos` and `screenshots` (`../../../docs/kdos`,
`../../../docs/screenshots`), `kdos-kfs` through `docs` and `book` (`../../../docs/kfs`,
`../../../script/book`). The recipe hash follows those links. A link left
dangling would drop out of the hash, so the tree on the machine would hash the port differently
from the tree its package was built from, and kpkg would take the installed package for stale. A
missing directory stops the phase. `kdos-kfs` builds only in a full checkout all the same, because
its renderer also reads `ports/core` and `script/phases`.

Dot-files and `__pycache__` directories are removed from the copies, and every mode is made
readable by all.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=116-124 pin=2ae9719ff407 -->

A file over 16 MB under the copied `core/` is a source the filters above missed, and stops the
phase, naming up to five of them. The staging then logs the number of ports, files and programs of
our own, and the size of the staged tree.

## The stamp and the key

<!-- kfs:include script/phases/70_image/105_ports.sh lines=161-194 pin=971c5de60f2f -->

`/ports/TREE` names the commit the tree is and that commit's time, in the format
`ports/publish --release` writes into a published tree. `kdos update fetch` refuses a signed tree
whose `TREE` time is older than the one in `/ports`, and takes one of the same commit for current,
so an image without the stamp would take any signed tree, an older one included. Without
`KDOS_GIT_COMMIT` there is no stamp.

A fourth line, `version <tag>`, names the [release]{.idx term="release tag"} the image is: the
release whose signed `kdos-update.idx` the binhost is found through, so without it every prebuilt
install refuses unless `/etc/kdos/update.conf` names a `binhost =`. The line is
`KDOS_GIT_RELEASE`, which `script/chroot/exec.sh` sets to the Makefile's `KDOS_GIT_TAG`, the one
release tag (`v<digit>…`) on `HEAD`, only when the checkout is clean, untracked files included: a
dirty tree's packages are not the release's, and an archive tag (`sources-N`, `src-*`, `packs-*`)
names no release. The step says which case it met: `/ports/TREE names release <tag>`,
`the checkout has changes: /ports/TREE does not name release <tag>`, or `HEAD carries no tag:
/ports/TREE names no release, and prebuilt installs need binhost = in update.conf`. A tag outside
the alphabet `ports/publish --release` accepts stops the phase with `FATAL: tag '<t>' cannot name a
release: letters, digits and . _ + ~ - only, not first . or -, at most 40`. How a tag is chosen
and refused before the build starts is in
[The release tag](../../kdos/05-developer/build-system.md#the-release-tag).

[`/etc/kdos/keys/tree`]{.idx} is the keyring `kdos update fetch` checks a published tree against. The step
empties it of keys and installs the public half of the release key, `build/keys/kdos-release.pub`,
the key `ports/publish --release` signs with. The key lives in `build/keys/` on the machine that
builds, never in git. A build without it ships an empty ring, and that image refuses every fetched
tree rather than trusting an unsigned one.

[`/etc/kdos/keys`]{.idx} itself, the host ring, is emptied of keys and gets the same build's two:
the binhost key's public half, `build/binhost-key/kdos-binhost.pub`, which
[010_binhost.sh](010-binhost.md) signs the binhost with, and the release key's, which signs the
slot image and the release index. The release index, the binhost index, every prebuilt package
and the slot image's manifest are verified against this ring, so the image takes its own
release's updates and no other builder's. A build with neither key ships the ring empty, and
that image's `kdos update install --prebuilt` and image lane refuse with `no-key`.

## The source offer

<!-- kfs:include script/phases/70_image/105_ports.sh lines=213-233 pin=959efcc4c4c7 -->

The step reads every installed package's recipe, under `/ports/core` or `/kdos/src`, and collects
the hash of every source it names. `ports/sources.idx` says which `sources-<N>` release of the
archive holds each hash, so the releases the image's sources are in are those the index gives for
any of the collected hashes. The hashes the index does not hold are counted.

<!-- kfs:include script/phases/70_image/105_ports.sh lines=235-261 pin=2ea75f8df3d3 -->

`SOURCES` names the repository, the commit (with `-dirty` when the tree had unrecorded changes,
`unrecorded` without a commit), its date, and a `release` line for each archive release that
holds a source of an installed package. The recipes and the programs of our own are the tree at
that commit, a copy of which is `/ports`. A source the index does not hold is reported as a
warning: `ports/publish` has not run for it, and a machine fetching it goes to the upstream URL its
recipe names. If no installed package's source is in the index at all, the phase stops.

## KDOS's licence text

The repository's `LICENSE`, the MIT text of everything KDOS wrote, is installed as
[`/usr/share/licenses/kdos/LICENSE`]{.idx}; a tree without it stops the phase. No port of our own
carries a copy, and kpkg copies only what a source holds, so the step then gives each installed
package of our own its `/usr/share/licenses/<name>/LICENSE` as a link to `../kdos/LICENSE`. A
package is of our own when its recipe is under `src/`, or is a `ports/core` recipe with no
`source` (`containers-common`, `musl-ldd`). It gets the link only when its recipe's `license =`
names `MIT`, so `kdos-bb` (GPL-2.0) and `kdos-sddm-theme` (LGPL-2.0) keep only the texts their
`build.sh` installs; when it owns at least one file, so a meta package such as `kde-plasma` gets
none; and when no `LICENSE` is there already. The text and the links belong to no package:
`testing/unowned.py` subtracts exactly these, a link there pointing anywhere else is reported, and
`kpkg check --all` lists them as unowned. A package of ours installed later from the binhost gets
no link; `/usr/share/licenses/kdos/LICENSE` is its text. A recipe under `MIT AND` another licence
gets the link for its MIT part, and its `build.sh` installs the other text: `kdos-icons` the
LGPL-3.0 and GPL-3.0 texts it copies from gcc's licence directory, `kdos-splash` terminus-font's,
and `kdos-tools`, `kdos-pack` and `kdos-packd`, which link the vendored Monocypher, its
`LICENCE.md` as `monocypher-LICENCE.md`.

## What it leaves

On the image, `/etc/kdos/keys/tree/` with the release key or empty, `/etc/kdos/keys/` with the release and binhost keys the build has, and
`/usr/share/kdos/SOURCES`, `/usr/share/licenses/kdos/LICENSE` and the `LICENSE` links to it. On your machine, `build/image-ports/ports/`, which becomes `/ports` on
the image when `110_iso.sh` appends it to `system.sfs`:

```text
/ports/core/<shelf>/<name>/   each port's recipe files
/ports/shelves
/ports/sources.idx
/ports/groups
/ports/RELEASE                the release notes
/ports/TREE                   the commit, and the release on a clean tagged build
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

*Part XV. The image, chapter 382.* Previous: [381. The pack store and the base pack](100-packs.md) · [Contents](../README.md) · Next: [383. The ISO](110-iso.md)
