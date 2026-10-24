# Sweeping orphaned packages

`build/fs` is built incrementally: a package installed by one build stays installed through every
later one unless something removes it. A port deleted from the tree therefore leaves its package
behind, with its programs, libraries and service files, and nothing in a package phase notices.
`script/phases/70_image/040_orphans.sh` removes every installed package that has no recipe, and
every package whose recipe `ports/unbuilt` lists, before the image is made. This page shows how it
finds the recipes and the two refusals that keep a broken lookup from emptying the image.

## Where it looks for a recipe

<!-- kfs:include script/phases/70_image/040_orphans.sh lines=29-46 pin=1d210d8327cf -->

`REPOS` is every repository a phase's `PORT_REPO` names, and `src/libs`. `has_port` looks for a
`kpkgbuild` directly under each repository and one [shelf]{.idx} down, as kpkg itself does, so
`ports/core/cli/pv` and `src/system/kdos-tools` are both found by name. See
[Where kpkg looks for a port](../../kdos/05-developer/build-system.md#where-kpkg-looks-for-a-port).

The list is written out in the script, and it must hold every place a recipe can be. A repository
or a shelf level missing from it makes every package from there look orphaned, and the sweep would
uninstall them all. `testing/preflight.sh` fails when the list leaves out a `src/` area that holds
a recipe.

## The refusals

<!-- kfs:include script/phases/70_image/040_orphans.sh lines=48-75 pin=b45c37bc3f81 -->

A package is an orphan when no recipe has its name, or when its name is in `ports/unbuilt`, the
list of recipes no phase builds. A package of that second kind can only be left from a build made
before the port was listed there, and would otherwise ship.

The step stops the phase before it removes anything when `/ports/core` holds no recipe at all,
which is a broken bind mount rather than an empty distribution. It also stops when more than half
the installed packages would go:

<!-- kfs:include script/phases/70_image/040_orphans.sh lines=77-83 pin=298e87d46e48 -->

Retiring half the system in one pass is never a port removal; it is a lookup that misses a whole
repository or shelf level. Either way, sweeping would ship an image with most of the system
missing.

## The removal

<!-- kfs:include script/phases/70_image/040_orphans.sh lines=85-101 pin=fd834092d466 -->

Each orphan is listed in the log and then removed with [`kpkgdel`]{.idx}, which deletes the files its
database entry names and the entry itself. A package that fails to remove, for instance because its
manifest is damaged, is reported with `[WARN]` and left in place, and the phase goes on: one damaged
package does not stop the image from being made.

## Why it runs here

The sweep runs before the hardware database, the manual index and the font cache are built, so an
orphan's udev rules, manual pages and fonts are not indexed into an image that does not carry them,
and before [087_gates.sh](087-gates.md) checks the tree. Files under `fs/` need no sweep:
`000_file_system.sh` keeps a manifest of them and removes a path `fs/` has dropped.

## What it leaves

Every package removed is gone from `build/fs` and from its package database,
`/var/lib/kpkg/db`, and so from the image. The log names each one; on a tree that matches the
recipes, it prints `no orphaned packages`.

## See also

- [Sweeping orphaned packages](../../kdos/05-developer/build-system.md#sweeping-orphaned-packages): the same rules in the build-system chapter
- [Upgrades and removals](../../kdos/03-architecture/packaging.md#upgrades-and-removals): what `kpkgdel` does
- [Files gone from fs/ are removed](../03-bootstrap/000-file-system.md#files-gone-from-fs-are-removed): the counterpart for `fs/`

<!-- book-nav -->
---

*Part XV. The image, chapter 345.* Previous: [344. The application launchers](030-launchers.md) · [Contents](../README.md) · Next: [346. Stamping the build identity](045-identity.md)
