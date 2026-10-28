# The file system

The first step of the bootstrap compiles nothing. `script/phases/10_bootstrap/000_file_system.sh`
lays out the directories of the new root in `build/fs`, makes `/bin`, `/sbin`, `/lib` and `/lib64`
links into `/usr`, makes `/var/run` and `/var/lock` links into `/run`, and copies the repository's
`fs/` overlay over the result. Unlike every other step it has no mark and runs every time the
bootstrap phase runs, so the copy is written to be repeated on a tree that already has packages in
it. This page goes through the script in order and says what each part guarantees.

**Approximate build time:** less than 0.1 SBU

## The directory skeleton

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=17-22 pin=dad701630cf9 -->

These are the directories every later step installs into, or that the running system expects to
find: the kernel's mount points (`dev`, `proc`, `sys`, `run`), `/usr` and its usual
subdirectories, and `/var` with its state, log and temporary directories. `/var/empty` stays empty
and belongs to no package: it is the directory a program names when it needs one that holds
nothing, such as the build context `kdos-appbox` gives podman for a box image.

`/tmp` and `/var/tmp` are mode `1777`, world-writable with the sticky bit, so any account can create
files there and only a file's owner can remove it. `/var/tmp` must be writable by the desktop user
because rootless podman and skopeo stage pulled image layers in it.

## /var/run and /var/lock

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=37-46 pin=071a99762a4a -->

`/var/run` is a link to `../run`, and `/var/lock` to `../run/lock`. Libraries built with upstream's
defaults still have the older path compiled in: basu, the sd-bus library KDOS uses, defaults to
`/var/run/dbus/system_bus_socket`, while `dbus-daemon` listens on `/run/dbus/system_bus_socket`.
With the link, both names reach the same socket, and every client of the system bus finds it. Were
`/var/run` a real directory, every such client would report the bus unreachable while the daemon
was running.

`ln -sfT` refuses to replace a directory, so a real `/var/run` or `/var/lock` left in the tree is
removed first. `build/fs` is a build tree, never a running system, so no runtime state is lost by
that.

## Merged /usr

The script changes into `build/fs` for these four links, so each target is relative to the root:

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=57-60 pin=9eb925b3be84 -->

The four top-level names are links into `/usr`, so there is one `bin`, one `sbin` and one library
directory, and a path such as `/bin/sh` or `/lib/ld-musl-x86_64.so.1` resolves to the file under
`/usr`. `/lib64` points at `usr/lib64`, which the `fs/` copy below makes a link to `lib`.

The `-T` on each `ln` matters on the second run. Without it, `ln` dereferences the link that is
already there and creates the new one inside its target, as `/usr/bin/bin` or `/usr/lib/lib`. Such a
link dangles, no package owns it, and the orphan sweep, which works from kpkg's database, steps
over it, so it would ship. A second run is ordinary: every pass of the bootstrap phase runs this
step, and a narrow build runs it alone to bring a change under `fs/` into the tree.

## Copying fs/

The rest of the script copies `fs/` into the root. `fs/` holds the files KDOS adds to the system
directly, configuration and scripts that belong to no port; [The fs/ overlay](../01-preparing/fs-overlay.md)
describes what is in it. A plain `cp -r` would be enough on an empty tree. On a tree with packages
in it, it would delete accounts, keep files `fs/` has dropped, and keep stale modes, so the copy is
surrounded by the work below, in the order the script does it.

### Accounts survive the copy

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=72-82 pin=202e46a424dd -->

Install hooks of the package phases add service accounts, such as `polkitd` and `geoclue`, to
`/etc/passwd`, `/etc/group` and `/etc/shadow` long after this step first runs. Copying `fs/` over
them would delete those accounts. Before the copy, `awk` saves every line of the tree's file whose
name is not in `fs/`'s file, and the lines are appended back after it. Entries in `fs/` win;
entries that exist only in the tree survive.

### Files gone from fs/ are removed

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=94-104 pin=ccd12b012d65 -->

