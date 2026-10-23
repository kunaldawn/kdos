# Filesystems and fstab

This page covers the [filesystems]{.idx term="filesystem"} a KDOS machine can make, mount and check, and [`/etc/fstab`]{.idx}, the
table of what the boot mounts. It shows which port provides each filesystem's tools, what the
image's own `fstab` holds and what the installer adds to it, how the boot checks and mounts the
table, and the tasks of an installed system: adding a disk, checking a filesystem by hand,
labelling one and trimming an SSD. When the root itself is checked, and why only [ext4]{.idx} gets a boot
check, is in the KDOS book's
[Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root); this page does
not repeat it. Swap lines are on [zram and swap](zram-and-swap.md).

## Installation

[util-linux](#port-util-linux) provides `mount`, `umount`, `findmnt`, `blkid`, `lsblk`, [`fsck`]{.idx},
[`fstrim`]{.idx}, `wipefs`, `cfdisk` and `sfdisk`. Its `fsck` is the one the boot runs: a front end that
runs the checker of each filesystem type, `fsck.<type>`. [e2fsprogs](#port-e2fsprogs) is built
with `--disable-fsck`, so util-linux's is the only `fsck` on the system.

Each filesystem's own tools come from its port:

| Filesystem | Port | Kernel driver |
|---|---|---|
| ext2, ext3, ext4 | [e2fsprogs](#port-e2fsprogs) | built in |
| btrfs | [btrfs-progs](#port-btrfs-progs) | built in |
| xfs | [xfsprogs](#port-xfsprogs) | built in |
| f2fs | [f2fs-tools](#port-f2fs-tools) | module |
| FAT (vfat) | [dosfstools](#port-dosfstools), [mtools](#port-mtools) | built in |
| exFAT | [exfatprogs](#port-exfatprogs) | built in |
| NTFS | [ntfs-3g](#port-ntfs-3g) | `ntfs` and `ntfs3`, modules; [`ntfs-3g`]{.idx} itself mounts through FUSE |
| ISO 9660, UDF | [udftools](#port-udftools) for UDF | built in |
| squashfs | [squashfs-tools](#port-squashfs-tools) | built in |
| EROFS | [erofs-utils](#port-erofs-utils) | module |
| HFS+, JFS, NILFS2 | [hfsprogs](#port-hfsprogs), [jfsutils](#port-jfsutils), [nilfs-utils](#port-nilfs-utils) | modules |

The kernel column is read from `ports/core/boot/linux/kdos.config`, where each driver is `=y`
(built in) or `=m` (a module). [fuse](#port-fuse) provides `fusermount3`, setuid root, for the
FUSE filesystems: `ntfs-3g`, e2fsprogs' `fuse2fs`, [sshfs](#port-sshfs) and
[gocryptfs](#port-gocryptfs). The four filesystems the installer offers for the root, ext4, btrfs,
xfs and f2fs, are the ones the initramfs can mount; see
[The root filesystem](../../kdos/02-user-guide/installation.md#the-root-filesystem).

Nothing is started for a filesystem except at boot. `rcS` checks and mounts `/etc/fstab`;
`03_lvm` does the same again for entries on the volume groups it activates (see
[LVM and RAID](lvm-and-raid.md)); `53_xfs_healer` watches each XFS filesystem mounted at boot (see
[Disk health](disk-health.md)); and a periodic job runs `fstrim -a` daily. All are enabled on the
image.

## Configuration

**`/etc/fstab`**, from `fs/etc/fstab`. The image's own table mounts only the kernel's
pseudo-filesystems and the two temporary ones:

<!-- kfs:include fs/etc/fstab -->

The live medium uses this table unchanged. Its root is the overlay the initramfs built, which no
line of the table names. Every key and line is described in
[`/etc/fstab`](../../kdos/06-reference/configuration.md#etcfstab).

**The installer's lines.** On an installed system the installer puts its own lines in front of the
image's, keeping every shipped one, from `src/system/kdos-installer/install.c`:

<!-- kfs:include src/system/kdos-installer/install.c lines=1622-1648 pin=722f1c369b15 -->

So an installed `fstab` begins with the root by its filesystem UUID, the EFI system partition at
`/boot/efi` with `umask=0077` and pass 2, and any [swap]{.idx} partition or swap file. The root's type,
options and pass number come from one table, from `src/system/kdos-installer/conf.c`:

<!-- kfs:include src/system/kdos-installer/conf.c lines=39-57 pin=2664f1a53985 -->

Only ext4 has pass 1; btrfs, xfs and f2fs have 0. See
[The filesystem table](../../kdos/04-programs/kinstall.md#the-filesystem-table).

**The boot's check and mount**, from `fs/etc/init.d/rcS`. `fsck -A -R -T -a` checks every entry with
a non-zero pass except the root, and `mount -a` mounts every entry:

<!-- kfs:include fs/etc/init.d/rcS lines=55-80 pin=992729eea9fd -->

**The daily trim**, [`/etc/kdos/timers.d/20-fstrim.timer`]{.idx}, from `fs/etc/kdos/timers.d/20-fstrim.timer`:

<!-- kfs:include fs/etc/kdos/timers.d/20-fstrim.timer -->

How a timer line is read is on [Timers](timers.md).

## Usage

### Seeing what is mounted

```bash
findmnt                 # the mount tree, with each filesystem's source, type and options
df -h                   # space used and free on each
sudo blkid              # every filesystem's type, label and UUID
```

`lsblk -f` needs root to report a filesystem's type, label and UUID; run as a user its columns
are empty. `blkid` run as a user cannot read a device (`blkid -p /dev/sdb1` answers
`Permission denied`) and prints only what root's last `blkid` left in its cache, which may be out
of date, so ask it as root. See [Known gaps](#known-gaps).

### Adding a disk to fstab

1. Find the partition and its filesystem's UUID:

   ```bash
   sudo lsblk -f
   sudo blkid -s UUID -o value /dev/sdb1
   ```

   For a new partition, make the filesystem first, for example `sudo mkfs.ext4 -L data /dev/sdb1`.

2. Make the [mount point]{.idx}; the image ships no `/srv`, so `-p` makes it too:

   ```bash
   sudo mkdir -p /srv/data
   ```

3. Add a line with `sudoedit /etc/fstab`, naming the filesystem by its UUID:

   ```text
   UUID=<uuid>   /srv/data   ext4   defaults,noatime   0 2
   ```

   Use pass `2` for ext2, ext3, ext4 and FAT, so that the boot checks it, and `0` for btrfs, xfs and
   f2fs, as the installer does for the root. Add `nofail` to the options of a disk that is not
   always connected.

4. Check the table, then mount everything it names that is not mounted yet:

   ```bash
   sudo findmnt --verify
   sudo mount -a
   findmnt /srv/data
   ```

   A table with no fault gives `Success, no errors or warnings detected`, and `findmnt` then shows
   the line mounted, `/srv/data /dev/sdb1 ext4 rw,noatime`.

`mount` is not setuid on KDOS (util-linux is built `--disable-makeinstall-setuid`), so a `user` or
`users` option does not let an ordinary account mount the line; see
[setuid binaries](../../kdos/03-architecture/security-model.md#setuid-binaries). A removable disk an
ordinary account should mount belongs to [`kdos-mountd`]{.idx} instead; see
[Storage and removable media](storage-and-removable-media.md).

On the live medium the edit lands in the session's overlay and lasts only as long as the session,
or as long as a persistence store holds it.

### Checking a filesystem by hand

A filesystem is checked unmounted, except where its tool says otherwise. Unmount it first:

```bash
sudo umount /srv/data
```

Then run its checker:

| Filesystem | Check | Notes |
|---|---|---|
| ext2, ext3, ext4 | `sudo e2fsck -f /dev/sdb1` | `-f` checks a filesystem marked clean. It asks before each repair, so it needs a terminal; `-n` only reports, and `-p` repairs what is safe without asking |
| xfs | `sudo xfs_repair /dev/sdb1` | `xfs_repair -n` only reports. `sudo xfs_scrub /srv/data` checks while mounted |
| btrfs | `sudo btrfs scrub start -B /srv/data` | Runs mounted, reading every block against its checksum. `btrfs check` is the unmounted, offline check |
| f2fs | `sudo fsck.f2fs /dev/sdb1` | |
| FAT | `sudo fsck.vfat -a /dev/sdb1` | |
| exFAT | `sudo fsck.exfat /dev/sdb1` | |
| NTFS | `sudo ntfsfix /dev/sdb1` | Repairs a few common faults and marks the volume for Windows to check (`Volume is scheduled for check`). The kernel's `ntfs3` then refuses to mount it until Windows has checked it (`volume is dirty and "force" flag is not set!` in `dmesg`); `ntfs-3g` still mounts it |

The root cannot be unmounted on the running system. An ext2, ext3 or ext4 root is checked by the initramfs at
every boot; to check any root by hand, boot the live medium and check it there, as
[Encrypted root](encrypted-root.md#opening-the-root-from-the-live-medium) does for an encrypted one.

### Labelling a filesystem

A label names a filesystem in `fstab` (`LABEL=data`) and becomes the mount point's name under
`/media/<user>/` when `kdos-mountd` mounts it.

| Filesystem | Command |
|---|---|
| ext2, ext3, ext4 | `sudo e2label /dev/sdb1 data` |
| btrfs | `sudo btrfs filesystem label /srv/data data` |
| xfs | `sudo xfs_admin -L data /dev/sdb1` (unmounted) |
| FAT | `sudo fatlabel /dev/sdb1 DATA` |
| exFAT | `sudo exfatlabel /dev/sdb1 data` |
| NTFS | `sudo ntfslabel /dev/sdb1 data` |

### Trimming an SSD by hand

The daily job runs `fstrim -a`. To run it at once and see what it did:

```bash
sudo fstrim -av
```

It prints one line for each filesystem it trimmed, such as
`/srv/data: 487.1 MiB (510717952 bytes) trimmed on /dev/sdb1`. The job itself runs as the
supervised service `kdos-timer-fstrim`, which `service status timers` lists.

### Changing a mount's options

Edit the line in `/etc/fstab`, then apply it to the mounted filesystem without unmounting:

```bash
sudo mount -o remount /srv/data
```

## Tips

- Name filesystems by `UUID=` or `LABEL=`, never by `/dev/sdX`: device names follow the order in
  which disks are found, and that order can change from one boot to the next.
- `kdos-mountd` stops offering a removable device that `/etc/fstab` names by its device path or by
  `LABEL=`. It does not match `UUID=`, so a USB disk listed by UUID is still offered in the
  desktop's device list.
- e2fsprogs is built without its cron directory (`--without-crond-dir`) and xfsprogs without
  `xfs_scrub_all`, so no periodic online check runs. A btrfs scrub or an `xfs_scrub` can be made a
  job of the system's timer table; see [Timers](timers.md).
- Clear a partition with `sudo wipefs -a /dev/sdb1` before making a new filesystem over an old
  one. `mkfs.btrfs -f` over an XFS filesystem leaves the XFS superblock at offset 0 beside the new
  btrfs one at 64 KiB; `sudo wipefs /dev/sdb1` lists both, and a `mount` that is not told
  `-t btrfs` tries XFS and fails with `fsconfig() failed: Data consistency error.`
- The installer mounts ext4 and xfs without `discard`; the daily `fstrim` covers them. btrfs and
  f2fs discard on their own.
- `kdos why /etc/fstab` prints the reasons recorded against the file, among them why `/tmp` must
  be mode 1777.

## Troubleshooting

**The splash says `fsck: errors left uncorrected, see /run/kdos-fsck.log`.** A filesystem with a pass
number had errors `fsck -a` would not repair alone. It is mounted anyway. Read the report, then
unmount that filesystem and check it by hand as in
[Checking a filesystem by hand](#checking-a-filesystem-by-hand):

```bash
cat /run/kdos-fsck.log
```

The log is in `/run` and is gone at the next boot; a boot whose `fsck` printed nothing, as on the
live medium, writes none. An entry on a volume group activated by `03_lvm` is checked there, and
its report is appended to the same file.

**The splash says `ROOT FILESYSTEM HAS ERRORS - RUN e2fsck`.** The initramfs's `e2fsck -p` left
errors on an ext root and the boot went on. Boot the live medium and run `sudo e2fsck -f` on the
root's device.

**A filesystem in `fstab` is not mounted after boot.** Mount it by hand to see the error the boot
printed:

```bash
sudo findmnt --verify
sudo mount -a
```

`findmnt --verify` reports a line whose source does not exist, a type the kernel does not know and
a mount point that is missing, and exits 1. A line naming a disk that is not there reads:

```text
/nonexist
   [E] unreachable on boot required target: No such file or directory
   [W] unreachable source: /dev/vdz9: No such file or directory
   [W] cannot detect on-disk filesystem type (No such file or directory)

0 parse errors, 1 error, 2 warnings
```

and `mount -a` says `mount: /nonexist: fsconfig() failed: /dev/vdz9: Can't lookup blockdev.`
For an entry on a logical volume, see [LVM and RAID](lvm-and-raid.md#troubleshooting).

**`mount: unknown filesystem type 'f2fs'`, or `'ntfs'`, `'ntfs3'`, `'erofs'`, `'hfsplus'`, `'jfs'`, `'nilfs2'`.**
These drivers are modules, and `mount` has the kernel load the one it needs, so this message
means the module could not be loaded. `sudo modprobe f2fs` (or the type named) prints why, such as
`modprobe: FATAL: Module f2fs not found in directory /lib/modules/<version>` when the modules of
the running kernel are not installed.

**`lsblk -f` shows empty columns.** Run it as root; see [Known gaps](#known-gaps).

## Known gaps

- [`lsblk` shows a filesystem's type, label and UUID to root only](../../kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only)
- [An NFSv3 mount needs `-o nolock`](../../kdos/06-reference/known-gaps.md#an-nfsv3-mount-needs--o-nolock)
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  ntfs-3g reads no boot geometry from `libhd`, and btrfs-progs cannot convert a ReiserFS volume

## See also

- [Storage](../../kdos/02-user-guide/administration.md#storage): the boot's checks, swap, zram and removable media
- [Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root): the initramfs's check and the two boot passes
- [Mount points that must be right](../../kdos/03-architecture/boot-and-init.md#mount-points-that-must-be-right): the pseudo-filesystems and why each is mounted
- [`/etc/fstab`](../../kdos/06-reference/configuration.md#etcfstab): the file, in the configuration reference
- [The filesystem table](../../kdos/04-programs/kinstall.md#the-filesystem-table): the installer's four root filesystems
- [Mount options](../../kdos/03-architecture/security-model.md#mount-options): `nosuid`, `nodev` and `noexec` across the system
- [zram and swap](zram-and-swap.md), [LVM and RAID](lvm-and-raid.md), [Encrypted root](encrypted-root.md), [Storage and removable media](storage-and-removable-media.md), [Disk health](disk-health.md), [Timers](timers.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 374.* Previous: [373. Live persistence](live-persistence.md) · [Contents](../README.md) · Next: [375. Storage and removable media](storage-and-removable-media.md)
