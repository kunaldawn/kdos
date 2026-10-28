# Live persistence

A live KDOS session writes into the upper layer of an [overlay]{.idx} on top of the read-only system
image, and that layer is held in RAM unless the medium has a [persistence store]{.idx}. This page
covers making a store, checking that the session is using it, booting past it, starting it
afresh and removing it. How the initramfs builds the overlay and what it requires of the store is
explained in
[The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence);
this page gives the procedures.

## Installation

Persistence is part of the live medium and needs nothing installed. Three pieces take part:

| Piece | From | Does |
|---|---|---|
| `kdos persist` | [kdos-tools](#port-kdos-tools) | Reports the store, and makes one with `sfdisk` and `partx` from [util-linux](#port-util-linux) and `mkfs.ext4` from [e2fsprogs](#port-e2fsprogs) |
| The live path of the initramfs `init` | `script/phases/70_image/090_initramfs.sh` | Finds the store by its label at boot and uses it as the overlay's upper layer |
| `KDOS Live (clean session)` | `script/phases/70_image/110_iso.sh` | A menu entry that passes `nopersist` and ignores the store for one boot |

There is no service script. An installed machine has no overlay and never looks for a store: the
initramfs reads the label only on the live path, when the command line names no `root=UUID=`.

## Configuration

The store is configured by its label and nothing else. No file names its device, and nothing has
to be edited for a store to be used: at boot the initramfs asks `blkid` for a filesystem labelled
`KDOS_PERSIST` and uses whichever answers. The decision, from the generated `init` (`\$` here is
the generator's escaping: the init is written by a heredoc, and the generated script has a plain
`$`):

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1155-1216 pin=df8a4d87c418 -->

`find_persist` is the lookup. It asks for the label again every tenth of a second while USB storage
is still arriving, so a store on a stick that is enumerated after the medium is found is still used:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=952-1055 pin=4d82dbf65d60 -->

The store's mount is moved into the new root as `/mnt/persist`, so the running session can see it
and the shutdown's `umount -a -r` flushes it:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1271-1280 pin=65f01fb10a3e -->

On the store itself, the overlay uses two directories, `upper/` (the session's files) and `work/`
(overlayfs's own), which the initramfs creates when they are missing.

`kdos persist create` makes the filesystem with no reserved blocks and the label:

<!-- kfs:include src/system/kdos-tools/persist.c lines=298-313 pin=adc5db1e67b9 -->

The menu entry that skips the store:

<!-- kfs:include script/phases/70_image/110_iso.sh lines=327-332 pin=00a38d5e1ac3 -->

## Usage

### Making a store on the boot stick

1. Boot `KDOS Live`.
2. See what is there: `kdos persist`. With no store it prints `no persistence store`, followed
   by a reminder that the session's writes are in RAM and that `kdos persist create` makes a
   store.
3. Make the store in the free space after the image:

   ```bash
   sudo kdos persist create
   ```

   It names the disk, the number the new [partition]{.idx} will get and the filesystem, and asks you to
   type `yes`:

   ```text
     disk       /dev/vda  (2.0G)
     partitions 1 now; the store becomes number 2
     filesystem ext4, labelled KDOS_PERSIST

     Nothing already on /dev/vda is moved or rewritten: the store is
     made from the free space AFTER the last partition.

   Type yes to go ahead:
   ```

   Any other answer ends it with `kdos persist: not confirmed` and exit status 1; `--yes` (or
   `-y`) skips the question. It appends one partition with `sfdisk --append` and never rewrites
   the existing entries, registers it with `partx -a`, makes [ext4]{.idx} on it, and ends with
   `store ready on /dev/vda2 (1.9G).` and `It becomes the session's upper layer at the next boot.`
4. Run `kdos persist`. Until the next boot it reports `state    present, NOT in use by this session`.
5. Reboot. The store is used from the next boot, not the one that made it.
6. Run `kdos persist` again. It reports `state    in use by this session` and the free space,
   as `free     1.8G of 1.8G`. `findmnt /mnt/persist` shows the store, and `/` is an overlay
   whose `upperdir` is `/mnt/persist/upper`. Files written by root and by `kdos` in the session
   are there after the next reboot.

With no device named, `create` picks the disk the live system booted from. A machine that booted
the image from an optical drive, as a virtual machine with the ISO attached does, offers
`/dev/sr0` with `partitions 0 now`, which cannot take a partition; name a writable disk there.

### Making a store on another device

The store can be on a second USB stick or on an internal disk; the label is what the boot looks
for, wherever it is.

1. Make it with `kdos persist`, naming the disk: `sudo kdos persist create /dev/sdb`. The new
   partition goes in the free space after that disk's last partition.
2. Or make one by hand on a partition you have already made:
   `sudo mkfs.ext4 -L KDOS_PERSIST /dev/sdb1`. Any Linux filesystem with extended attributes, hard
   links and file types in directory entries will do; vfat, exfat, ntfs, iso9660 and squashfs are
   refused at boot. A FAT or exFAT label holds at most 11 characters, so `mkfs.vfat` and
   `mkfs.exfat` both fail on `KDOS_PERSIST` before such a store can exist, `mkfs.vfat` with an
   error about the label's length; an NTFS volume takes the label, and the boot refuses it.
3. Reboot with that device attached.

The initramfs asks for the label as soon as it has mounted the system image from the medium, and
keeps asking while USB storage is still arriving: while a hub port holds a device that is not yet
configured, or a USB mass-storage interface has no disk beneath it. A new stick has no disk for
about a second after it is enumerated, because usb-storage waits that long before it scans it. While a hub other than a
root hub is attached, the initramfs also waits until the set of USB devices has stood still for
one second, since a hub shows nothing in the moment before it reports what is plugged into it.
The console shows `Waiting for USB devices to settle before looking for a persistence store...`
while it waits. The wait ends as soon as the store answers, and after 10 seconds at most, with
`USB storage still settling after 10s; no persistence store found`, after which the session runs
in RAM. A boot with no USB storage pending and no external hub does not wait. It asks once, and
asks again straight away whenever the set of USB disks has changed since the ask before, because
the kernel creates a disk before it has read the disk's partitions.

Keep one store attached. With two filesystems labelled `KDOS_PERSIST`, `blkid -L` answers with one
of them, and which one is not settled.

### Booting without the store

1. Stop the boot menu's countdown.
2. Choose `KDOS Live (clean session)`.

The command line gains `nopersist`, the console shows `nopersist: this session will not be saved`
and `Mounting OverlayFS...`, and the session runs in RAM, its upper layer on a tmpfs, as with no
store. The store is left exactly as it was, and `kdos persist` reports it as
`present, NOT in use by this session`. This is the way past a change saved in the store that
stops the desktop from starting.

### Starting the store afresh

1. Boot `KDOS Live (clean session)`.
2. Mount the store somewhere other than `/mnt/persist`, which `kdos persist` takes to mean the
   session is using it: `sudo mkdir -p /mnt/x`, then `sudo mount -L KDOS_PERSIST /mnt/x`.
3. Remove the saved session: `sudo rm -rf /mnt/x/upper /mnt/x/work`.
4. `sudo umount /mnt/x` and reboot normally. The initramfs creates both directories again, and
   the session starts with nothing of the old one in it.

### Removing the store

1. Boot `KDOS Live (clean session)`, so that the session is not writing to the store.
2. Find the store's device with `sudo blkid -L KDOS_PERSIST`, for example `/dev/sdb2`.
3. Take the label off: `sudo e2label /dev/sdb2 ""` for an ext store. `kdos persist` then prints
   `no persistence store`, and the next boot shows `Mounting OverlayFS...` with no `(persistent)`
   and runs in RAM.
4. To free the space as well, delete the partition: `sudo sfdisk --delete /dev/sdb 2`, naming the
   disk and the partition's number.

## Tips

- Shut down (`poweroff` or `reboot`) rather than pulling the stick: the shutdown's `umount -a -r`
  is what flushes the store.
- `kdos persist create` refuses when a store already exists anywhere the system can see
  (`kdos persist: a store already exists on /dev/vda2` and `remove it first, or use it as it is`,
  exit status 1), when no device is named and the system did not boot from a medium, when the
  device named is not a block device, and when it is not run as root.
- On a persistent boot the console shows `Persistent session on /dev/vda2 (ext4)`,
  `Mounting OverlayFS (persistent)...`; a boot in RAM shows `Mounting OverlayFS...` alone.
- Software installed with `kpkg` in a persistent session is kept with everything else the session
  writes, in `upper/`.

## Troubleshooting

**`kdos persist` says `present, NOT in use by this session`.** The store was made during this
boot, the boot was a clean session, the boot refused the store for its type (see the next
entry), or the store is on a device that took more than 10 seconds to appear, which the console
reports as `USB storage still settling after 10s; no persistence store found` (see
[Making a store on another device](#making-a-store-on-another-device)). Reboot with `KDOS Live`;
for the last case, put the store on the boot stick, on an internal disk, or on a port that is
not behind a slow hub.

**The store is there, but every boot starts clean.** The initramfs prints why on the console,
which on the live medium is the serial line: `persistence store /dev/vda1 is ntfs, which cannot
hold an overlay upper`, `persistence store … would not mount`, or `nopersist: this session will
not be saved`. Check the type with `sudo blkid -L KDOS_PERSIST` and `sudo blkid <device>`, and remake a
store of the wrong type as ext4. When the overlay itself refuses the store, the session falls back
to RAM, and the console shows `Mounting OverlayFS (persistent)...` followed by
`Mounting OverlayFS...`.

**`kdos persist create` says `sfdisk would not add a partition`.** There is no free space after the
last partition on that disk. Name another device, or make a partition by hand and label it.

**`kdos persist create` says the new partition `did not appear`.** The partition was added to the
table, but the kernel has no device node for it yet, and no filesystem was made. The message says
to reboot and run the command again; reboot, but then make the filesystem on that partition by
hand, as `kdos persist create` would have: `sudo mkfs.ext4 -m 0 -L KDOS_PERSIST /dev/sdX<n>`, with
the device the message named. Running `kdos persist create` again would append a second
partition, because the first has no label for it to find.

**The desktop does not start after a change.** Boot `KDOS Live (clean session)`, then either start
the store afresh or remove the file that caused it from the store's `upper/` directory, mounted as
above.

**Writes fail with "No space left on device".** The store is full. `kdos persist` shows the free
space; start it afresh or make a larger one on another device.

## Known gaps

- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
- [Applications imported or built during an install stay in the live session](../../kdos/06-reference/known-gaps.md#applications-imported-or-built-during-an-install-stay-in-the-live-session):
  a store keeps them for the next live boot, and an install still leaves them behind

## See also

- [The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence)
- [kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)
- [Keep what you change](../../kdos/02-user-guide/getting-started.md#keep-what-you-change)
- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md): the live medium's menu,
  its clean-session entry, and adding a parameter for one boot
- [The initramfs and the rescue shell](initramfs-and-rescue.md): looking inside the initramfs, and
  the shell it opens when a boot cannot finish

<!-- book-nav -->
---

*Part XVI. System administration, chapter 373.* Previous: [372. A/B slots and rollback](ab-slots-and-rollback.md) · [Contents](../README.md) · Next: [374. Filesystems and fstab](filesystems-and-fstab.md)
