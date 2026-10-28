# Findutils

GNU findutils provides `find`, which walks a directory tree and acts on what it finds, and `xargs`,
which turns a list into command lines. Build systems and install scripts use both. The source also
carries [`locate`]{.idx} and [`updatedb`]{.idx}, which KDOS takes from plocate instead, so this step deletes them
after the install. This page shows the commands of `script/phases/10_bootstrap/064_findutils.sh`
and explains them.

**Source:** the archive of the [findutils](#port-findutils) port\
**Installs into:** `build/fs/usr`\
**Approximate build time:** 0.5 SBU

## Installation of findutils

<!-- kfs:include script/phases/10_bootstrap/064_findutils.sh lines=23-44 pin=d9dbb9187d61 -->

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`, `--prefix=/usr`
: A cross build, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--localstatedir=/var/lib/locate`
: Where `updatedb` would keep its database. The `locate` half is deleted below, so the only trace
  of this switch is the empty directory, which the `rmdir` removes.

`--disable-nls`
: Build no message catalogues.

### Removing locate

`locate`, `updatedb` and their helper `frcode` are deleted with their manual pages. The
[plocate](#port-plocate) port provides `/usr/bin/locate` and `/usr/sbin/updatedb`. The findutils
port leaves its own copies out of its package, so a file installed here would be adopted by no
package (see [What the bootstrap leaves behind](intro.md#what-the-bootstrap-leaves-behind)).
plocate's `locate` would take over a `/usr/bin/locate` left here, but nothing would ever replace
`/usr/bin/updatedb`, `frcode` or the manual pages, and the profile's `PATH` lists `/usr/bin` before
`/usr/sbin`, so findutils' `updatedb` would run in place of plocate's.

`rmdir` removes `/var/lib/locate` only when it is empty, and its `|| true` keeps a missing or
non-empty directory from stopping the step.

## Contents of findutils

**Installed programs:** [`find`]{.idx} and [`xargs`]{.idx}.

The [findutils](#port-findutils) port's page describes each program.

## Built again later

The [findutils](#port-findutils) port, in `30_foundation`, installs the same two programs, and kpkg
adopts the files this step installed. It differs in three ways:

- It applies `updatedb-copyright-pipe.patch`. The rule that generates `updatedb` means to pipe
  [`sed`]{.idx}'s output into `grep`, but as released it hands `grep` and `.` to `sed` as file names; the
  patch adds the missing `|`.
- It adds `--without-selinux`.
- It deletes the same `locate` files from its package, and with them the directories they leave
  empty: `/usr/libexec`, `/usr/share/man/man5` and `/var`.

## See also

- [Part III](intro.md): the bootstrap, and [why an unowned file must not be left behind](intro.md#what-the-bootstrap-leaves-behind)
- [Finding a file by name](../../kdos/02-user-guide/administration.md#finding-a-file-by-name): the `locate` and `updatedb` KDOS ships
- [Gawk](065-gawk.md): the next step

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 18.* Previous: [17. Diffutils](063-diffutils.md) · [Contents](../README.md) · Next: [19. Gawk](065-gawk.md)
