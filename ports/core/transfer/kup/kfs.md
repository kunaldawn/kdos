## About

Kup is Plasma's backup scheduler. `kup-daemon` runs each backup plan by hand, on a timer or after a
set amount of active use, a plan on an external drive once the drive is connected and a backup is
due, and reports through notifications
and a panel applet; the Backups page of System Settings edits the plans. A plan copies with
`rsync`, keeping one mirror, with `bup`, keeping every version, or with
[restic](#port-restic), keeping every version encrypted. The `restic:` KIO worker shows a
restic repository in Dolphin as one folder per snapshot, and `kup-filedigger` and the `bup:`
worker browse bup archives.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`restic-backend.patch` adds restic as a third backup type, written for upstream and to be offered
as a merge request against `system/kup`. A restic plan initialises an empty destination,
optionally checks the repository, saves a snapshot tagged `kup` and, when asked, removes old ones with one fixed policy (every snapshot of two days, then daily for
a month, weekly for six months, monthly for two years, yearly after); only `kup`-tagged snapshots
are ever removed, so those `kdos backup` saves into the same repository are left alone. The
password is kept in KWallet and reaches restic through a pipe at file descriptor 3, never as an
argument or a variable; a plan may name a password file instead, which must be readable by its
owner alone, the same rule `kdos backup` applies to `~/.config/kdos/backup.pass`. The patch also
adds `kio_restic`, which reads with `restic --no-lock`, so it browses read-only media and works
while a backup runs.

The patch gives a restic plan `~/.restic` as its destination where the plan's stored default is
bup's `~/.bup`: a new plan created as restic starts there, and choosing restic by hand moves a
destination still at `~/.bup`. The stored default is upstream's and stays, so every saved plan reads
back unchanged. The password field says "Stored in KDE Wallet" only when the wallet is enabled and
holds the plan's entry; KWallet reports a disabled wallet as holding every key, which on the live
medium, where the wallet is off, would claim a password that is not there. A backup that has no
password file and finds the wallet off fails with "the KDE Wallet is turned off … set a password
file", not with "enter it in the plan's settings": that field cannot store a password until the
wallet is on.

## Notes

Kup runs its engines as programs. [restic](#port-restic) and [rsync](#port-rsync) are in
`depends` for run time. `bup` and `par2` are not ports, so a plan set to bup versioned backups
cannot be chosen, the file digger button is hidden, and the Backups page offers restic for new
plans. `kdos backup` stays the command-line tool for the same repositories.

[kwallet](#port-kwallet) holds the restic passwords, in the network wallet's `Kup` folder, one
entry per repository location; a password changed in the settings page replaces the entry and
is never shown back. The daemon, the settings page and `kio_restic` link it, and the configure
requires it.

[libgit2](#port-libgit2) is linked, not run: a bup repository is a git object store, and the
`bup:` worker and the file digger read it directly.

The daemon starts from `/etc/xdg/autostart/kup-daemon.desktop` at login. The panel applet links
[libplasma](#port-libplasma), which ties the port to the Plasma release it is built against.
