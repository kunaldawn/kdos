# Creating the homes

`fs/etc/passwd` declares the accounts the image ships, and the one human account, `kdos`, has
its home at `/home/kdos`. git records no owner and no empty directory, and the `fs/` copy runs in
`10_bootstrap`, before any package exists, so the home cannot come from `fs/`.
`script/phases/70_image/070_user.sh` makes it here, from [`/etc/skel`]{.idx}, once every package and the
theme have written their part of the template. It also sets the modes git cannot carry on two
mail files and removes the account files' backups.

## Fixing /etc/skel first

<!-- kfs:include script/phases/70_image/070_user.sh lines=33-35 pin=7063b9f1910c -->

`.msmtprc` and `.mbsyncrc` hold, or will hold, a password or the command that prints one. git
records only the execute bit, so the `fs/` copy lands them at mode 644. msmtp refuses to send
through an `.msmtprc` that has a `password` line and is not mode 600, so both go to 600
here, after every package phase, since a port may install one of these paths as well. aerc's
`accounts.conf` is not shipped: aerc's own wizard writes it at 600 on first run.

<!-- kfs:include script/phases/70_image/070_user.sh lines=43-45 pin=13c08bfb9ae1 -->

The `fs/` manifest removes files `fs/` has dropped but never the directory a removal emptied, and
git cannot carry an empty directory, so an empty directory under [`/etc/skel/.config`]{.idx} is never one
`fs/` asked for. Left alone it would be copied into every home. Only `.config` is pruned: the
generated icon and theme trees have empty directories that belong there.

## The account backups

<!-- kfs:include script/phases/70_image/070_user.sh lines=52-52 pin=42eca05132f9 -->

shadow's tools write [`/etc/passwd-`]{.idx}, `group-`, `shadow-` and `gshadow-` beside each file they
change, and those copies carry the password hashes of the accounts as the build left them. The
image ships none of them.

## Each home

<!-- kfs:include script/phases/70_image/070_user.sh lines=54-63 pin=08765e3b2f18 -->

The loop reads `/etc/passwd` and makes a home for each ordinary account: a numeric UID from 1000 to
65533, with a home directory named. The service accounts, below 1000, and any UID from 65534 up
are skipped. On a KDOS root that is `kdos` alone.

<!-- kfs:include script/phases/70_image/070_user.sh lines=64-75 pin=7ea86e8af715 -->

The template wins over what the home holds. `cp -r` overwrites, so an edit to `fs/etc/skel` reaches
the home on the next image build; a copy that kept existing files would ship the previous build's
dotfiles instead. `cp` never deletes, though, so before the copy the step removes
`.local/share/applications`: the boxed-application launchers [030_launchers.sh](030-launchers.md)
reconciled in `/etc/skel`, where a launcher whose file name changes would otherwise stay under
its old name too, and the menu would list the application twice. Anything else edited by hand in
`build/fs/home/kdos` survives only where the template has no file of the same name.

<!-- kfs:include script/phases/70_image/070_user.sh lines=92-97 pin=340559f9943d -->

git cannot carry these directories either, so they are made here. Each is a directory a program
expects and will not create before its first use: the folders `/etc/xdg/user-dirs.defaults` lists,
which the first Plasma login's `xdg-user-dirs-update` records in `~/.config/user-dirs.dirs` and
Dolphin's Places shows from the start; `Pictures/Screenshots`, where Spectacle saves; `~/Mail`,
which notmuch and mbsync both name; `.local/share/calendars/*`, which khal's shipped configuration
reads; and `.local/share/contacts/personal`, which khard's names.

<!-- kfs:include script/phases/70_image/070_user.sh lines=102-108 pin=02825bae40ce -->

`cp` gives a new file the template's mode but leaves the mode of an existing one alone, so the two
mail files are set to 600 in the home as well. The whole home is then given to the account's UID
and GID, and the home directory itself is mode 0700.

## Why it runs here

It runs after [030_launchers.sh](030-launchers.md) and [050_theme.sh](050-theme.md), which write
into `/etc/skel`. Anything written into `/etc/skel` after this step reaches no home on the image.

## What it leaves

On the image, `/home/kdos`, owned by `kdos` (1000:1000) and mode 0700, holding a copy of
`/etc/skel` and the empty directories above; [`/etc/skel/.msmtprc`]{.idx} and `.mbsyncrc` at 600; and no
`/etc/passwd-`, `group-`, `shadow-` or `gshadow-`.

## See also

- [Syncing fs/](../../kdos/05-developer/build-system.md#syncing-fs): the `fs/` copy and the trees `/etc/skel` owns
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): accounts on the running system
- [Modes and owners git cannot carry](../03-bootstrap/000-file-system.md#modes-and-owners-git-cannot-carry): what the bootstrap sets instead

<!-- book-nav -->
---

*Part XV. The image, chapter 372.* Previous: [371. The udev hardware database](060-udev-hwdb.md) · [Contents](../README.md) · Next: [373. The manual index](080-whatis.md)
