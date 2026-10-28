# LVM and RAID

KDOS carries both of Linux's ways of building one block device out of several: [LVM]{.idx}, whose
volume groups hold resizable logical volumes, and md software [RAID]{.idx}, whose arrays mirror or stripe
[partitions]{.idx term="partition"} across disks. This page shows the ports and boot scripts involved, where each is
configured, and the everyday tasks: bringing up a volume group on a disk plugged in later, making
and growing volumes, building and watching an array, and replacing a failed disk. When the boot
activates volume groups and assembles arrays, and why, is in the KDOS book's
[Activating volume groups](../../kdos/03-architecture/boot-and-init.md#activating-volume-groups);
this page does not repeat it.

## Installation

[lvm2](#port-lvm2) provides `lvm` and its command names (`pvcreate`, `vgcreate`, `lvcreate`,
`vgchange`, [`lvs`]{.idx} and the rest), [`dmsetup`]{.idx}, [`dmeventd`]{.idx} and `libdevmapper`.
[thin-provisioning-tools](#port-thin-provisioning-tools) provides [`thin_check`]{.idx} and [`cache_check`]{.idx},
which lvm runs before it activates a thin pool or a cached volume. [mdadm](#port-mdadm) provides
[`mdadm`]{.idx} and `mdmon`. All three are on the live medium and on an installed system.

lvm2 is built with event activation off, and its hotplug rule, `69-dm-lvm.rules`, which activates
a group through `systemd-run`, is deleted from the package; see the
`--with-default-event-activation=0` line and the last line of its `build.sh` in
[Configuration](#configuration). Nothing activates a volume group when a disk appears. mdadm keeps its [udev rules]{.idx term="udev rule"}, so a disk carrying an array member
is assembled incrementally as it appears.

Two boot scripts come from the tree's `fs/` overlay, and both are enabled on the image
(`/etc/service.disabled/` holds no marker for either):

| Script | Does | Skips itself when |
|---|---|---|
| `03_lvm` | Activates every volume group once, then checks and mounts any [`/etc/fstab`]{.idx} entry on a volume that was not active earlier in the boot, and turns on any swap entry on one. It starts no daemon | `/usr/sbin/lvm` is missing |
| `51_mdmonitor` | Supervises `mdadm --monitor --scan --syslog`, which writes a failed member, a degraded array or a missing spare to the system log | [`/proc/mdstat`]{.idx} lists no array |

The initramfs carries `mdadm`, `lvm`, `dmsetup`, `thin_check` and `cache_check`, and assembles
every array before it looks for the root; see [The initramfs](../15-image/090-initramfs.md).

[udisks2](#port-udisks2) is built with its LVM2 module, for the natively ported disk applications,
but the polkit rule grants no account the right to change volume groups or arrays through it; see
[Storage and removable media](storage-and-removable-media.md).

## Configuration

**[`/etc/lvm/lvm.conf`]{.idx}** is lvm2's own, installed by its `make install`. Every setting in it is
commented out, so each is the compiled default, and the defaults that matter here were set at
configure time, from `ports/core/disk/lvm2/build.sh`:

<!-- kfs:include ports/core/disk/lvm2/build.sh lines=14-43 pin=d9375164ccfe -->

`--with-thin-check=/usr/sbin/thin_check` and the other `--with-*` paths are where lvm looks for
the thin-provisioning tools; `--with-default-event-activation=0` is why nothing activates a group
on hotplug.

**The initramfs's `lvm.conf`** is a different file, written into the initramfs alone, from
`script/phases/70_image/090_initramfs.sh`:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=332-340 pin=9b5a3ded256f -->

**[`/etc/init.d/03_lvm.sh`]{.idx}**, from `fs/etc/init.d/03_lvm.sh`. Its `start` activates the groups and,
when any is active, checks and mounts the `fstab` entries the earlier `mount -a` could not reach,
and turns on their swap:

<!-- kfs:include fs/etc/init.d/03_lvm.sh lines=33-56 pin=f2fa25519dd0 -->

**[`/etc/mdadm.conf`]{.idx}** is written by the [mdadm](#port-mdadm) port's `build.sh`. It lists no arrays;
`DEVICE partitions` tells mdadm to scan every partition for a member's superblock:

<!-- kfs:include ports/core/disk/mdadm/build.sh lines=16-22 pin=99ed2ee65218 -->

A `MAILADDR` or `PROGRAM` line added here gives `mdadm --monitor` a route of its own beside the
system log (`mdadm.conf(5)`).

**[`/etc/init.d/51_mdmonitor.sh`]{.idx}**, from `fs/etc/init.d/51_mdmonitor.sh`:

<!-- kfs:include fs/etc/init.d/51_mdmonitor.sh lines=21-34 pin=eaef8c452b08 -->

**The RAID step of the initramfs**, from `script/phases/70_image/090_initramfs.sh`, assembles every
array whose members it finds, on the live medium and on an installed system:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=606-614 pin=1019c6f0eacb -->

## Usage

Every command here runs as root. On the live medium, everything you make is real and on disk, but
the `/etc/fstab` you edit is in the session's overlay and is gone at power-off unless the medium
has a persistence store; see [`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist).

### Bringing up a volume group plugged in later

1. Activate every group the machine can see:

   ```bash
   sudo vgchange -aay
   ```

2. List the logical volumes:

   ```bash
   sudo lvs
   ```

   Each is at `/dev/<group>/<volume>`.

3. Mount the one you want, for example `sudo mount /dev/data/files /mnt`.

`sudo service status lvm` runs `lvs` and prints the same list, or nothing when no group is active.
Run without `sudo` it prints nothing either way, because `lvs` needs root.

### Making a volume group on a new disk

1. Find the partition with `sudo lsblk -f`, and make sure nothing on it is wanted.
2. Make it a physical volume and a group, and a volume in the group:

   ```bash
   sudo pvcreate /dev/sdb1
   sudo vgcreate data /dev/sdb1
   sudo lvcreate -n files -L 200G data
   ```

3. Make a filesystem on the volume, and read back its UUID:

   ```bash
   sudo mkfs.ext4 -L files /dev/data/files
   sudo blkid -s UUID -o value /dev/data/files
   ```

4. Add it to `/etc/fstab` by that UUID and mount it, as on
   [Filesystems and fstab](filesystems-and-fstab.md#adding-a-disk-to-fstab).

At the next boot, a group the initramfs saw is already active when `rcS` mounts it; one it did not
see is activated and mounted by `03_lvm`.

Do not name a new group `kdos`: the installer makes that group, and refuses to install while a
group of that name has any part on another disk.

### Growing a volume and its filesystem

```bash
sudo vgs                                  # VFree is the space left in the group
sudo lvextend -r -L +50G data/files
```

`-r` (`--resizefs`) grows the filesystem in the same step, through the helper lvm2 installs,
`/usr/libexec/lvresize_fs_helper`. On an unmounted ext4 volume it runs `e2fsck` first, then
`resize2fs`; on a mounted one `resize2fs` grows it online. It ends
`Logical volume data/files successfully resized.` `lvreduce -r` shrinks an ext4 volume the same
way, its filesystem first. xfs grows and never shrinks; see
[The root filesystem](../../kdos/02-user-guide/installation.md#the-root-filesystem).

lvm learns whether the volume is mounted from `/etc/mtab`, a link to `/proc/self/mounts` that the
image ships; see [Troubleshooting](#troubleshooting) for a machine without it.

### Slot B's volume

On an install with LVM, when the disk has room for two systems, the installer gives `root_a` half
of the group `kdos` and leaves the other half free for a second root slot.
The volume is made with `sudo lvcreate -n root_b -l 100%FREE kdos`; the rest of the procedure is on
[A/B slots and rollback](ab-slots-and-rollback.md).

### Building a mirror

1. Pick two partitions of the same size on two disks, both holding nothing wanted.
2. Create the array:

   ```bash
   sudo mdadm --create /dev/md0 --level=1 --raid-devices=2 /dev/sdb1 /dev/sdc1
   ```

   mdadm asks two questions first: whether to enable a write-intent bitmap, which speeds up a
   resynchronisation after a crash, and, after a note that the metadata sits at the start of each
   member, `Continue creating array [y/N]?`. Answer `y` to the second. `--run` skips both
   questions and makes the array without a bitmap; add `--bitmap=internal` beside it to have one.
   It ends `mdadm: array /dev/md0 started.`

3. Watch the first synchronisation:

   ```bash
   cat /proc/mdstat
   ```

   While it runs, the array's line is followed by a progress bar, `resync = 0.0% …`;
   `sudo mdadm --wait /dev/md0` returns when it is done.

4. Make a filesystem on `/dev/md0` and add it to `/etc/fstab` by its UUID, as for any partition.
5. Start the monitor, which skipped itself at boot because there was no array then:

   ```bash
   sudo service start mdmonitor
   service status mdmonitor          # [ OK ] mdmonitor  (pid …)
   ```

   It logs `NewArray event detected on md device /dev/md0` for each array it finds.

No `ARRAY` line is needed in `/etc/mdadm.conf`: the initramfs's `mdadm --assemble --scan` finds the
array by its members' superblocks at every boot, and `51_mdmonitor` starts on its own from then on.
The installer does not offer an array as a disk to install onto; its disk list leaves `md` devices
out.

### Checking an array

```bash
cat /proc/mdstat
sudo mdadm --detail /dev/md0
```

`[UU]` in `/proc/mdstat` is a mirror with both members; `[U_]` is one running degraded.

### Replacing a failed disk

1. Find the failed member in `sudo mdadm --detail /dev/md0`, marked `faulty`.
2. Remove it from the array:

   ```bash
   sudo mdadm /dev/md0 --fail /dev/sdc1 --remove /dev/sdc1
   ```

   It answers `mdadm: hot removed /dev/sdc1 from /dev/md0`, and `/proc/mdstat` shows the mirror
   as `[2/1] [U_]`.

3. Fit the new disk, give it a partition of at least the same size, and add it:

   ```bash
   sudo mdadm /dev/md0 --add /dev/sdd1
   ```

4. Follow the rebuild in `cat /proc/mdstat`. The new member is listed with `(S)`, a spare, until
   the rebuild starts on it, and the mirror reads `[2/2] [UU]` again when it ends.

### Starting an array that is missing a member

An array whose disks are plugged in after boot is assembled by mdadm's udev rule as its members
appear, and started once all are present. One that stays short of a member stays inactive: the
rule's fallback for that case, in `/usr/lib/udev/rules.d/64-md-raid-assembly.rules`, is a systemd
timer (`mdadm-last-resort@.timer`), and KDOS has no systemd to run it. The incremental assembly
says `mdadm: /dev/sdb1 attached to /dev/md/0, not enough to start safely.` Start it degraded by
hand, by the name `/proc/mdstat` gives it:

```bash
cat /proc/mdstat                 # md0 : inactive, the member listed as a spare, (S)
sudo mdadm --run /dev/md0        # mdadm: started array /dev/md/0
```

The array then runs as `active (auto-read-only) raid1 … [2/1] [U_]`, read-only until the first
write.

## Tips

- `sudo lsblk` shows how the layers stack, disk to partition to array or physical volume to
  logical volume; add `-f` for filesystems, which it shows to root only.
- Volumes activated during the boot are not monitored by `dmeventd`, because the boot activates
  with `--sysinit`, which turns monitoring off. A thin pool's automatic extension and an LVM RAID
  volume's automatic repair depend on that monitoring; `sudo vgchange --monitor y <group>` turns
  it on for a running group and starts `dmeventd` when a volume needs it.
  `sudo lvs -o lv_name,seg_monitor` shows a thin pool as `not monitored` after a `--sysinit`
  activation and `monitored` after the `--monitor y`.
- With encryption and LVM together, the installer puts the group `kdos` inside the container, so
  a volume made in its free space is encrypted with the root and opened by the same passphrase.

## Troubleshooting

**A disk carrying LVM shows no volumes.** Nothing activates a group on hotplug. Run
`sudo vgchange -aay` as in [Bringing up a volume group plugged in later](#bringing-up-a-volume-group-plugged-in-later).

**A filesystem on a logical volume is not mounted after boot.** Read what `03_lvm` did, and what
the checks found:

```bash
cat /run/kdos-init.lvm.log
cat /run/kdos-fsck.log
```

`[WARN] lvm: fsck left errors uncorrected` means the check found errors it would not repair alone;
unmount the volume if it is mounted and run [`fsck`]{.idx} on it by hand. After
`[WARN] lvm: mount -a reported a failure`, the lines above it in the same log are `mount`'s own
error; `sudo mount -a` run again by hand prints it once more.

**The splash marks `VOLUME GROUPS` failed.** The initramfs's `lvm vgchange -aay --sysinit` failed.
The boot goes on, and if the root was in that group the next stage reports it missing and opens the
rescue shell. There, `lvm vgchange -aay` prints lvm's reason; see
[When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops).

**`lvextend -r` prints only `Command failed with status code 5`.** The volume keeps its size. With
`-vvv`, the line before that status, after `Completed: lvextend …`, is
`Internal error: Failed command did not use log_error`, and only lvm's cleanup lines follow. lvm
could not open `/etc/mtab`. Restore the link and run it again:

```bash
sudo ln -s ../proc/self/mounts /etc/mtab
```

Without the link, grow in two steps: `sudo lvextend -L +50G data/files`, then
`sudo resize2fs /dev/data/files` (or `sudo xfs_growfs <mount point>`).

**A thin pool or cached volume will not activate.** lvm ran `thin_check` or `cache_check` and it
failed, or the tool is missing. `sudo lvchange -ay <group>/<pool>` prints which.

**`service status mdmonitor` says it is not running.** `/run/kdos-init.mdmonitor.log` holds
`[SKIP] mdmonitor: no md array on this machine` when there was no array at boot; start it once
there is one. A monitor that exited with status 0 or 1 is not restarted: 0 is mdadm finding no
array with redundancy, which includes an array that is all RAID0, and 1 is a configuration it
cannot use. Its own lines are in `/run/kdos-svc.mdmonitor.log`; once the last redundant array is
stopped they end `mdadm: No array with redundancy detected, stopping` and
`Exited with code 0, a final status: not restarting`.

**An array is degraded.** `grep mdadm /var/log/messages` shows the monitor's events with the
member named, such as `Fail event detected on md device /dev/md0, component device /dev/sdc1`;
replace it as in [Replacing a failed disk](#replacing-a-failed-disk).

## Known gaps

- [LVM volume groups are activated only at boot](../../kdos/06-reference/known-gaps.md#lvm-volume-groups-are-activated-only-at-boot)
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [`lsblk` shows a filesystem's type, label and UUID to root only](../../kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  lvm2 has no SELinux labels

## See also

- [Activating volume groups](../../kdos/03-architecture/boot-and-init.md#activating-volume-groups): when and how the boot brings up LVM, and the thin and cache targets
- [Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root): the two `fsck` passes, `rcS`'s and `03_lvm`'s
- [A root on LVM](../../kdos/04-programs/kinstall.md#a-root-on-lvm): the installer's layout, names and refusals
- [LVM](../../kdos/02-user-guide/installation.md#lvm): the Layout page's option
- [Services](../../kdos/02-user-guide/administration.md#services): `03_lvm` and `51_mdmonitor` among the boot scripts
- [The initramfs init](../15-image/091-initramfs-init.md): the RAID and VOLUME GROUPS stages
- [Encrypted root](encrypted-root.md), [Filesystems and fstab](filesystems-and-fstab.md), [Disk health](disk-health.md), [A/B slots and rollback](ab-slots-and-rollback.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 377.* Previous: [376. Encrypted root](encrypted-root.md) · [Contents](../README.md) · Next: [378. Disk health](disk-health.md)
