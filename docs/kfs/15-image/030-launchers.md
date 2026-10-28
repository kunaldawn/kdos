# The application launchers

A boxed application reaches the desktop through generated files: a [desktop entry]{.idx} for the Start
menu, a [MIME]{.idx term="MIME type"} cache for Open With, a table that maps each command name to its box, and a shim in
`/usr/local/bin` so the application is a command. `kdos-appbox genlaunchers` writes all four from
the applications in a set of packs. `script/phases/70_image/030_launchers.sh` runs it against the
packs the image carries, so the launchers on the medium match its packs exactly. The medium
carries no application packs, so on a standard build this step removes every generated [launcher]{.idx}.

## The step

<!-- kfs:include script/phases/70_image/030_launchers.sh lines=49-62 pin=6d8af2dadcf1 -->

`--packs-dir` reads one extracted pack per subdirectory, named after the pack, and takes each
pack's `usr/share/applications`. The other form, `--packs`, asks `kdos-packd` for the installed
packs and mounts them, and there is no daemon and no mount in a build chroot. The last argument,
`/`, is the root to write into, which in the chroot is `build/fs`.

The extraction directory is `/var/tmp/kdos-pack-extract` unless `KDOS_PACK_EXTRACT` names another.
`script/chroot/exec.sh` does not pass that variable into the chroot, so under `make build` it is
always the default. Once `kdos-appbox` is found, the step creates the directory empty when it is
missing and removes it again at the end if it is still empty; `genlaunchers` reads a missing
directory as no packs and still sweeps. No step of the phase extracts a pack into it, so on a standard build it is empty.

`genlaunchers` reconciles rather than appends. It removes every desktop entry carrying
`X-KDOS-Alien=true` and every `/usr/local/bin` link it recognises as its own shim (a symbolic
link with a relative target or the target `/usr/local/bin/kdos-appbox`, except under a name it
reserves, such as `kdos-box` and `xdg-open`), and rewrites the table and the MIME cache whole,
from the packs it found. Against an empty source that is the clean-out; against one holding packs
it is the population. The four outputs and their naming
rules are in [Launcher generation](../../kdos/04-programs/kdos-appbox.md#launcher-generation).

`kdos-appbox` installs in `/usr/local/bin`, which is last on the chroot's `PATH`. Without the
`kdos-appbox` package the step prints that it is not installed and exits 0 before it creates
anything, and whatever launchers the tree holds ship unchanged.

## The check

<!-- kfs:include script/phases/70_image/030_launchers.sh lines=68-83 pin=b9ba0e6bd5ac -->

`genlaunchers` exits 0 whether it wrote anything or not, so the step checks what is on disk
instead. The table, [`/usr/share/kdos/alien-apps`]{.idx}, must exist and not be empty: with no packs it
is its `#` header line alone. The step counts its rows and the alien desktop entries in
[`/etc/skel/.local/share/applications`]{.idx}, and prints both. A table with no rows beside alien entries
means the sweep did not happen, and the phase stops.

## Why it runs before 070_user.sh

The desktop entries are written into [`/etc/skel`]{.idx}, the template for every home. `070_user.sh`
copies `/etc/skel` into `/home/kdos`, and the Start menu reads the home's entries before the
system's. Run after that step, the reconcile would clean `/etc/skel` and leave the old launchers
in `/home/kdos`, offering applications whose packs the medium does not have.

The launchers are generated, not copied from `fs/`, so the `fs/` manifest that removes files `fs/`
has dropped (see
[Files gone from fs/ are removed](../03-bootstrap/000-file-system.md#files-gone-from-fs-are-removed))
never reaches them. This step is what removes them.

## What it leaves

On the image:

| Path | Holds |
|:--|:--|
| `/etc/skel/.local/share/applications/` | One desktop entry per boxed application, and its `mimeinfo.cache` |
| `/usr/share/kdos/alien-apps` | The command table, header line only when there are no packs |
| `/usr/local/bin/` | One shim per boxed application |

On a standard build the first and last hold no generated launcher.

## See also

- [Launcher generation](../../kdos/04-programs/kdos-appbox.md#launcher-generation): every form of `genlaunchers` and its outputs
- [Where a pack comes from](../../kdos/03-architecture/packs-and-boxes.md#where-a-pack-comes-from): why the medium carries no applications
- [Creating the homes](070-user.md): the step that copies `/etc/skel`

<!-- book-nav -->
---

*Part XV. The image, chapter 344.* Previous: [343. Cleaning up the root](020-cleanup.md) · [Contents](../README.md) · Next: [345. Sweeping orphaned packages](040-orphans.md)
