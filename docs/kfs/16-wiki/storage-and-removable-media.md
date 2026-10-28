# Storage and removable media

USB sticks, SD cards, external disks and optical discs are mounted on KDOS by a root daemon,
[kdos-mountd]{.idx}, which the desktop and the [`kdos-mount`]{.idx} command ask by row number; the
natively ported applications reach the same disks through udisks2 instead. This page shows which
ports provide each route, the files that change what the daemon allows, and the tasks: mounting
and ejecting, unlocking an encrypted stick, formatting and partitioning, writing an image and
checking that a stick holds what it claims. Which devices the daemon offers, what it refuses, and
why a client may name only a row, is in the KDOS book's
[kdos-mountd](../../kdos/04-programs/daemons.md#kdos-mountd); this page does not repeat it.

## Installation

[kdos-mountd](#port-kdos-mountd) installs the daemon, `/usr/sbin/kdos-mountd`, and the command-line
client `kdos-mount`, a link to the same binary. Its service script, [`/etc/init.d/58_mountd.sh`]{.idx},
comes from the tree's `fs/` overlay and is enabled on the image, on the live medium and on an
installed system alike: `/etc/service.disabled/` holds no `mountd` marker. The service's name is
`mountd`, from the file name, and the supervised daemon's is `kdos-mountd`, which is what
`service status mountd` prints (`[ OK ] kdos-mountd  (pid …)`). The daemon listens on
`/run/kdos-mountd.sock`, a socket anyone may connect to (mode 0666), and answers root and the
members of `seat` and [`wheel`]{.idx}, by the connecting process's credentials.

[kdos-shell](#port-kdos-shell) provides the desktop's side, all as the user and none privileged:

| Program | Does | Opened by |
|---|---|---|
| [`kdos-devices`]{.idx} | Removable media beside the cameras, microphones and scanners: mount, open, unmount | `Super+F6`, the menu's route `setup.devices`, or the command `kdos-devices` |
| [`kdos-disks`]{.idx} | Mount, unmount, unlock, close, [SMART]{.idx}, partition, format and write an image; not eject | The menu's route `system.disks`, or the command `kdos-disks` |
| `kdos-mediad` | No window. A toast when a device appears, with *Open* and *Eject* | The session, at login |

Other ports that work on [removable media]{.idx}:

- [udisks2](#port-udisks2), with [gvfs](#port-gvfs) above it for GTK, is the route of the natively
  ported applications (Dolphin's device list, GNOME Disks, K3b, the GTK and Qt file choosers).
  `udisksd`, `/usr/lib/udisks2/udisksd`, has no service script; the system bus starts it on the
  first request, from `/usr/share/dbus-1/system-services/org.freedesktop.UDisks2.service`.
- [caligula](#port-caligula) writes a disk image to a stick from a terminal and verifies it.
- [f3](#port-f3) tests whether a stick holds as much as it reports.
- [kdos-tools](#port-kdos-tools) provides `kdos clone`, which copies the KDOS medium itself onto
  another stick.
- [android-file-transfer](#port-android-file-transfer) reaches a phone over MTP, which no block
  device route does. It is one of the native applications, in `44_apps`.

## Configuration

**[`/etc/kdos/mountd.conf`]{.idx}** is not shipped. Create it, as root, to change one of three defaults:

| Line | Without it | With it |
|---|---|---|
| `exec = yes` | Removable media and network shares are mounted `noexec` | Programs on them can run |
| `format = yes` | `kdos-disks` cannot format a device | Formatting is allowed |
| `write = yes` | No disk image can be written through the daemon | Writing an image over a whole removable disk is allowed |

The daemon reads the file on each request that needs it, so no restart is needed. It takes one
`key = value` per line, and blanks around the key, the `=` and the value are ignored, so
`exec=yes` works too. A line whose first non-blank character is `#` is a comment, so
`# exec = yes` leaves the setting off. The key must be exactly `exec`, `format` or `write`, and
the value exactly `yes`; anything else leaves the setting off. When a key appears on more than
one line, the last one wins. See
[`/etc/kdos/mountd.conf`](../../kdos/04-programs/daemons.md#etckdosmountdconf).

**`/etc/init.d/58_mountd.sh`**, from `fs/etc/init.d/58_mountd.sh`, checks that the daemon is
installed and starts it in the foreground under the supervisor:

<!-- kfs:include fs/etc/init.d/58_mountd.sh lines=21-23 pin=744e191b2c87 -->

**`/etc/polkit-1/rules.d/50-kdos.rules`**, from `fs/etc/polkit-1/rules.d/50-kdos.rules`, decides
what udisks2 may do for whom. Members of `wheel` are granted these actions without a password:

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=99-131 pin=91e113259747 -->

For anyone outside `wheel` the rule decides nothing and udisks2's own policy applies. Where that
policy asks for a password the request is refused, because the system has no authentication agent
to ask it. Changing `/etc/fstab` and `/etc/crypttab`, managing RAID arrays, LVM volume groups,
btrfs volumes and swap, and firmware erases are left out of the rule for `wheel` too; the comment
at the rule's end lists them.
See
[What is granted instead: `50-kdos.rules`](../../kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules).

**udisks2's own build**, from `ports/core/disk/udisks2/build.sh`. `--disable-fhs-media` puts its
[mount points]{.idx term="mount point"} under `/run/media/<user>` rather than `/media`:

<!-- kfs:include ports/core/disk/udisks2/build.sh lines=12-32 pin=074d50f6ce10 -->

**`/etc/fstab`.** A device that `/etc/fstab` names by device path or by `LABEL=` is not offered by
`kdos-mountd` at all. A line that names it by `UUID=` is not matched, so that device is still
offered; see [Filesystems and fstab](filesystems-and-fstab.md#tips).

## Usage

The disk the system runs from is never offered, every partition with it: the one mounted at `/`,
`/boot` or `/mnt/iso`, and the persistence stick at `/mnt/persist`. The live medium and an
installed system otherwise behave the same here, with one difference: in a live session, every
disk that carries an ISO 9660 superblock, on the whole disk as the shipped image does or on a
partition, is taken for the boot medium and refused whole, so neither a second data CD nor a
stick holding another system's image can be mounted while running live.

### Mounting a stick from the desktop

1. Plug it in. A toast says `<label> is ready`, with the label or, without one, the kernel name.
2. Press *Open* to mount it and open the folder, or open `kdos-devices` with `Super+F6`, select the
   row under removable media and press Enter, which mounts what is not mounted and opens what is.
3. To unmount, press `u` on the row in `kdos-devices`, or Enter on it in `kdos-disks`.

The device is mounted at `/media/<user>/<label>`, a directory the daemon makes for the mount and
removes again at unmount, and its top directory is yours. On FAT, exFAT and NTFS, which have no
ownership of their own, every file and directory is yours and closed to other accounts (mode
0660 and 0770); ext4 and btrfs keep the ownership stored on them, and only the top directory is
made yours. Every mount is `nosuid,nodev`, and `noexec` unless `exec = yes` is set. A FAT stick,
for example, is mounted
`rw,nosuid,nodev,noexec,relatime,uid=1000,gid=1000,fmask=0117,dmask=0007,…`.

### Mounting from a terminal

1. List the devices the daemon offers:

   ```bash
   kdos-mount list
   ```

   Each row is the index, kernel name, label, filesystem, size and mount point, separated by tabs,
   with `-` for no label or no mount point; a line `ok` ends the list:

   ```text
   0	sda1	STICK	vfat	0.1G	-
   1	sda2	-	crypto_LUKS	0.1G	-
   ok
   ```

2. Mount a row, which prints the mount point, and unmount it again:

   ```bash
   kdos-mount mount 0          # ok /media/kdos/STICK
   kdos-mount unmount 0        # ok /media/kdos/STICK
   ```

   Mounting a row that is already mounted answers with its mount point again.

`kdos-mount` exits 0 whenever the daemon answered, including when the answer is `err …`
(`err no such device` for a row that is not in the list); a script must look for a leading `err`
on standard output. It exits 2 when no daemon answers, and prints its usage for a verb it does not
know. An index that is not a number is taken as row 0.

### Ejecting

Unmount the device first, then press *Eject* on its toast. The daemon powers down the whole disk
the partition is on, or ejects an optical drive's tray; the disk then reads as `0B` in
`lsblk` and leaves the list. Eject is refused while the device the toast names is mounted
(`err unmount it first`), and while anything else on the same disk holds it, such as an unlocked
mapping (`err eject refused by the device`). Neither `kdos-mount` nor `kdos-disks` offers it: once
the toast has gone, unmount and pull the device out, or as root run `sudo eject /dev/sdb` on the
whole disk.

### Unlocking an encrypted stick

1. Open `kdos-disks`. A [LUKS]{.idx} device's TYPE column reads `crypto_LUKS`.
2. Select it, press `u`, type the passphrase and press Enter.
3. A row for the opened mapping, `kdos-<kname>`, appears beside it, with the label and filesystem
   inside the container (`kdos-sda2	secret	ext4`). Select it and press Enter to mount it, at
   `/media/<user>/<label>`.
4. When done, press Enter on the mapping row to unmount it, then `c` on the same row to close the
   mapping. The close is sent for the container's row, which is the one the daemon names the
   mapping from.

The passphrase goes to `cryptsetup` on its standard input and is wiped after the request; see
[Encrypted volumes](../../kdos/04-programs/daemons.md#encrypted-volumes). A wrong passphrase and a
header the installed `cryptsetup` will not open both answer `could not unlock`. The machine's own
encrypted root is a different matter; see [Encrypted root](encrypted-root.md).

### Formatting a stick

From the desktop, with `format = yes` in `/etc/kdos/mountd.conf`:

1. In `kdos-disks`, select the device, which must be unmounted, and press `e`.
2. Type the device's kernel name, `sdb1` for example, and press Enter. Nothing else confirms it.

Without `format = yes` the daemon answers ``format is off; set `format = yes` in
/etc/kdos/mountd.conf``, and a name that is not the row's answers `type sdb1 to confirm`.
`kdos-disks` always formats as ext4, with the label `KDOS`. For another filesystem, format from a
terminal as root, on the unmounted partition:

```bash
sudo mkfs.vfat -F 32 -n STICK /dev/sdb1      # FAT32, for any machine
sudo mkfs.exfat -L stick /dev/sdb1           # exFAT, for files over 4 GiB
```

### Partitioning a stick

In `kdos-disks`, select an unmounted row on the disk and press `p`. It opens [`cfdisk`]{.idx} in a
terminal on the whole disk the row is on, never on the partition. `cfdisk` runs through `sudo`, so
the terminal asks for your password and the account must be in `wheel` (see
[sudo, PAM and polkit](sudo-pam-polkit.md)); the daemon is not involved. A mounted row and an
unlocked mapping's row are refused. From a terminal, the same is `sudo cfdisk /dev/sdb`. `cfdisk`
asks before it writes.

### Writing a disk image to a stick

Every way of writing replaces the whole disk, partition table and all. Choose one:

- **`kdos-disks`**, with `write = yes` in `/etc/kdos/mountd.conf`: select a row on the stick, press
  `w`, type the image's path (or drop the file on the window), then type the disk's name, `sdb`, to
  confirm. The bar reads `writing` and then `verifying`, and ends `written: sdb … verified`.
- **`kdos-mount`**, with the same setting:

  ```bash
  kdos-mount write 0 kdos.iso sdb
  ```

  Without the setting it answers ``err write is off; set `write = yes` in /etc/kdos/mountd.conf``,
  and a disk name that is not the row's disk answers `err type sda to confirm`. While it runs it
  prints `progress write <done> <total>` and then `progress verify <done> <total>` lines, in bytes,
  and it ends `ok sda <bytes> verified` once every byte read back matched.

- **[`caligula`]{.idx}**, in a terminal, which lists only removable disks unless given `--show-all-disks`,
  offers to become root through `sudo` when it cannot open the disk, and verifies what it wrote:

  ```bash
  caligula burn image.iso
  ```

- **`kdos clone`**, to copy the KDOS medium the machine booted from; see
  [`kdos clone`](../../kdos/04-programs/kdos-command.md#kdos-clone).

`kdos-mountd` offers only a disk carrying a filesystem it recognises, so a blank stick cannot be
written through `kdos-disks` or `kdos-mount`, and neither can a stick after an image with no
filesystem the daemon knows was written over it; use `caligula` for one. `kdos-mount write` has
written and verified an image on an emulated USB stick in a virtual machine; none of these routes
has written to a real stick; see [Known gaps](#known-gaps).

### Checking that a stick is genuine

A counterfeit stick reports more space than it has and silently overwrites earlier data. Two ways
to test one:

1. Without losing what is on it, fill its free space and read it back, on the mounted stick:

   ```bash
   f3write /media/$USER/<label>
   f3read  /media/$USER/<label>
   ```

2. Faster, and destroying everything on it, as root on the unmounted whole disk:

   ```bash
   sudo f3probe --destructive --time-ops /dev/sdb
   ```

`f3write` writes at the stick's own sequential speed until the filesystem is full, so on a slow
stick the first way takes as long as filling it does; `f3read` then reports how much came back
`Corrupted`, `Slightly changed` or `Overwritten`. `f3probe` ends with the probe time and the time
each operation took.

`kdos clone` runs `f3probe` itself before it writes, when f3 is installed.

### Using a disk from a native application

Dolphin, GNOME Disks and the file choosers mount through udisks2, under `/run/media/<user>/`, and
only for a member of `wheel`. A disk mounted either way appears as mounted in the other's list,
since both read `/proc/mounts`. The native applications have not been run; see
[Known gaps](#known-gaps).

### A phone

An MTP phone is not a block device, so neither list shows it. Mount its storage as a directory,
which must exist first, and take it away again when done:

```bash
mkdir -p ~/Phone
aft-mtp-mount ~/Phone
fusermount3 -u ~/Phone
```

Or open the Android File Transfer window, which copies with no mount.

## Tips

- Give a backup disk a label. The mount point is named after it, so `/media/<user>/<label>` is the
  same path every time it is plugged in. Every character other than letters, digits, `.`, `_` and
  `-` is dropped from the name, so a label `My Disk` mounts at `/media/<user>/MyDisk`.
- A row number is valid only for the list it came from: the daemon rescans on every request. In a
  script, run `kdos-mount list` again before acting on a row after anything was plugged in or out.
- `kdos-disks` has no `fsck`. Check a removable filesystem from a terminal, unmounted, as on
  [Filesystems and fstab](filesystems-and-fstab.md#checking-a-filesystem-by-hand).
- FAT, exFAT and NTFS have no ownership of their own, so the daemon mounts them as yours with
  `fmask=0117,dmask=0007`; no file on them is executable even with `exec = yes`.
- SMART health for a removable disk is the `s` key in `kdos-disks`, or `kdos-mount smart <row>`;
  see [Disk health](disk-health.md).

## Troubleshooting

**`kdos-mount: no kdos-mountd on /run/kdos-mountd.sock (Connection refused)`.** The daemon is not
running, and `service status mountd` says `[DOWN] kdos-mountd`. Check it and read its log:

```bash
service status mountd
cat /run/kdos-svc.kdos-mountd.log
```

`sudo service start mountd` starts it. `[SKIP] kdos-mountd: /usr/sbin/kdos-mountd not found` in
`/run/kdos-init.mountd.log` means the package is not installed.

**A plugged-in stick is not listed.** The daemon offers a device only when it is removable or on
USB, carries a filesystem it recognises (LUKS, ext2/3/4, FAT, NTFS, exFAT, ISO 9660 or btrfs), is
not named in `/etc/fstab` and is not the boot medium. Check what the kernel sees with
`sudo lsblk -f`; a stick that shows a filesystem of another type, or none, is not offered. See
[Which devices are offered](../../kdos/04-programs/daemons.md#which-devices-are-offered).

**`err this kernel cannot mount <type>`.** The driver is a module that is not loaded or not
installed; `sudo modprobe ntfs3` (or the type named) says which.

**The answer is `err` and nothing else helps.** Each refusal names its reason; the full list is in
[What a destructive verb refuses](../../kdos/04-programs/daemons.md#what-a-destructive-verb-refuses)
and [Verbs](../../kdos/04-programs/daemons.md#verbs). The daemon's own lines are in
`/run/kdos-svc.kdos-mountd.log` and in `/var/log/messages`, tagged `kdos-mountd`.

**A program on a stick will not run: `Permission denied`.** The stick is mounted `noexec`. Add
`exec = yes` to `/etc/kdos/mountd.conf`, unmount and mount it again. On FAT, exFAT and NTFS that
is not enough, because every file there is mounted without execute permission (see
[Tips](#tips)); run a script through its interpreter instead, `sh /media/$USER/<label>/script.sh`.

**GNOME Disks or Dolphin says it is not authorised.** The account is not in `wheel`; udisks2's
grants in `50-kdos.rules` are for `wheel` only. Use `kdos-disks`, which `seat` members may use, or
add the account to `wheel`.

## Known gaps

- [Burning a disc and writing an image are unproved on hardware](../../kdos/06-reference/known-gaps.md#burning-a-disc-and-writing-an-image-are-unproved-on-hardware)
- [A phone is mounted by hand](../../kdos/06-reference/known-gaps.md#a-phone-is-mounted-by-hand)
- [`lsblk` shows a filesystem's type, label and UUID to root only](../../kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only)
- [A USB modem that first appears as a storage device is not switched into a modem](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem)
- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run),
  which includes every udisks2 consumer
- [Status](../../kdos/06-reference/status.md#daemons): what has been asserted of `kdos-mountd`, against
  a recorded device tree

## See also

- [kdos-mountd](../../kdos/04-programs/daemons.md#kdos-mountd): the verbs, the request format, the eligible devices and every refusal
- [Storage](../../kdos/02-user-guide/administration.md#storage): removable media and udisks2 in the user guide
- [kdos-disks](../../kdos/04-programs/kdos-shell.md#kdos-disks) and [kdos-devices](../../kdos/04-programs/kdos-shell.md#kdos-devices): the two surfaces
- [Mount options](../../kdos/03-architecture/security-model.md#mount-options): why removable media are `nosuid,nodev,noexec`
- [`kdos clone`](../../kdos/04-programs/kdos-command.md#kdos-clone): copying the medium
- [Filesystems and fstab](filesystems-and-fstab.md), [Encrypted root](encrypted-root.md), [Disk health](disk-health.md), [Backups](backups.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 375.* Previous: [374. Filesystems and fstab](filesystems-and-fstab.md) · [Contents](../README.md) · Next: [376. Encrypted root](encrypted-root.md)
