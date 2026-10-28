# The fs/ overlay

Most of a KDOS root is installed by packages, but about two hundred files belong to no package: the
boot scripts, the account databases, the shell profile, the skeleton of a new user's home, the
firewall rules. They are kept in the repository under `fs/`, laid out as they sit in the root, and
copied into `build/fs` by the first step of `10_bootstrap`, `000_file_system.sh`. This chapter says
what is in `fs/`, what that step does with it, and what does not belong there.

## What fs/ holds

`fs/` mirrors the root filesystem. A file at `fs/etc/inittab` becomes `/etc/inittab` in the root,
and one at `fs/usr/local/bin/kdos-desktop` becomes `/usr/local/bin/kdos-desktop`. The main areas are:

`fs/etc/`
:   The system's own configuration: `inittab`, which sysvinit reads, and the boot scripts under
    `init.d/`, numbered scripts that `rcS` runs in order at boot, one per service or boot task, with
    `service.disabled/` naming those that must not start; `passwd`, `group` and [`shadow`]{.idx}
    with the accounts every machine has; `profile`, `bash.bashrc` and `profile.d/`; `fstab`,
    `hostname`, `hosts`, `nsswitch.conf` and `resolv.conf`; the firewall in `nftables.conf` and
    `nftables.d/`; KDOS's own settings under `kdos/`, such as `login.conf` and `zram.conf`, and the
    directories of trusted signing keys under `kdos/keys/`; and configuration for udev, polkit, PipeWire, ALSA
    and the XDG menus.

`fs/etc/skel/`
:   The skeleton of a user's home directory, copied into each home when the image phase creates the
    users. `fs/root/` holds root's own `.bashrc` and `.bash_profile`.

`fs/usr/local/`
:   KDOS's own scripts that belong to no port: the desktop's start-up script `bin/kdos-desktop` and
    a few helpers beside it, the tty1 login script `sbin/kdos-login`, and the session bring-up that
    `kdos-desktop` and `kdos-desktop-start` share, `lib/kdos/session-common.sh`.

`fs/usr/share/`
:   Data files: the boot menu's pictures, the in-system help pages, the logo and the screensaver
    text under `kdos/`; the portal configuration `xdg-desktop-portal/kdos-portals.conf`; the
    GSettings override in `glib-2.0/schemas/`; bash completion for the `kdos` command; the desktop
    entries of KDOS's own tools under `applications/`; and the default wallpaper under
    `backgrounds/kdos/`.

`fs/usr/lib64`
:   A symbolic link to `lib`, so that the root has one library directory. Anything that installs
    into `/usr/lib64` lands in `/usr/lib`.

`fs/etc/mtab`
:   A symbolic link to `../proc/self/mounts`, the kernel's list of mounts, for the programs that
    read or write `/etc/mtab`: lvm2's `lvextend -r` and `mount.nfs4` fail without it; see
    [Mount points that must be right](../../kdos/03-architecture/boot-and-init.md#mount-points-that-must-be-right).

To see the whole of it, list it from the top of the repository on your machine:

```bash
find fs -type f -o -type l | sort
```

## What the copy does with it

`000_file_system.sh` copies `fs/` into the root on every run of `10_bootstrap`, with no "already
done" marker, so an edit to `fs/` reaches the tree the next time the step runs. Because it runs
over a tree that packages have already filled, the copy is more than a `cp -r`, and four things
follow for whoever edits `fs/`:

- An account in `fs/etc/passwd`, `group` or `shadow` always wins over the tree's entry of the same
  name, and an account only the tree has, one a package's install hook added, survives the copy.
- A file deleted from `fs/` is deleted from the tree on the next run, because the step keeps a
  manifest of what `fs/` provided in `/var/lib/kdos/fs-manifest`. Only files and links the
  manifest names are removed, never a directory.
- Every regular file from `fs/` ends up 755 when it is executable in the repository and 644 when it
  is not. git carries no other mode and no owner, so a path that needs one, such as `/etc/shadow`
  at 600 or the polkit and udev rules owned by root, is listed in a table inside the step.
- The link `fs/usr/lib64` cannot replace a real directory, so a tree whose `/usr/lib64` is still a
  directory keeps it until it is next built from empty.

Each of these is shown line by line, with what goes wrong without it, in
[The file system](../03-bootstrap/000-file-system.md#copying-fs), the chapter on that step.

## Running the step on its own

After changing anything under `fs/`, copy it into an existing tree and rebuild the image, without
re-running the rest of the bootstrap, with a build plan. From the top of the repository on your
machine:

```bash
make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"
```

`--phases` runs only `10_bootstrap` and `70_image`, and `--steps` limits `10_bootstrap` to this
one script; the image phase runs whole.
[Narrowing a build](kdosbuild.md#narrowing-a-build) explains both.

## What does not belong in fs/

`fs/` holds what no package installs. A few rules follow from how the copy works and from the
system's own principles:

- **No file a package installs.** The copy writes over whatever is at a path, so a file both in
  `fs/` and in a package ends up as `fs/`'s copy the next time the step runs, while the package
  database still records the package's. Deleting such a file from `fs/` later deletes it from the
  tree too, package or not, because the manifest named it. Configuration a port installs belongs
  in its port.
- **No `fs/etc/X11/`.** KDOS has no X server on its login path, and the directory must never exist;
  see [No Xorg server](../../kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).
- **Nothing for systemd.** No unit files and no `systemd-*` configuration; services are started by
  the scripts in `fs/etc/init.d/`. See [No systemd](../../kdos/01-philosophy/principles.md#no-systemd).
- **No mode or owner git cannot carry,** unless it is added to the step's table as well.

## See also

- [Syncing fs/](../../kdos/05-developer/build-system.md#syncing-fs): the same step in the KDOS book, with the home directories the image phase makes from `/etc/skel`
- [The file system](../03-bootstrap/000-file-system.md): `000_file_system.sh` line by line, the copy of `fs/` included
- [The build tree](build-tree.md): where `build/fs` sits among the build's other directories
- [Boot and init](../../kdos/03-architecture/boot-and-init.md): what the scripts in `fs/etc/init.d/` do at boot

<!-- book-nav -->
---

*Part I. Preparing, chapter 3.* Previous: [2. Sources and the archive](sources.md) · [Contents](../README.md) · Next: [4. The build tree](build-tree.md)
