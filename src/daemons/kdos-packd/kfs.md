## About

`kdos-packd` is the only program on a KDOS machine that mounts an application [pack]{.idx}: a
signed, read-only EROFS image holding an application, a runtime it needs, or data. It verifies a
pack's signature before it mounts it, composes a box's root filesystem as an overlay of the packs
that box is made of, installs and rolls back packs in the store under `/var/lib/kdos/packs`, and
places data packs where their consumers look. A client names a pack by an id out of the list the
daemon published, never by a path. Mounting as root is what makes the overlay's whiteouts real, so
a box's root is composed from the packs as they are, with no flattening step. Its clients are
[kdos-appbox](#port-kdos-appbox) and `kdos doctor`. The pack format is in
[Packs and boxes](../../../docs/kdos/03-architecture/packs-and-boxes.md), and the daemon's verbs, paths and
configuration in [kdos-packd](../../../docs/kdos/04-programs/daemons.md#kdos-packd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles the daemon's files with
the sources of `libkbase`, `libksig` (and the Monocypher code inside it, which is where the
recipe's `BSD-2-Clause OR CC0-1.0` licence comes from), `libkpkg` and `libkpack`, and nothing
else, because every library a root daemon links is code running as root; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

The package installs `/var/lib/kdos/packs/staging` and `/var/lib/kdos/packs/mnt` as ordinary
directories. The daemon sets the staging directory to mode 01777 itself when it starts, which is
where an unprivileged client copies a pack before asking for `install`; until the daemon has run
once, nobody but root can write there.

The daemon mounts with the kernel's `erofs` driver and runs no program of
[erofs-utils](#port-erofs-utils), which the recipe depends on. `/etc/init.d/59_packd.sh`
skips the daemon at boot when `erofs` is missing from `/proc/filesystems` after
`modprobe erofs`, so a kernel without EROFS boots with no pack support at all.
