# Backups

KDOS backs up with [restic]{.idx}: deduplicated, encrypted [snapshots]{.idx term="snapshot (restic)"}
in a repository on a local path or a remote one. There are two ways to drive it. **Kup**, the
**Backups** page of System Settings, is the [backup]{.idx} a person sets up with the mouse: plans
that run on a timer, after a set amount of use or when a drive is plugged in, with notifications
and a tray applet. **`kdos backup run`** is the backup written in a file, which a
[timer line]{.idx} can run nightly. Both may share one repository. This page shows the ports
involved, the files and the timer line, and the tasks: setting a repository up, backing up to a
USB disk, restoring, pruning, backing up the system's own files as root, and imaging a whole
filesystem. The KDOS book's [Backups](../../kdos/02-user-guide/administration.md#backups) is the
user guide's account; this page gives the procedures around it.

## Installation

- [restic](#port-restic) is the backup program.
- [kup](#port-kup) is the scheduler `kup-daemon`, the Backups page and the tray applet. KDOS
  carries a patch that adds restic as a plan type beside `rsync` and `bup`, and its `restic:` KIO
  worker shows a repository in Dolphin as one folder per snapshot.
- `kdos backup run`, from [kdos-tools](#port-kdos-tools), makes one snapshot as
  `~/.config/kdos/backup.conf` says. It does nothing restic does not.

Nothing backs up on the image as shipped. No service script is involved: Kup runs in the session,
and the nightly line is in your own timer table, run under your user services from your first
login to your last, and commented out in the copy every new home receives; see
[Timers](timers.md).

Other ports that copy data:

- [rclone](#port-rclone) syncs to and from remote storage, and is also what restic's `rclone:`
  repositories run through.
- [rsync](#port-rsync) copies a directory tree, locally or over SSH, built with ACL and extended
  attribute support.
- [partclone](#port-partclone) images a filesystem by its used blocks, and
  [ddrescue](#port-ddrescue) images a failing disk; see [Disk health](disk-health.md).
- [deja-dup](#port-deja-dup), Déjà Dup, is GNOME's graphical backup program over restic.

## Configuration

**`~/.config/kdos/backup.conf`**, copied into each new home from
`fs/etc/skel/.config/kdos/backup.conf`. It is parsed, never sourced, and every line ships commented
out:

<!-- kfs:include fs/etc/skel/.config/kdos/backup.conf -->

`repo`, `include`, `exclude` and `password-file` are the keys, described in
[`~/.config/kdos/backup.conf`](../../kdos/06-reference/configuration.md#configkdosbackupconf).
The paths become separate arguments, so a directory with a space in its name is one path. No
shell reads them either, so `~` and `$HOME` are not expanded: write each path in full. `#` starts
a comment anywhere on a line, so a path containing one is cut short there. `include` and `exclude`
may each appear eight times; a ninth line of either is ignored without a message.

**`~/.config/kdos/backup.pass`** is not shipped. It holds the repository's password and nothing
else, and must be mode 0600: `kdos backup run` refuses to run while any group or other permission
bit is set on it. A different file can be named with `password-file`.

**`~/.config/kdos/timers.d/10-backup.timer`**, copied from
`fs/etc/skel/.config/kdos/timers.d/10-backup.timer`. Its one line, commented out, runs
`kdos backup run` at 03:40 with eight hours' slack:

<!-- kfs:include fs/etc/skel/.config/kdos/timers.d/10-backup.timer -->

**What `kdos backup run` does**, from `src/system/kdos-tools/backup.c`. It reads the two files,
refuses before starting restic if either is missing or unsafe, runs one `restic backup` with every
`include` and `exclude` as its own argument and the tag `kdos`, and waits for it. Kup's own pruning
forgets only the snapshots it tagged `kup`, so the two never remove each other's. A refusal is
printed as `kdos backup: …`; otherwise restic's own output and exit status are passed through: 10
for a repository that does not exist, 11 for one another restic has locked, 12 for a wrong
password, 3 for a snapshot made with some files unread; 1 is a refusal, 127 no restic.

## Usage

On the live medium your home directory is in RAM unless the medium has a persistence store, so
both the configuration and a repository under your home last only as long as the session; keep
the repository on another disk. See [`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist).

### Setting up a plan in Kup

1. System Settings → **Backups**, turn backups on, and **Add New Plan**.
2. Choose **Restic** as the type, the folders to include and exclude, the destination (a folder,
   or an external drive, which Kup recognises when it is plugged in), the schedule and the
   repository's password.
3. **Apply**. Kup runs the plan when it is due and shows its state in the tray applet.

### Setting up kdos backup run

1. Create the repository in a directory you can write. A repository on the disk it protects is
   lost with that disk, so keep it on another one where you can; see
   [Backing up to a USB disk](#backing-up-to-a-usb-disk). This example uses `/srv/backup`; the
   image has no `/srv` and only root may create one, so the first command makes the directory as
   root and gives it to you. restic asks for a new password twice and stores it nowhere:

   ```bash
   sudo install -d -o "$USER" /srv/backup
   restic -r /srv/backup/restic init
   ```

2. Put that password, alone, in the password file and make it private:

   ```bash
   printf '%s\n' 'the password' > ~/.config/kdos/backup.pass
   chmod 600 ~/.config/kdos/backup.pass
   ```

   Typed this way, the password lands in your shell's history; an editor avoids that.

3. Edit `~/.config/kdos/backup.conf`: uncomment `repo` and set it to the repository, and name each
   directory to back up with `include` and each to leave out with `exclude`.

4. Run one backup and read its exit status:

   ```bash
   kdos backup run
   echo $?
   ```

   `0` is a snapshot made, after restic's summary of what it read and added.

The first backup of a home directory reads every file and can take hours; later ones send only
what changed.

### Backing up to a USB disk

1. Give the disk's filesystem a label, `backup` for example, so it always mounts at the same path;
   see [Filesystems and fstab](filesystems-and-fstab.md#labelling-a-filesystem).
2. Mount it from the Device Notifier or Dolphin, or with `udisksctl mount -b /dev/sdX1`. It is
   mounted at `/run/media/<user>/backup`.

3. Create the repository there with `restic -r /run/media/$USER/backup/restic init` and set `repo`
   in `backup.conf` to that path.

A Kup plan whose destination is an external drive runs when the drive is plugged in, and Kup
mounts it itself. A `kdos backup run` while the disk is not mounted fails at restic with status 10,
so a nightly job to a USB disk needs the disk mounted before the slot.

### Backing up every night

1. Uncomment the `backup` line in `~/.config/kdos/timers.d/10-backup.timer`.
2. `service --user restart timers` reads the table again.

Your timers run under your user services, from your first login to your last, so a nightly job
runs only while you have a login, unless you linger (`loginctl enable-linger`). The job's
standard error goes to `$XDG_RUNTIME_DIR/ksvc/kdos-svc.timers.log` and syslog. Check the date of
the newest snapshot from time to time, or run `kdos backup run` by hand to read restic's message. `snooze -n -H 3 -M 40 true` prints
the next times the line would run, one a line, such as
`2026-10-22T03:40:00+0000 Thu  0d  7h 26m 51s`. A backup that must run whether or not anyone is logged in
belongs in the system's table; see [Backing up the system's own files](#backing-up-the-systems-own-files).

### Restoring a file or folder

1. In Dolphin, open the repository as `restic:/` followed by its path, or click a Kup plan's
   **Show Files** in the tray applet. Each snapshot is a folder.
2. Browse into the snapshot and copy the file or folder out, or drag it to where you want it.
   Restore beside the original rather than over it, and compare the two.

From a terminal, the same restore of one directory from the newest snapshot is:

```bash
restic -r /srv/backup/restic --password-file ~/.config/kdos/backup.pass \
    restore latest --target ~/Restored/latest --include /home/kdos/Documents
```

The files land under the same full path, `~/Restored/latest/home/kdos/Documents/…`.
`restic help restore` lists the options.

### Checking and pruning the repository

`kdos backup run` never deletes a snapshot, so a repository it alone writes only grows. Kup prunes
its own `kup` snapshots by its plan's settings. With restic directly:

```bash
R="-r /srv/backup/restic --password-file $HOME/.config/kdos/backup.pass"
restic $R snapshots                                          # what is there
restic $R check                                              # the repository's own consistency
restic $R forget --tag kdos --keep-daily 7 --keep-weekly 5 --keep-monthly 12 --prune
```

`forget` removes the `kdos` snapshots outside the policy, and `--prune` frees the space they
alone used; leaving `--tag` out applies the policy to Kup's snapshots too.

### Backing up the system's own files

`kdos backup run` and Kup run as you and read what you can read, which leaves out the files in `/etc` that
only root may read, such as `/etc/shadow` and `/etc/sudoers`, and every other account's files. A
backup of the system's own files runs as root, from the system's timer table. The image ships no such job; this is an example of one. Make the repository and its
password file first, as root, then add a file such as [`/etc/kdos/timers.d/40-etc-backup.timer`]{.idx}:

```text
etc-backup  -H 2 -M 50 -s 8h  --  restic -r /var/backup/etc --password-file /root/.config/restic.pass --no-cache backup /etc
```

The line is a list of words, not a shell command; `--no-cache` keeps restic from needing a home
directory for its cache. Start it with `sudo service restart timers`; it runs as the service
`kdos-timer-etc-backup`, which `ksvc check kdos-timer-etc-backup` reports on
(`[ OK ] kdos-timer-etc-backup  (pid …)` while `snooze` waits for the slot). The format of the
line is in [The line](timers.md#the-line).

### Imaging a whole filesystem

For an unmounted filesystem, `partclone` copies only the blocks in use, so the image is the size
of the data rather than of the partition:

```bash
sudo partclone.ext4 -c -s /dev/sdb1 -o /srv/backup/sdb1.img     # back up
sudo partclone.ext4 -r -s /srv/backup/sdb1.img -o /dev/sdb1     # restore over the partition
```

Each filesystem has its own program: `partclone.btrfs`, `partclone.xfs`, `partclone.vfat` and the
rest. The root cannot be imaged while it is mounted; boot the live medium to image it.

## Tips

- Test a restore once, before you need one: copy a single folder out of a snapshot and look at
  it.
- Keep a copy of the repository's password somewhere other than the machine it protects. Without
  it, the repository cannot be read.
- On an encrypted root, keep a copy of the LUKS header too; see
  [Encrypted root](encrypted-root.md#keeping-a-copy-of-the-header).
- Exclude large caches and the container store, as the commented lines in `backup.conf` show, or
  the first backup copies them.
- A second root slot lets an update be rolled back; it is not a copy of your files. See
  [A/B slots and rollback](ab-slots-and-rollback.md).
- `kdos clone` copies the live medium itself to another stick; see
  [`kdos clone`](../../kdos/04-programs/kdos-command.md#kdos-clone).

## Troubleshooting

**`kdos backup: … no such file — name a repository and what goes in it`.** `backup.conf` is
missing or empty; copy it back from `/etc/skel/.config/kdos/backup.conf`.

**`kdos backup: … names no repo`.** `repo` is still commented out in `~/.config/kdos/backup.conf`.
Uncomment it.

**`kdos backup: … names no include path`.** No `include` line is uncommented. The password file is
checked first, so this message appears only once the password file is right.

**`no password file`, or `… is mode 0644 — it must be 0600`.** The password file is missing, or
carries a group or other permission bit. Create it, or run `chmod 600 ~/.config/kdos/backup.pass`,
and check that the file named by `password-file`, if set, is the one you mean.

**A Kup plan says it failed.** The tray applet's notification names restic's error; **Show Log
File** in the plan's menu shows the whole run.

**`kdos backup run` exits 10, after restic's `Fatal: repository does not exist: unable to
open config file`.** The path in `repo` is not a restic repository: a USB disk that is not
mounted, a mistyped path, or a repository never made with `restic init`.

**`kdos backup run` exits 12, after restic's `Fatal: wrong password or no key found`.** The
password file does not hold the repository's password. The file must hold the password and nothing
else.

**`kdos backup run` exits 11.** Another restic holds the repository's lock: a backup or
restore still running, or one that was killed. When none is running, `restic -r <repo>
--password-file ~/.config/kdos/backup.pass unlock` removes a stale lock.

**The nightly backup never seems to run.** Check that the line in
`~/.config/kdos/timers.d/10-backup.timer` is uncommented, that you have logged in since the machine
booted, that `service --user status timers` runs, and that `snooze -n` accepts its timespec; a user
timer line that does not parse is skipped silently.
See [Timers](timers.md).

## Known gaps

- [The native applications have been opened, not used](../../kdos/06-reference/known-gaps.md#the-native-applications-have-been-opened-not-used),
  Déjà Dup among them
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  Déjà Dup cannot restore a Duplicity backup
- [A login that typed no password leaves the wallet locked](../../kdos/06-reference/known-gaps.md#a-login-that-typed-no-password-leaves-the-wallet-locked):
  Déjà Dup keeps its passphrase through the Secret Service

## See also

- [Backups](../../kdos/02-user-guide/administration.md#backups): Kup and `kdos backup run` in the user guide
- [`~/.config/kdos/backup.conf`](../../kdos/06-reference/configuration.md#configkdosbackupconf): the keys
- [Periodic jobs](../../kdos/02-user-guide/administration.md#periodic-jobs) and [Your own jobs](../../kdos/02-user-guide/administration.md#your-own-jobs): the two timer tables
- [Backups](../../kdos/04-programs/kdos-settings.md#backups): System Settings' Backups page and its
  restic backend
- [Timers](timers.md), [Storage and removable media](storage-and-removable-media.md), [Disk health](disk-health.md), [Encrypted root](encrypted-root.md), [A/B slots and rollback](ab-slots-and-rollback.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 403.* Previous: [402. Disk health](disk-health.md) · [Contents](../README.md) · Next: [404. sudo, PAM and polkit](sudo-pam-polkit.md)