A copy overwrites but never deletes, so a file removed from `fs/` would stay in the tree for good.
The step keeps a [manifest]{.idx} of what `fs/` provided, in `/var/lib/kdos/fs-manifest`, and on the next
run deletes every path in the old manifest that `fs/` does not provide now. Only regular files and
links are removed, and only ones the manifest put there: the manifest is rewritten after the copy,
and a path a package later installs over belongs to that package. A link that is in `fs/` counts as
provided even when its target resolves to nothing inside `fs/`, as `etc/mtab`'s, which points into
`/proc`, and `usr/lib64`'s do.

### The copy

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=112-119 pin=77680c6ebbee -->

`fs/usr/lib64` is a link to `lib`. `cp` cannot replace a real directory with a link, so on a tree
whose `usr/lib64` is still a directory that one path fails to copy. The step reports the directory
before the copy and lets the copy fail without stopping, and the link reaches the tree when it is
next built from empty. That allowance covers the whole `cp`, so on such a tree any other copy
failure passes too. On a tree where `usr/lib64` is already the link, any copy failure stops the
build.

### Modes follow fs/

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=128-136 pin=c16a1118216e -->

`cp` keeps the mode of a file that already exists at the destination, so without this loop a file
made executable in `fs/` would stay non-executable in a tree synced before. That breaks things
quietly: the boot script `rcS` skips any service script that is not executable. The loop sets every
regular file from `fs/` to 755 when it is executable in the repository and 644 otherwise, which is
what `cp` gives a new file under this build's umask, so a first copy and a later one agree.

### Modes and owners git cannot carry

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=171-193 pin=ad17f2fc6f55 -->

git records whether a file is executable and nothing else: no narrower mode and no owner. The loop
above therefore gives every non-executable file 644, and a file in the tree belongs to whoever ran
the build. For three paths that is a hole:

- `/etc/shadow` at 644 would let every account read every password hash. It is 600 and owned by
  root. `kdos-checkpass`, the setuid helper the lock screen calls, exists so that nothing else opens
  this file, and a readable shadow file would undo it.
- [`/etc/polkit-1/rules.d`]{.idx} and `50-kdos.rules` in it are root's. polkitd reads every rule it finds
  there without checking ownership, so a directory the desktop user could write would let that user
  grant themselves anything.
- `/etc/udev/rules.d/` and everything under it is root's, directories 755 and files 644. udevd runs
  every `RUN+=` of every rule as root, with no ownership check, so a writable rules directory would
  be root for the desktop user on the next device event. The trailing `/` means the directory and
  all it holds, because ports install rules there long after this step runs.

A path in the list that the tree does not have is reported rather than skipped silently.

### The manifest and the accounts are written

<!-- kfs:include script/phases/10_bootstrap/000_file_system.sh lines=195-211 pin=e4300e9cfca0 -->

The manifest lists every file and link in `fs/`, sorted. It is built with POSIX `find` and `sed`
because the step runs on the container's userland, not the tree's. The saved account lines are
appended, the step prints how many it kept and whose they are, and the temporary directory goes.

## Running it on its own

This step is how a change under `fs/` reaches `build/fs`, on a tree at any stage of the build. The
narrow form runs it and then repacks the image:

```sh
make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"
```

The step has no mark to consult, so naming it is enough; `KDOS_REPLAY` makes no difference to it.
No later phase repeats this work, though `70_image` builds the home directories from the
`/etc/skel` this step copies; see
[Syncing fs/](../../kdos/05-developer/build-system.md#syncing-fs).

## See also

- [Part III](intro.md): the bootstrap, its marks and its steps
- [The fs/ overlay](../01-preparing/fs-overlay.md): what `fs/` holds
- [Mount points that must be right](../../kdos/03-architecture/boot-and-init.md#mount-points-that-must-be-right): `/tmp` and `/var/run` on the running system
- [Syncing fs/](../../kdos/05-developer/build-system.md#syncing-fs): the same rules from the build system's side
- [Packaging](../../kdos/03-architecture/packaging.md#who-owns-a-file): why an unowned path is adopted by the package that installs it

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 8.* Previous: [Part III. The bootstrap](intro.md) · [Contents](../README.md) · Next: [9. Linux API headers](010-linux-headers.md)
