# The initramfs init

`init` is the first program the kernel runs, as process 1, out of the initramfs
[the previous chapter](090-initramfs.md) builds. It is a bash script that `090_initramfs.sh` writes
from a here-document, and this chapter walks through it in the order it runs: the early mounts, the
splash, the [rescue shell]{.idx} every failure ends in, the device manager, [RAID]{.idx}, the [kernel command line]{.idx},
the [A/B slot]{.idx term="A/B slots"}, LVM and the unlock, and then the two ways to a root, an installed disk or the live
medium. For each stage it says what the script does when the stage fails.

Why the stages are in this order, and what each protects against, is in
[Boot and init](../../kdos/03-architecture/boot-and-init.md), from
[The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs) to
[Tools that must be util-linux's](../../kdos/03-architecture/boot-and-init.md#tools-that-must-be-util-linuxs).
This chapter does not repeat that; it shows the code those sections describe.

## Reading the excerpts

The excerpts are quoted from the step script, so every `$` that belongs to `init` appears as `\$`
and every backquote as `` \` ``; [The init script](090-initramfs.md#the-init-script) explains why.
Read `\$ROOT_UUID` as `$ROOT_UUID`. After a build, `build/initramfs/init` holds the script as the
kernel runs it.

`testing/selftest.sh` extracts the here-document the same way, undoing the escapes, checks it with
`bash -n`, and runs three of its functions, `unlock_root`, `activate_lvm` and `find_persist`,
against stub programs. The variables `PASS_TTY`, `CRYPT_MAPPER_DIR`, `LVM_BIN` and `USB_DEVICES`
below exist so that it can.

## The stages at a glance

Each stage is announced on the splash with `kdos-splash step` and closed with `ok` or `fail`. The
total the splash's progress bar counts to is added up as the script learns which stages it will run.
Each section below is named after the stage it covers, the label in the table's first column.

| Stage | Runs when | When it fails |
|:--|:--|:--|
| `DEVICE MANAGER` | always | nothing is checked; the stage closes as passed |
| `RAID` | `/bin/mdadm` is carried | ignored: assembling nothing is the ordinary case |
| `FILESYSTEM MODULES` | always | a line per module that would not load; the boot goes on |
| `BOOT SLOT` | the command line has `bootstate=UUID=` and `/bin/kdos-bootctl` is carried | the `root=` of the command line is kept; the stage closes as passed |
| `VOLUME GROUPS` | `root=UUID=` or `cryptdevice=` is given and `blkid` reports physical volumes not yet activated | the stage is marked failed and the boot goes on |
| `UNLOCKING` | `cryptdevice=` is set | rescue shell: `Failed to unlock …` |
| `ROOT DEVICE` | `root=UUID=` is set | rescue shell: `Disk boot failed: no root at UUID=… with /sbin/init on it` |
| `MOUNTING ROOT` | the root was found | the same rescue shell, when `/sbin/init` is not on what was mounted |
| `BOOT MEDIA` | there is no `root=UUID=` | rescue shell: `Failed to find KDOS installation media.` |
| `SYSTEM IMAGE` | the medium was found | rescue shell: `Failed to set up a loop device for system.sfs` or `Failed to mount system.sfs` |
| `OVERLAY ROOT` | `system.sfs` is mounted | a broken persistence store falls back to a session in memory; rescue shell only if `/newroot` is not a directory |
| `SWITCHING ROOT` | the last stage on both paths | live medium: rescue shell, `Error: /sbin/init not found in new root!` |

## The early mounts

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=511-529 pin=142f276345e9 -->

`PATH` is `/bin` alone, the directory the step filled. Everything the script prints goes to
[`/dev/console`]{.idx}, which is the last `console=` on the kernel command line: the serial port on the live
medium's entries, the first virtual terminal on an installed machine's (see
[Command-line parameters KDOS reads](../../kdos/03-architecture/boot-and-init.md#command-line-parameters-kdos-reads)).
Then the kernel's filesystems are mounted: `/proc`, `/sys`, `/dev` as devtmpfs, `/dev/pts` and a
`/dev/shm` with `nosuid,nodev`. None of the mounts is checked. [devpts]{.idx} gets
`gid=5,mode=620,ptmxmode=666`: a new terminal belongs to the `tty` group with mode 620, so `write`
and `wall` reach it, and `/dev/pts/ptmx` is open to every user. The mount moves into the real root
with `/dev` and nothing mounts it again, so these are the booted system's options; the kernel's own
are the opener's group, 600 and 000.

## The splash

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=531-541 pin=f2e0a7926697 -->

The splash is started in the background, with its own input and output thrown away, and the four
helpers send it commands. Each helper does nothing when the splash is not carried, and returns 0
whatever happens, so a splash that fails can never stop the boot. The commands are listed in
[The splash](../../kdos/03-architecture/boot-and-init.md#the-splash).

On an installed machine `/dev/console` is the screen, so the script's first line,
`KDOS Init Starting...`, has already ended the console's deferred takeover of the framebuffer
before the splash starts, and every later line is drawn over the splash. The live entries print
on the serial line and leave the splash alone. See
[Known gaps](../../kdos/06-reference/known-gaps.md#on-an-installed-machine-the-boot-messages-are-drawn-over-the-splash).

## The rescue shell

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=543-577 pin=1f2cc107f7c6 -->

`rescue MESSAGE` is the end of every path that cannot boot. It prints the message, shows it on the
splash as a detail line and quits the splash, which hands the screen back. It then reads the active
consoles from `/sys/class/tty/console/active`; the last name there is the one `/dev/console` is.

- When that is a virtual terminal (`tty0`, `tty1` and so on), there is one screen. The message and a
  line saying this is a rescue shell are printed, and the script replaces itself with `/bin/sh`.
- Otherwise `/dev/console` is a serial line. The message goes to `tty1` as well, `setsid -f -c`
  starts an interactive shell there in a session of its own with `tty1` as its controlling terminal,
  and the script becomes the shell on the serial line.

In either case the shell on `/dev/console` is process 1, and leaving it panics the kernel. There is
no `reboot` program in the initramfs; see
[The rescue shell](../../kdos/03-architecture/boot-and-init.md#the-rescue-shell) for how to restart
from it, and [The initramfs](090-initramfs.md#the-gnu-programs) for the programs it has.

## Device manager

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=579-590 pin=dfaa5668d723 -->

`sp_total 2` counts this stage and `FILESYSTEM MODULES`, the two every boot runs. `udevd --daemon`
starts the device manager; the two `udevadm trigger` calls ask the kernel to replay the `add` event
of every subsystem and then of every device present since boot, which nobody was listening for; and
`udevadm settle` waits until [udev]{.idx} has handled them all. After this, `/dev` holds a node for every
disk the kernel has found, and the modules udev loads for the hardware are in.

## RAID

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=592-614 pin=50958f709bd4 -->

`mdadm --assemble --scan` reads the RAID superblock on each device and assembles every array it
finds, with no configuration file. Its exit status is thrown away, because a machine with no array
makes it fail. `udevadm settle` then waits for the arrays' own devices. This runs before anything
looks for a root, so a root on an array is visible to every lookup that follows.

## Filesystem modules

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=616-629 pin=4afab6607b04 -->

`loop`, `isofs`, `squashfs` and `overlay` are what the live path needs. A module that will not load
prints a line and the boot goes on; one that is built into the kernel loads as a no-op. When there
is no `/dev/loop0`, it is made by hand, block device major 7, minor 0.

## The kernel command line

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=631-654 pin=ab65e27c8608 -->

The script reads `/proc/cmdline` word by word and keeps five things: `root=UUID=`, `cryptdevice=`,
`bootstate=UUID=`, `kdos_slot=`, and `single` or `emergency`, which are handed to the real init at
the end. The live medium's entries pass `root=/dev/ram0`, which matches none of the patterns, so
`ROOT_UUID` stays empty there, and an empty `ROOT_UUID` is what sends the script down the live path.
`INIT_ARG` is cleared first because the kernel passes every `name=value` it does not recognise to
init as an environment variable, so a stray `INIT_ARG=` on the command line would otherwise arrive
set.

## The unlock function

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=656-682 pin=f3949433a167 -->

The comment sets out the `cryptdevice=` syntax and the two different UUIDs it involves. The two
`: "${VAR:=default}"` lines give `PASS_TTY` and `CRYPT_MAPPER_DIR` their real values unless the
environment already set them, which only the self-test does.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=684-735 pin=caa21598c504 -->

`unlock_root` accepts `UUID=<luks-uuid>:<name>` or `/dev/<node>:<name>`, and anything else is an
error. An empty name becomes `kdosroot`. Given a UUID, it asks `blkid -U` once a second, for up to
ten seconds, for the device holding that container. It fails when no device turns up, or when the
initramfs carries no `cryptsetup`.

Then it allows three tries. Each one shows the prompt on the splash, reads a line from `tty1` with
`read -r -s`, which does not echo, and feeds the passphrase to `cryptsetup open --key-file=-` on its
standard input. An attempt succeeds only when `cryptsetup` exits 0 and the mapped device exists
under `/dev/mapper`. The variable is cleared after every attempt. After the third wrong passphrase
the function returns 1, and its caller opens the rescue shell. The design of the prompt is in
[Unlocking an encrypted root](../../kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root).

The function is only defined here. It runs in the `UNLOCKING` stage, after the slot selection below,
which may change the container it is given.

## Boot slot

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=737-759 pin=780e010816fe -->

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=760-815 pin=75069784095f -->

On a machine with A/B slots, the command line names the ESP that holds the state file. The script
loads `vfat`, waits up to ten seconds for that ESP, and mounts it at `/esp`. Two questions are put
to `kdos-bootctl`, both against `/esp/EFI/kdos/bootstate`:

1. `select`, given the entry's `kdos_slot=` when it has one, prints the filesystem UUID to boot and
   counts the attempt in the state file. Its messages go to the console.
2. `crypt <uuid>` prints the LUKS container that filesystem is inside, if it is in one.

The ESP is unmounted at once. A UUID from `select` replaces `ROOT_UUID`, and a container from
`crypt` replaces `CRYPTDEV`, always under the mapper name `kdosroot`. When there is no ESP, no state
file or no answer, the script prints which, keeps the command line's `root=` and closes the stage as
passed: a machine whose slot state cannot be read still boots the root its menu entry names. The
verbs and the state file are in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection) and
[A slot knows its own container](../../kdos/03-architecture/boot-and-init.md#a-slot-knows-its-own-container).

## Volume groups

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=817-863 pin=bda25eb0d9fd -->

`activate_lvm` is called from three places: here, after the unlock, and on each pass of the wait for
the root. Each call asks `blkid -t TYPE=LVM2_member -o device` for the physical volumes and returns
at once when there are none, or when the list is the one the last call activated. The first call
that finds any loads the snapshot, thin, cache and write-cache targets with `modprobe -q -a`. Each
call that goes on adds a `VOLUME GROUPS` stage and runs `lvm vgchange -aay --sysinit`, which
activates every logical volume set to activate automatically; `--sysinit` leaves out the monitoring
daemon and background polling, and does not fail on locking, three things an initramfs cannot
provide. A failure marks the stage failed and nothing more; if the root was on that group, the root
lookup reports it missing.

The line after the function makes the first call only when `root=UUID=` or `cryptdevice=` is set, so
the live medium never runs `lvm` here. See
[Activating volume groups](../../kdos/03-architecture/boot-and-init.md#activating-volume-groups).

## Unlocking

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=865-880 pin=093a88f36da4 -->

When `CRYPTDEV` is set, by the command line or by the slot, `unlock_root` runs in a stage of its
own. Once it succeeds, udev is given time to settle, because the filesystem inside the container has
only just appeared, and `activate_lvm` runs again for a volume group inside the container. A failed
unlock is the first stage that opens the rescue shell.

## The installed root

### Root device

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=882-902 pin=93a2f192a555 -->

With `ROOT_UUID` set, this is a disk boot. The path adds its three stages and asks `blkid -U` for
the root once a second for up to ten seconds, running `activate_lvm` before each try so that a
volume group on a disk that appears late is activated in time. The first try is made at once: udev
has settled already, and on most machines it succeeds.

### Mounting and switching root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=903-949 pin=33b3fa389272 -->

The filesystem type is read with `blkid -o value -s TYPE`. An ext2, ext3 or ext4 root is checked
with `e2fsck -p`, which repairs what is safe to repair without asking. An exit status of 4 or more
means errors were left; the script prints the status, shows
`ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` on the splash and mounts the root anyway. The reasons are
in [Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root).

The root is mounted at `/newroot` with no type and no options, so `mount` probes the type and mounts
it read-write. The mount's own status is not tested; the test that follows is whether
`/newroot/sbin/init` is executable, which also fails when nothing was mounted. When it is, `/dev`,
`/proc` and `/sys` are moved into the new root, and [`switch_root`]{.idx} makes `/newroot` the root and runs
`/sbin/init` there as process 1, with `single` or `emergency` if the command line had one. `exec`
means no line of this script runs after it. When the root or its `/sbin/init` is not found, the
stage is marked failed and the next line opens the rescue shell.

## The live medium

### Waiting for USB storage

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=951-1054 pin=4d82dbf65d60 -->

These three functions are defined before the live path and called once, from the overlay stage,
to find the persistence store. `uptime_cs` sets `NOW` to the uptime in hundredths of a second.
`usb_scan` reads the USB device tree under `USB_DEVICES` and sets four variables:

- `USB_BUSY` names every hub port whose `state` is not `not attached`, `configured` or
  `suspended`, which is a device still being enumerated, and every interface of class `08` (mass
  storage) with no `host*/target*/*/block/*` beneath it, which is a stick usb-storage has not
  scanned yet.
- `USB_SEEN` lists the names of the USB devices, root hubs included.
- `USB_DISKS` lists the block devices found beneath the mass-storage interfaces.
- `USB_HUB` is 1 when a device other than a root hub has device class `09`, a hub.

`find_persist` calls `usb_scan` and then `blkid -L KDOS_PERSIST`, every tenth of a second. A
lookup that answers sets `PERSIST` and returns 0. Otherwise it returns 1 when `USB_BUSY` is empty,
the quiet time has passed and `USB_DISKS` is what it was at the scan before, or when 10 seconds
have passed since the first call. The quiet time is reset to now whenever `USB_SEEN` changes, and
to one second from now when a hub is present: a hub's empty ports report nothing between the hub
being configured and its first connection being seen. A disk's block device appears before the
kernel has read its partition table, so a lookup started then can miss the partition that holds
the store. That lookup opens the disk, and the open waits for the read, so when `USB_DISKS` has
changed the function makes one more lookup at once, without sleeping, before it gives up. A boot
from a USB stick therefore makes two lookups. Before its first sleep it prints
`Waiting for USB devices to settle before looking for a persistence store...`, and when the bound
is reached `USB storage still settling after 10s; no persistence store found`.

### Boot media

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1056-1124 pin=30b351fd293a -->

With no `root=UUID=`, the script looks for the medium the image was booted from. It tries every node
of `/dev/sr*`, `/dev/sd*`, `/dev/vd*`, `/dev/nvme*` and `/dev/mmcblk*` in turn, mounting each as
ISO9660 on `/mnt/iso` and looking for `system.sfs` at its top. It stops at the first that has one.

The scan repeats every tenth of a second until ten seconds of uptime have passed since it started,
with these rules:

- A node that cannot be opened for reading is passed over without a mount or a message: an
  optical drive with no disc, or a card reader with no card, whose open fails with `ENOMEDIUM` and
  which mount would only answer with `Can't open blockdev`. The size in
  `/sys/class/block/<name>/size` is no test for this, because an empty optical drive reports a
  guessed size rather than 0. The node is not recorded, so a disc that is still spinning up is
  tried on a later pass.
- `CHECKED` lists, space-separated, every node that has mounted. A node on that list is skipped on
  later passes, because a filesystem without `system.sfs` will not grow one; only nodes that have
  not mounted yet are retried.
- Only the first pass prints the name of each node it tries and mount's errors; later passes are
  silent.
- An unmount that fails sets `STUCK` and ends the scan, because a second mount on a busy `/mnt/iso`
  would stack on top of the first.

`FOUND=0` at the end, after the time is up or after a stuck unmount, opens the rescue shell with
`Failed to find KDOS installation media.` The scan relies on the medium's ISO9660 filesystem being
at the start of the whole-disk node; [The ISO](110-iso.md#xorriso) shows how the image keeps it
there.

### System image

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1126-1147 pin=342c388bc0e9 -->

`losetup -f` prints the first unused loop device, with `/dev/loop0` as the fallback. `losetup -r`
attaches `system.sfs` to it read-only, since it sits on a read-only filesystem, and the device is
mounted at `/mnt/system` as [squashfs]{.idx term="SquashFS"}. A failure of either opens the rescue shell, the first with
`Failed to set up a loop device for system.sfs` and the second with `Failed to mount system.sfs`.

### Overlay root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1149-1222 pin=397af640c193 -->

The root of a live session is an [overlay]{.idx}: `system.sfs` as the read-only lower layer, and a writable
upper layer where every change lands. The upper layer comes from one of two places.

1. Unless the command line has `nopersist`, `find_persist` looks for a filesystem labelled
   `KDOS_PERSIST`, waiting for USB storage as
   [Waiting for USB storage](#waiting-for-usb-storage) describes. When one exists and is not of a type that cannot hold an upper layer (vfat, exfat, ntfs,
   iso9660, squashfs), it is mounted at `/mnt/persist`, and its `upper` and `work` directories are
   used. If the overlay mount over it fails, the store is unmounted and the script goes on to the
   second choice.
2. Otherwise a tmpfs is mounted at `/mnt/overlay` and its `upper` and `work` directories are used,
   and the session lasts until the machine is switched off.

Every way the store can fail ends in the second choice and never in the rescue shell. The tmpfs
overlay mount is not checked. What the store is and how `kdos persist` makes one is in
[The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence).

### Moving the mounts

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1224-1279 pin=e6969a9e190d -->

The mount points the real init expects are made in the new root. Then, once `/sbin/init` has been
found in it, `umount -l` detaches the squashfs at `/mnt/system` and, on a boot in RAM, the tmpfs at
`/mnt/overlay`, and `rmdir` removes both mount points. The overlay took private copies of its
layers when it was mounted, so the root keeps reading them. The detach comes before `/proc` moves,
because util-linux `umount` finds its target in `/proc/self/mountinfo` and without it detaches
nothing. A missing `/sbin/init` skips it, so the rescue shell still sees both mounts.

`/dev`, `/proc` and `/sys` are then moved into the new root. Two more mounts are moved: `/mnt/iso`,
the medium itself, guarded by a test for `system.sfs` on it, and `/mnt/persist` when a store is in
use. Both have to be reachable by name from the booted system.

### Switching root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1281-1310 pin=eee243888baa -->

`udevadm control --exit` stops this `udevd`, and `switch_root` starts the real `/sbin/init` from
the overlay, as on the installed path. Before it does, it deletes the initramfs's own files to give
back their memory, and it never crosses into another mount: one mount still under `/mnt` would keep
`/mnt` from going, with `switch_root: failed to unlink mnt: Directory not empty` on the console,
which is why the squashfs and the tmpfs were detached above. The splash survives the switch, as
the comment says. A missing `/sbin/init` prints what `ls -l` can see of it and opens the rescue
shell. The last line of the script is a `rescue` too, so that `init` never runs off its end.

## See also

- [The initramfs](090-initramfs.md): how the tree this script runs in is built
- [The sequence](../../kdos/03-architecture/boot-and-init.md#the-sequence): every stage of a boot,
  from the firmware to the desktop
- [When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops): what each
  stage's failure looks like, and where to look
- [Finding the root](../../kdos/03-architecture/boot-and-init.md#finding-the-root)
- [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts):
  what `/sbin/init` runs next

<!-- book-nav -->
---

*Part XV. The image, chapter 354.* Previous: [353. The initramfs](090-initramfs.md) · [Contents](../README.md) · Next: [355. The pack store and the base pack](100-packs.md)
