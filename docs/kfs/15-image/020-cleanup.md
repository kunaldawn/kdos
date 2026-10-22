# Cleaning up the root

By the time the image phase starts, `build/fs` has been the working root of every package build,
and it holds what those builds left behind: scratch files, caches, built package archives, Python
bytecode written by build tools, a container store, and whatever a tool wrote into root's home.
`script/phases/70_image/020_cleanup.sh` removes all of it, so that the root carries only what a
package or `fs/` put there. It also fails the image when a packaged Python module has no compiled
bytecode. This page goes through the script in order.

## The switches

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=26-29 pin=774dca90cca2 -->

Each of the three passes below is guarded by one of these variables, set in the script itself. No
`make` variable reaches them; turning a pass off means editing the line. The removal of podman's
stores and the reset of `/root` have no switch.

The step reports the size of `build/fs` before and after, and the bytes each pass freed, in its
log.

## The build cache

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=47-60 pin=e547544be06f -->

`root/.cache`, `root/.cargo` and `root/.npm` are the caches tools write into root's home; the
contents of `/tmp` and `/var/tmp` are scratch, and `/var/tmp` is also `HOME` for every chroot
command (`HOME=/var/tmp/kdos-home`, set by `script/chroot/exec.sh`). `/var/cache/kpkg/work` is
kpkg's work directory, where each port is unpacked and staged; a build that failed leaves its
directory there. Without this pass all of it would go into `system.sfs`, including the
`podman<pid>` directories an appbox bake leaves in `/var/tmp`, which are multi-gigabyte.

`$target` is deliberately unquoted in the `rm`, so that `tmp/*` and `var/tmp/*` expand to the
directories' contents and the directories themselves stay.

## The package cache

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=67-76 pin=c20e8d77290e -->

`/var/cache/kpkg/packages` holds the package archives kpkg keeps: every package of the build when
`KDOS_MAKE_BINHOST=1` (which is why [010_binhost.sh](010-binhost.md) runs first), and the packages
a level build has built and not yet installed. A `.pending` record names a package in this
directory that is waiting to be committed, so the two are removed together. A record left without
its package would read as a build to commit that cannot be found.

## Python bytecode

kpkg compiles every Python module a package ships into checked-hash bytecode inside the package,
so each `.pyc` is owned by the package database and an upgrade or removal replaces it. `/usr` is
read-only on the live medium, so a module with no `.pyc` would be compiled again on every import.
The pass is one embedded Python program, run only when [`python3`]{.idx} is installed. Its first half
reads every package's file list and removes the bytecode no package owns:

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=104-151 pin=26318ff72863 -->

`canon` folds the merged-`/usr` aliases, because a package may list `./lib/x` for the file that is
on disk at `usr/lib/x`. The bytecode no package owns is what build tools wrote while importing
modules in the chroot, with the build's time in it; no package would ever remove it. A
`__pycache__` directory left empty by the removal goes too.

The second half is the check:

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=153-190 pin=d693c124dc63 -->

Every `.py` a package owns under `/usr` or `/opt` must have bytecode for this interpreter at the
path `importlib.util.cache_from_source` gives. Four kinds of file are exempt:

- scripts in `/usr/bin` and `/usr/sbin`, which are run and never imported;
- `*.tmpl.py` templates;
- gcc's libstdc++ pretty printers under `/usr/share/gcc-*/python` and gdb's auto-load scripts:
  gcc is built in `20_selfhost` and `30_foundation`, before `python3` exists, so kpkg has nothing
  to compile them with, and gdb reads them only in a debugging session;
- a file this interpreter cannot compile, such as Python 2 source, which is listed in the log and
  passes, because there is no bytecode for it to have.

Any other module without bytecode stops the phase, and the log lists each path. The package that
owns it was built by a kpkg that does not compile bytecode, or compiled it under another
interpreter. See
[Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image).

## podman's stores

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=201-215 pin=9abf4339ed4b -->

A shipped root carries no container store. An application's box is built on the machine that asks
for it, in that user's home, so images in `/home/kdos/.local/share/containers` or in the root
store `/var/lib/containers/storage` are something a build pulled, and would be copied onto every
installed system. No package owns these paths, so they are removed with `rm` and not with
`kpkgdel`.

## root's home

<!-- kfs:include script/phases/70_image/020_cleanup.sh lines=225-232 pin=5d44f628e809 -->

`/root` is replaced with exactly what `fs/root` holds, with no file writable by group or others
and the directory itself at mode 0700. The chroot's `HOME` is a scratch directory, but a tool that
names `/root` itself still writes there: a CMake package registry pointing into kpkg's work
directories, which a later `find_package` run as root would consult, git's `safe.directory`, logs
and telemetry dated by the build. The installer copies the live root onto the disk, so each of
these would reach every installed system.

## What it leaves

The removals are all in `build/fs`, and so in `system.sfs`. `/root` on the image is `fs/root`.
Nothing is written outside the root.

## See also

- [The packaging steps](../../kdos/05-developer/build-system.md#the-packaging-steps): the step among the others
- [The package lint](../../kdos/03-architecture/packaging.md#the-package-lint): what kpkg checks in a package before it is installed
- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md#package): where the bytecode is compiled into a package

<!-- book-nav -->
---

*Part XV. The image, chapter 343.* Previous: [342. The package store](015-pkgstore.md) · [Contents](../README.md) · Next: [344. The application launchers](030-launchers.md)
