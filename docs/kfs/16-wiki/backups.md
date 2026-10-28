# Backups

KDOS backs up with [restic]{.idx}, through [`kdos-backup`]{.idx}, a small window that lists the [snapshots]{.idx term="snapshot (restic)"}
in your repository, starts a [backup]{.idx} and restores a file or folder from one. This page shows the
ports involved, the two files you write and the [timer line]{.idx} that runs it nightly, and the tasks:
setting a repository up, backing up to a USB disk, restoring, pruning, backing up the system's own
files as root, and imaging a whole filesystem. The KDOS book's
[Backups](../../kdos/02-user-guide/administration.md#backups) is the user guide's account of the
same program; this page gives the procedures around it.

## Installation

[restic](#port-restic) is the backup program: deduplicated, encrypted snapshots in a repository on
a local path or on a remote one. `kdos-backup` is part of [kdos-shell](#port-kdos-shell), opened
from the control centre's *Backup…* row or as `kdos-backup`, and it does nothing restic does not:
it runs `restic snapshots`, `restic backup`, `restic ls` and `restic restore`.

Nothing backs up on the image as shipped. No service script is involved: the nightly job is a line
in your own timer table, which your desktop session starts when you log in, and it is commented
out in the copy every new home receives; see [Timers](timers.md).

Other ports that copy data:

- [rclone](#port-rclone) syncs to and from remote storage, and is also what restic's `rclone:`
  repositories run through.
- [rsync](#port-rsync) copies a directory tree, locally or over SSH, built with ACL and extended
  attribute support.
- [partclone](#port-partclone) images a filesystem by its used blocks, and
  [ddrescue](#port-ddrescue) images a failing disk; see [Disk health](disk-health.md).
- [deja-dup](#port-deja-dup), Déjà Dup, is a graphical backup program that drives an external
  tool; restic is the one of its tools that KDOS carries. It is one of the native applications, in
  `44_apps`. When it is installed, `kdos-desktop-start` starts its
  scheduler, `deja-dup-monitor`, at login.

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
else, and must be mode 0600: `kdos-backup` refuses to run while any group or other permission bit
is set on it. A different file can be named with `password-file`.

**`~/.config/kdos/timers.d/10-backup.timer`**, copied from
`fs/etc/skel/.config/kdos/timers.d/10-backup.timer`. Its one line, commented out, runs
`kdos-backup --once` at 03:40 with eight hours' slack:

<!-- kfs:include fs/etc/skel/.config/kdos/timers.d/10-backup.timer -->

**What `--once` runs**, from `src/desktop/kdos-shell/backup.c`. It reads the two files, refuses
before starting restic if either is missing or unsafe, and waits for restic to finish:

<!-- kfs:include src/desktop/kdos-shell/backup.c lines=951-998 pin=5ef77426b44c -->

It exits 0 when restic succeeded, 2 for an option it does not know, and 1 for any other failure,
including a missing `repo` or `include`. The checks run in that order: that `backup.conf` exists,
then `repo`, then the password file, then `include`. A refusal before restic starts is printed.
restic runs with `kdos-backup`'s own standard output and standard error, so its summary and its errors are printed where `--once` was run, and a failure inside restic adds
`kdos-backup: restic exited N` with restic's status: 10 for a repository that does not exist, 11
for a repository another restic has locked, 12 for a wrong password, 3 for a snapshot made with
some files unread.

## Usage

On the live medium your home directory is in RAM unless the medium has a persistence store, so
both the configuration and a repository under your home last only as long as the session; keep
the repository on another disk. See [`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist).

### Setting up kdos-backup

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
   kdos-backup --once
   echo $?
   ```

   `0` is a snapshot made, after restic's summary of what it read and added. `1` follows a
   message: `kdos-backup`'s own refusal, or restic's error and then `kdos-backup: restic exited N`.

5. Open `kdos-backup`. The new snapshot is at the top of the list, newest first.

The first backup of a home directory reads every file and can take hours; later ones send only
what changed.

### Backing up to a USB disk

1. Give the disk's filesystem a label, `backup` for example, so it always mounts at the same path;
   see [Filesystems and fstab](filesystems-and-fstab.md#labelling-a-filesystem).
2. Mount it as yourself:

   ```bash
   kdos-mount list
   kdos-mount mount 0
   ```

   `list` numbers the disks in its first column; give `mount` the disk's number. It is mounted at
   `/media/<user>/backup`, and the filesystem's top directory is made yours.

3. Create the repository there with `restic -r /media/$USER/backup/restic init` and set `repo` in
   `backup.conf` to that path.

A backup run while the disk is not plugged in fails at restic and exits 1. Mounting is a request
to `kdos-mountd`, which the timer does not make, so a nightly job to a USB disk needs the disk
mounted before the slot.

### Backing up every night

1. Uncomment the `backup` line in `~/.config/kdos/timers.d/10-backup.timer`.
2. Log out and in again; the session starts your timers at login.

Your timers are the session's own background loops, not a supervised service. Log Out does not
stop them; they end with the login, and the next login starts them again, so a nightly job runs
only while somebody is logged in. The job's output is discarded, so a failing nightly backup prints nothing anywhere. Check the date of the newest snapshot in `kdos-backup` from time to
time, or run `kdos-backup --once` by hand to read restic's message. `snooze -n -H 3 -M 40 true` prints
the next times the line would run, one a line, such as
`2026-10-22T03:40:00+0000 Thu  0d  7h 26m 51s`. A backup that must run whether or not anyone is logged in
belongs in the system's table; see [Backing up the system's own files](#backing-up-the-systems-own-files).

### Restoring a file or folder

1. Open `kdos-backup`, select a snapshot and press Enter. Its folders are listed one level at a
   time.
2. Enter a folder with Enter, go back up with Backspace.
3. Select the file or folder to restore and press `t`.

It is restored into `~/Restored/<snapshot>/`, with its full original path recreated beneath that,
and never over the original; compare the two and move what you want. restic's last line shows the
progress, and closing the window does not stop the restore.

From a terminal, the same restore of one directory from the newest snapshot is:

```bash
restic -r /srv/backup/restic --password-file ~/.config/kdos/backup.pass \
    restore latest --target ~/Restored/latest --include /home/kdos/Documents
```

The files land under the same full path, `~/Restored/latest/home/kdos/Documents/…`.
`restic help restore` lists the options.

### Checking and pruning the repository

`kdos-backup` never deletes a snapshot, so the repository only grows. With restic directly:

```bash
R="-r /srv/backup/restic --password-file $HOME/.config/kdos/backup.pass"
restic $R snapshots                                          # what is there
restic $R check                                              # the repository's own consistency
restic $R forget --keep-daily 7 --keep-weekly 5 --keep-monthly 12 --prune
```

`forget` removes the snapshots outside the policy, and `--prune` frees the space they alone used.

### Backing up the system's own files

`kdos-backup` runs as you and reads what you can read, which leaves out the files in `/etc` that
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

- Test a restore once, before you need one: restore a single folder into `~/Restored/` and look
  at it.
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

**`no ~/.config/kdos/backup.conf — name a repository and what goes in it`** (with your home
written out). The file is missing or empty; copy it back from `/etc/skel/.config/kdos/backup.conf`.

**`kdos-backup` says ``backup.conf names no `repo` ``, or `--once` prints `backup.conf names no repo`.**
`repo` is still commented out in `~/.config/kdos/backup.conf`. Uncomment it.

**`backup.conf names no include path`.** No `include` line is uncommented. `--once` checks the
password file first, so this message appears only once the password file is right.

**`no password file at …`, or `… is mode 0644 — it must be 0600`.** The password file is missing,
or carries a group or other permission bit. Create it, or run `chmod 600 ~/.config/kdos/backup.pass`,
and check that the file named by `password-file`, if set, is the one you mean.

**The window shows a line from restic instead of the snapshot list.** When restic refuses to list
the snapshots, the window shows the first line restic wrote to its error output, such as
`Fatal: repository does not exist: unable to open config file` or `Fatal: wrong password or no
key found`. Look for that line in the entries below; `kdos-backup --once` in a terminal prints the
same message with restic's exit code.

**The window says `restic answered nothing — is <repo> a repository?`.** restic printed nothing at
all, neither a snapshot list nor an error. Check that `restic` is installed and that `repo` names
the path you mean, then run `kdos-backup --once` in a terminal to see how restic exits.

**`--once` prints `restic exited 10`, after restic's `Fatal: repository does not exist: unable to
open config file`.** The path in `repo` is not a restic repository: a USB disk that is not
mounted, a mistyped path, or a repository never made with `restic init`.

**`--once` prints `restic exited 12`, after restic's `Fatal: wrong password or no key found`.** The
password file does not hold the repository's password. The file must hold the password and nothing
else.

**`--once` prints `restic exited 11`.** Another restic holds the repository's lock: a backup or
restore still running, or one that was killed. When none is running, `restic -r <repo>
--password-file ~/.config/kdos/backup.pass unlock` removes a stale lock.

**The nightly backup never seems to run.** Check that the line in
`~/.config/kdos/timers.d/10-backup.timer` is uncommented, that you have logged in since the machine
booted, and that `snooze -n` accepts its timespec; a user timer line that does not parse is skipped silently.
See [Timers](timers.md).

## Known gaps

- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run),
  Déjà Dup among them
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  Déjà Dup cannot restore a Duplicity backup
- [A saved login needs KeePassXC running](../../kdos/06-reference/known-gaps.md#a-saved-login-needs-keepassxc-running):
  Déjà Dup keeps its passphrase through the Secret Service

## See also

- [Backups](../../kdos/02-user-guide/administration.md#backups): `kdos-backup` in the user guide
- [`~/.config/kdos/backup.conf`](../../kdos/06-reference/configuration.md#configkdosbackupconf): the keys
- [Periodic jobs](../../kdos/02-user-guide/administration.md#periodic-jobs) and [Your own jobs](../../kdos/02-user-guide/administration.md#your-own-jobs): the two timer tables
- [The system surfaces](../../kdos/04-programs/kdos-shell.md#the-system-surfaces): `kdos-backup`'s keys and restore view
- [Timers](timers.md), [Storage and removable media](storage-and-removable-media.md), [Disk health](disk-health.md), [Encrypted root](encrypted-root.md), [A/B slots and rollback](ab-slots-and-rollback.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 379.* Previous: [378. Disk health](disk-health.md) · [Contents](../README.md) · Next: [380. sudo, PAM and polkit](sudo-pam-polkit.md)
