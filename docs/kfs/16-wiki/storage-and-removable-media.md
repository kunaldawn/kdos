# Storage and removable media

USB sticks, SD cards, external disks and optical discs are mounted on KDOS by [udisks2]{.idx}, the
system bus's storage service, which Dolphin, the Device Notifier in the system tray, the file
dialogs, KDE Partition Manager and GNOME Disks all ask. polkit decides what each request may do.
This page shows which ports provide each route, the rule that changes what an administrator may
do without a password, and the tasks: mounting and ejecting, unlocking an encrypted stick,
formatting and partitioning, writing an image, checking that a stick holds what it claims, and a
phone.

## Installation

| Port | Provides |
|---|---|
| [udisks2](#port-udisks2), with [libblockdev](#port-libblockdev) | `udisksd`, `/usr/lib/udisks2/udisksd`, and `udisksctl`. It has no service script; the system bus starts it on the first request |
| [plasma-workspace](#port-plasma-workspace) | The Device Notifier in the system tray, which offers each new device and unmounts and ejects it |
| [plasma-desktop](#port-plasma-desktop) | The device automounter, and System Settings → **Disks & Cameras** → **Device Auto-Mount** |
| [dolphin](#port-dolphin) | The file manager, whose Places panel lists every device |
| [partitionmanager](#port-partitionmanager), [gnome-disk-utility](#port-gnome-disk-utility) | Partitioning, formatting and SMART, through udisks2 or as root |
| [isoimagewriter](#port-isoimagewriter), [caligula](#port-caligula) | Writing a disk image to a stick, from a window and from a terminal |
| [f3](#port-f3) | Testing whether a stick holds as much as it reports |
| [kio-extras](#port-kio-extras), [kdeconnect](#port-kdeconnect), [android-file-transfer](#port-android-file-transfer) | A phone over MTP in Dolphin, over KDE Connect, and a window of its own |
| [kdos-tools](#port-kdos-tools) | `kdos clone`, which copies the KDOS medium itself onto another stick |

Every one is in the base the ISO carries, the live medium included, except gnome-disk-utility,
caligula, f3 and android-file-transfer, which `sudo kdos update install <port>` adds (see
[Installing software](updating.md#installing-software)).

## Configuration

### Who may do what

udisks2's own policy lets the person in the active session at the machine mount, unlock, unmount,
eject and power off a removable drive with no password, and asks an administrator's password for
an internal disk, another user's mount and a raw device. KDOS's rule
`/etc/polkit-1/rules.d/50-kdos.rules`, from `fs/`, grants a member of `wheel` at the machine those
too, without a password:

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=102-124 pin=abfdaaae8881 -->

For everyone else, and for a member of `wheel` over SSH, udisks2's own policy applies, and where it
asks for a password, Plasma's authentication agent asks. The whole rule file is explained in
[What 50-kdos.rules changes](../../kdos/03-architecture/security-model.md#what-50-kdosrules-changes).

### Where a device is mounted

udisks2 mounts a device at `/run/media/<user>/<label>`, or under its UUID when it has no label,
`nosuid,nodev`. On FAT, exFAT and NTFS, which have no ownership of their own, every file is the
mounting user's; ext4 and btrfs keep the ownership stored on them.

### Mounting automatically

System Settings → **Disks & Cameras** → **Device Auto-Mount** sets whether a device is mounted
when it is plugged in, at login, or only when opened, per device or for all. The settings are
the person's own, in `~/.config/kded_device_automounterrc`.

## Usage

The disk the system runs from is never offered for unmounting or formatting: udisks2 marks it a
system device.

### Mounting a stick from the desktop

1. Plug it in. The Device Notifier in the tray opens with the device and its actions: **Open
   with File Manager**, and any other the device's content offers.
2. Or open Dolphin and click the device in the Places panel, which mounts it.
3. To unmount, click the eject arrow beside it in the Device Notifier or in Dolphin's Places
   panel.

### Mounting from a terminal

```bash
udisksctl status                         # the drives udisks2 sees
lsblk -f                                 # the block devices and their filesystems
udisksctl mount -b /dev/sdb1             # prints where it mounted
udisksctl unmount -b /dev/sdb1
udisksctl power-off -b /dev/sdb          # the whole drive, after every partition is unmounted
```

### Ejecting

The eject arrow in the Device Notifier or Dolphin unmounts every partition of the drive and then
powers it down, or opens an optical drive's tray. A drive that something still holds, an open
file or an unlocked mapping, is refused with the reason.

### Unlocking an encrypted stick

1. Plug it in, or click it in Dolphin's Places panel. A [LUKS]{.idx} device asks for its
   passphrase in a dialog.
2. Type it. udisks2 opens the container and mounts the filesystem inside.
3. Ejecting unmounts it and closes the container again.

From a terminal:

```bash
udisksctl unlock -b /dev/sdb2            # asks for the passphrase, prints the mapping
udisksctl mount -b /dev/dm-0
udisksctl lock -b /dev/sdb2              # after unmounting
```

The machine's own encrypted root is a different matter; see [Encrypted root](encrypted-root.md).

### Formatting a stick

1. Open **KDE Partition Manager** (or **Disks**, GNOME's), select the stick, and format a
   partition or the whole device with the filesystem you want. It asks for an administrator's
   password before it writes.
2. Or from a terminal as root, on the unmounted partition:

   ```bash
   sudo mkfs.vfat -F 32 -n STICK /dev/sdb1      # FAT32, for any machine
   sudo mkfs.exfat -L stick /dev/sdb1           # exFAT, for files over 4 GiB
   ```

Dolphin's context menu on a device offers **Format** as well, through Partition Manager.

### Partitioning a stick

KDE Partition Manager shows every disk, its partition table and free space, and applies a queue of
changes when you press **Apply**. From a terminal, the same is [`cfdisk`]{.idx} as root on the whole
disk: `sudo cfdisk /dev/sdb`, which asks before it writes.

### Writing a disk image to a stick

Every way of writing replaces the whole disk, partition table and all. Choose one:

- **ISO Image Writer**: pick the image and the stick, and **Write**. It asks for an administrator's
  password through polkit and verifies what it wrote.
- **[`caligula`]{.idx}**, in a terminal, which lists only removable disks unless given `--show-all-disks`,
  offers to become root through `sudo` when it cannot open the disk, and verifies what it wrote:

  ```bash
  caligula burn image.iso
  ```

- **`kdos clone`**, to copy the KDOS medium the machine booted from; see
  [`kdos clone`](../../kdos/04-programs/kdos-command.md#kdos-clone).

### Checking that a stick is genuine

A counterfeit stick reports more space than it has and silently overwrites earlier data. Two ways
to test one:

1. Without losing what is on it, fill its free space and read it back, on the mounted stick:

   ```bash
   f3write /run/media/$USER/<label>
   f3read  /run/media/$USER/<label>
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

### A phone

An MTP phone is not a block device, so udisks2 does not list it. Plugged in and unlocked, it
appears in Dolphin's Places panel and the Device Notifier through kio-extras' `mtp:/`; KDE Connect
reaches it over the network instead, once paired. From a terminal, mount its storage as a
directory, which must exist first, and take it away again when done:

```bash
mkdir -p ~/Phone
aft-mtp-mount ~/Phone
fusermount3 -u ~/Phone
```

## Tips

- Give a backup disk a label. The mount point is named after it, so `/run/media/<user>/<label>` is
  the same path every time it is plugged in.
- `udisksctl info -b /dev/sdb1` shows everything udisks2 knows about a device, its mount points
  and whether it counts as a system device.
- Check a removable filesystem from a terminal, unmounted, as on
  [Filesystems and fstab](filesystems-and-fstab.md#checking-a-filesystem-by-hand).
- SMART health for a disk is in KDE Partition Manager's properties, in Disks, and from the tray
  through plasma-disks; see [Disk health](disk-health.md).

## Troubleshooting

**A plugged-in stick is not listed.** Check what the kernel sees with `lsblk -f` and `dmesg | tail`.
A stick with no filesystem udisks2 recognises is listed by Partition Manager but not offered for
mounting. A device named in `/etc/fstab` follows that line instead.

**`Not authorized to perform operation`.** polkit refused: the request came from outside the active
session at the machine (an SSH login, a session switched away from), or the password prompt was
cancelled. `udisksctl` from Konsole in the session asks through Plasma's agent.

**`Error mounting … unknown filesystem type`.** The driver is a module that is not loaded or not
installed; `sudo modprobe ntfs3` (or the type named) says which.

**A program on a stick will not run: `Permission denied`.** udisks2 mounts removable media
`nosuid,nodev` and lets a program run only where the filesystem keeps an execute bit; on FAT, exFAT
and NTFS none is kept. Run a script through its interpreter instead,
`sh /run/media/$USER/<label>/script.sh`.

**More detail is needed.** udisks2 logs to the system log: `sudo grep udisksd /var/log/messages`.

## Known gaps

- [Burning a disc and writing an image are unproved on hardware](../../kdos/06-reference/known-gaps.md#burning-a-disc-and-writing-an-image-are-unproved-on-hardware)
- [A USB modem that first appears as a storage device is not switched into a modem](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem)

## See also

- [Storage](../../kdos/02-user-guide/administration.md#storage): removable media in the user guide
- [Encrypted root](encrypted-root.md), [Filesystems and fstab](filesystems-and-fstab.md),
  [Disk health](disk-health.md), [Backups](backups.md), [sudo, PAM and polkit](sudo-pam-polkit.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 401.* Previous: [400. Filesystems and fstab](filesystems-and-fstab.md) · [Contents](../README.md) · Next: [402. Encrypted root](encrypted-root.md)
