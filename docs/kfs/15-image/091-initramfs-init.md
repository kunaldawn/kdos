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
`bash -n`, and runs four of its functions, `unlock_root`, `activate_lvm`, `find_persist` and
`persist_dev`, against stub programs. The variables `PASS_TTY`, `CRYPT_MAPPER_DIR`, `LVM_BIN` and
`USB_DEVICES` below exist so that it can; `persist_dev` has none, and the test points its copy at a
fake `/sys/class/block`.

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
| `VOLUME GROUPS` | `root=UUID=` or `cryptdevice=` is given, `lvm` is carried, and `blkid` reports physical volumes not yet activated | the stage is marked failed and the boot goes on |
| `UNLOCKING` | `cryptdevice=` is set | rescue shell: `Failed to unlock …` |
| `ROOT DEVICE` | `root=UUID=` is set | rescue shell: `Disk boot failed: no root at UUID=… with /sbin/init on it` |
| `MOUNTING ROOT` | the root was found | the same rescue shell, when `/sbin/init` is not on what was mounted |
| `BOOT MEDIA` | there is no `root=UUID=` | rescue shell: `Failed to find the KDOS live medium.` |
| `SYSTEM IMAGE` | the medium was found | rescue shell: `Failed to set up a loop device for system.sfs` or `Failed to mount system.sfs` |
| `OVERLAY ROOT` | `system.sfs` is mounted | a broken persistence store falls back to a session in memory; rescue shell only if `/newroot` is not a directory |
| `SWITCHING ROOT` | the last stage on both paths | live medium: rescue shell, `Error: /sbin/init not found in new root!` |

## The early mounts

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=532-550 pin=142f276345e9 -->

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

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=552-587 pin=aca8c21c488d -->

The splash is started in the background, with its own input and output thrown away, and the
helpers send it commands. Each helper does nothing when the splash is not carried, and returns 0
whatever happens, so a splash that fails can never stop the boot. `sp_root_release` runs
`KDOS_SPLASH_ROOT=/newroot kdos-splash release`, which reads the mounted root's
`/etc/kdos-release` (its `/etc/os-release` when it has none) and has the splash name that release
in place of the initramfs's copy, which an installed machine keeps across every update that is
not a slot image. It and `sp_release` are called on both paths before `/dev` moves into the new
root: the commands travel through the FIFO `/dev/.kdos-splash`, and once `/dev` has moved no helper
can reach it. On an `emergency` boot `sp_release` runs `kdos-splash quit`: init then runs
`sulogin` before `rcS`, the one thing that stops the splash on a normal boot, so a splash still
drawing would hide the prompt and keep the keyboard's console showing a frozen bar. On every other
boot it does nothing and the splash survives the switch. The commands are listed in
[The splash](../../kdos/03-architecture/boot-and-init.md#the-splash).

On an installed machine `/dev/console` is the screen, so the script's first line,
`KDOS Init Starting...`, has already ended the console's deferred takeover of the framebuffer
before the splash starts. A drawing splash puts the terminal into graphics mode, so on a quiet
boot the later lines go into the terminal's buffer rather than over the picture. The live entries
print on the serial line and leave the splash alone.

## The rescue shell

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=589-674 pin=89a4a9e20983 -->

`rescue MESSAGE [HINT]` is the end of every path that cannot boot. It shows the message on the
splash as a detail line, holds it there for `RESCUE_HOLD`, four seconds, on a quiet boot so it can
be read before the fade, and quits the splash, which hands the screen back. It runs `sync`, since a
failure after the root was mounted leaves a disk mounted read-write and `Ctrl+Alt+Del`, which the
kernel answers with an immediate restart until init takes it over, syncs nothing. It then reads the
active consoles from `/sys/class/tty/console/active`; the last name there is the one
`/dev/console` is.

- When that is a virtual terminal (`tty0`, `tty1` and so on), there is one screen, and the message
  is printed on it once. The message, the hint when one is given, a line saying this is a rescue
  shell and `RESCUE_RESTART`, `Ctrl+Alt+Del restarts at once; run sync first if you changed a
  disk.`, are printed, and the script opens an interactive shell on that terminal with
  `setsid -c -w`, which makes the terminal the shell's controlling terminal (`/dev/console` can
  never be one, and a shell there has no job control), and waits for it, in a loop, so an `exit`
  opens a fresh shell; every shell, whatever its exit status, is followed by a second's wait
  before the next, so one that cannot start, or one that reads end-of-file at once from a hung-up
  line, does not spin the loop.
- Otherwise `/dev/console` is a serial line. The message and the hint are printed there, and go to
  `tty1` as well with the rescue-shell line, `RESCUE_VERBOSE` (`The boot menu's '(verbose)' entry
  shows the boot log on this screen.`) and the same restart line, with `Devices tried:` and
  `RESCUE_TRIED` under the hint when a failed medium scan set it. `setsid -f` detaches a process
  that runs `setsid -c -w /bin/sh -i` on `tty1` in a loop, so each shell there is a session with
  `tty1` as its controlling terminal and an `exit` opens a fresh one. It is not a background job of
  the init's shell, which has no job control: bash would start the job with SIGINT ignored, and
  Ctrl+C on `tty1` would reach nothing. The script itself runs a shell on `/dev/console` in the
  same kind of loop, so an `exit` on the serial line, or an input that ends the shell, opens a
  fresh one.

Every fixed line and every hint is 80 columns or fewer: they are printed before fbcon's deferred
takeover has widened the console, and a longer line wraps.

Process 1 never returns from `rescue` on either branch, because its exit panics the kernel with
`Attempted to kill init`. There is no `reboot` program in the initramfs, so `Ctrl+Alt+Del` is the
restart; see [The rescue shell](../../kdos/03-architecture/boot-and-init.md#the-rescue-shell) for
how to restart from it, and [The initramfs](090-initramfs.md#the-gnu-programs) for the programs it has.

## Device manager

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=676-699 pin=94024aaa294d -->

`sp_total 2` counts this stage and `FILESYSTEM MODULES`, the two every boot runs. `udevd --daemon`
starts the device manager; the two `udevadm trigger` calls ask the kernel to replay the `add` event
of every subsystem and then of every device present since boot, which nobody was listening for; and
`udevadm settle` waits until [udev]{.idx} has handled them all. After this, `/dev` holds a node for every
disk the kernel has found, and the modules udev loads for the hardware are in.

`udev_release` is the end of this `udevd`, called on every path just before `/dev` moves into the
new root: `udevadm settle` drains what is still queued and `udevadm control --exit` stops the
daemon. One left running survives `switch_root` with its program and libraries deleted under it,
the root's `01_udev` starts a second beside it, and `kdos restarts` names it on every boot as a
process running replaced code.

## RAID

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=701-723 pin=50958f709bd4 -->

`mdadm --assemble --scan` reads the RAID superblock on each device and assembles every array it
finds, with no configuration file. Its exit status is thrown away, because a machine with no array
makes it fail. `udevadm settle` then waits for the arrays' own devices. This runs before anything
looks for a root, so a root on an array is visible to every lookup that follows.

## Filesystem modules

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=725-738 pin=4afab6607b04 -->

`loop`, `isofs`, `squashfs` and `overlay` are what the live path needs. A module that will not load
prints a line and the boot goes on; one that is built into the kernel loads as a no-op. When there
is no `/dev/loop0`, it is made by hand, block device major 7, minor 0.

## The kernel command line

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=740-769 pin=77729ffd3989 -->

The script reads `/proc/cmdline` word by word and keeps five things: `root=UUID=`, `cryptdevice=`,
`bootstate=UUID=`, `kdos_slot=`, and `single` or `emergency`, which are handed to the real init at
the end. The live medium's entries pass `root=/dev/ram0`, which matches none of the patterns, so
`ROOT_UUID` stays empty there, and an empty `ROOT_UUID` is what sends the script down the live path.
`INIT_ARG` is cleared first because the kernel passes every `name=value` it does not recognise to
init as an environment variable, so a stray `INIT_ARG=` on the command line would otherwise arrive
set.

## The unlock function

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=771-816 pin=ddbd2f350d11 -->

The comment sets out the `cryptdevice=` syntax and the two different UUIDs it involves. The three
`: "${VAR:=default}"` lines give `PASS_TTY`, `CRYPT_MAPPER_DIR` and `LOADKEYS` their real values
(`/dev/tty1`, `/dev/mapper` and `/bin/loadkeys`) unless the environment already set them, which
only the self-test does. `KEYMAP` starts empty.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=818-828 pin=d34648fbe975 -->

`esp_keymap` sets `KEYMAP` from the first line of `EFI/kdos/keymap` under the ESP it is given. It
takes the line only when it is a map name: not empty, not starting with `.` or `-`, and made only
of letters, digits and `_ . + -`. Anything else, or a missing file, leaves `KEYMAP` empty. The file
is on a FAT partition anybody holding the disk can edit, and `loadkeys` would read a path or an
option as readily as a name. The installer and kdos-settingsd write the file; the
[Boot slot](#boot-slot) block below calls this function.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=830-891 pin=0440d7683092 -->

`unlock_root` accepts `UUID=<luks-uuid>:<name>` or `/dev/<node>:<name>`, and anything else is an
error. An empty name becomes `kdosroot`. Given a UUID, it asks `blkid -U` once a second, for up to
ten seconds, for the device holding that container. It fails when no device turns up, or when the
initramfs carries no `cryptsetup`.

Before the first try, when `KEYMAP` is set, it runs `$LOADKEYS -q -u -C $PASS_TTY $KEYMAP` once,
so the passphrase is typed with the machine's console map. `-u` loads the map in Unicode mode,
because the passphrase was set in a UTF-8 session. A map that fails to load leaves the kernel's US
map, and the prompt runs anyway. With no `KEYMAP` the US map is the only one.

Then it allows three tries. Each one shows `DISK PASSPHRASE (attempt N of 3)` on the splash, or
`Disk passphrase (attempt N of 3): ` on `PASS_TTY` when no splash draws, never the mapper name,
reads a line from `tty1` with `read -r -s`, which does not echo, and feeds the passphrase to
`cryptsetup open --key-file=-` on its standard input. An attempt succeeds only when `cryptsetup`
exits 0 and the mapped device exists under `/dev/mapper`. The variable is cleared after every
attempt. A `cryptdevice=` that cannot be used returns 1, and the third wrong passphrase returns 2,
so that its caller can tell the two apart in the rescue shell's reason. The design of the prompt is in
[Unlocking an encrypted root](../../kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root).

The function is only defined here. It runs in the `UNLOCKING` stage, after the slot selection below,
which may change the container it is given.

## Boot slot

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=893-979 pin=695a2de0d53e -->

On a machine with A/B slots, the command line names the ESP that holds the state file. The script
loads `vfat`, waits up to ten seconds for that ESP, and mounts it at `/esp`. Two questions are put
to `kdos-bootctl`, both against `/esp/EFI/kdos/bootstate`:

1. `select`, given the entry's `kdos_slot=` when it has one, prints the filesystem UUID to boot and
   counts the attempt in the state file. When its decision moves the next boot, it also rewrites
   the boot menu on the ESP. Its messages go to the console. It is told to read `/proc/cmdline`
   (`KDOS_CMDLINE`), because a state file away from `/boot/efi` reads no command line otherwise,
   and the `kdos_menu=trial` there is how it tells a confirmed entry picked from a trial menu
   from a firmware fallback.
2. `crypt <uuid>` prints the LUKS container that filesystem is inside, if it is in one.

While the ESP is still mounted, `esp_keymap /esp` reads `EFI/kdos/keymap` into `KEYMAP` for the
unlock: the ESP is not mounted again before the passphrase prompt. Then the ESP is unmounted. A
UUID from `select` replaces `ROOT_UUID`, and a container from `crypt` replaces `CRYPTDEV`, always under the mapper name `kdosroot`. When there is no ESP, no state
file or no answer, the script prints which, keeps the command line's `root=` and closes the stage as
passed: a machine whose slot state cannot be read still boots the root its menu entry names. The
verbs and the state file are in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection) and
[A slot knows its own container](../../kdos/03-architecture/boot-and-init.md#a-slot-knows-its-own-container).

## Volume groups

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=981-1027 pin=bda25eb0d9fd -->

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

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1029-1049 pin=758c5dbefd1e -->

When `CRYPTDEV` is set, by the command line or by the slot, `unlock_root` runs in a stage of its
own. Once it succeeds, udev is given time to settle, because the filesystem inside the container has
only just appeared, and `activate_lvm` runs again for a volume group inside the container. A failed
unlock is the first stage that opens the rescue shell: three wrong passphrases with the reason
`Wrong passphrase three times. Ctrl+Alt+Del restarts and asks again.`, and a container that cannot
be opened at all with `Failed to unlock <cryptdevice>`.

## The installed root

### Root device

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1051-1071 pin=93a2f192a555 -->

With `ROOT_UUID` set, this is a disk boot. The path adds its three stages and asks `blkid -U` for
the root once a second for up to ten seconds, running `activate_lvm` before each try so that a
volume group on a disk that appears late is activated in time. The first try is made at once: udev
has settled already, and on most machines it succeeds.

### Mounting and switching root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1072-1122 pin=4ba9d8fd9145 -->

The filesystem type is read with `blkid -o value -s TYPE`. When the initramfs carries `e2fsck`, an
ext2, ext3 or ext4 root is checked with `e2fsck -p`, which repairs what is safe to repair without
asking. An exit status of 4 or more means errors were left; the script prints the status, shows
`ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` on the splash and mounts the root anyway. The reasons are
in [Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root).

The root is mounted at `/newroot` with no type and no options, so `mount` probes the type and mounts
it read-write. The mount's own status is not tested; the test that follows is whether
`/newroot/sbin/init` is executable, which also fails when nothing was mounted. When it is, the
splash draws `SWITCHING ROOT` and marks it done while `/dev` is still the initramfs's: its FIFO is
in `/dev`, and nothing this script sends after the move reaches it. `sp_root_release` has the
splash name the release of the root just mounted. `udev_release` stops this `udevd`, as on the live
path. Then `/dev`, `/proc` and `/sys` are moved into the new root, and [`switch_root`]{.idx} makes
`/newroot` the root and runs `/sbin/init` there as process 1, with `single` or `emergency` if the command line had one. Before
`/dev` moves, on an `emergency` boot, `sp_release` stops the splash and leaves the console in text
mode; on every other boot the splash survives the switch. `exec`
means no line of this script runs after it. When the root or its `/sbin/init` is not found, the
stage is marked failed and the next line opens the rescue shell.

## The live medium

### Waiting for USB storage

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1125-1228 pin=4d82dbf65d60 -->

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

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1254-1405 pin=039fd82e4d08 -->

With no `root=UUID=`, the script looks for the medium the image was booted from. It tries
`/dev/mapper/ventoy` and then every block node of `/dev/sr*`, `/dev/sd*`, `/dev/vd*`, `/dev/nvme*`
and `/dev/mmcblk*` in turn, mounting each that carries an ISO9660 descriptor on `/mnt/iso` and
looking for `system.sfs` at its top. `/dev/mapper/ventoy` is the device Ventoy's hook maps over the ISO file on a Ventoy stick;
on any other boot it does not exist. It stops at the first that has one and belongs to this boot,
and keeps its name in `MEDIUM` for the persistence store.

Which medium belongs to this boot is read from the command line before the scan starts:
`kdos.medium=<id>`, which every live entry of the medium's `limine.conf` carries, goes into
`WANT_MEDIUM`. A node holding `system.sfs` is taken only when the first line of its
`kdos-medium.id` equals that id; one with another id, or with no `kdos-medium.id`, is a second KDOS
medium attached to the machine, and the scan prints `<dev> is another KDOS medium (<id>), not
<wanted>`, unmounts it and goes on. Taking it would boot this kernel with another release's
`system.sfs` and its modules, and the installer would then call the wrong device the boot medium.
A command line without `kdos.medium=`, from an entry written by hand, takes the first node that
has `system.sfs`. The `Found KDOS media on <dev> (medium <id>)` line names the id it matched.
[The ISO](110-iso.md#limineconf) shows where the id comes from.

The scan repeats every tenth of a second until ten seconds of uptime have passed since it started,
with these rules:

- A node that cannot be opened for reading is passed over without a read, and `WHY` records
  `no medium` for it: an optical drive with no disc, or a card reader with no card, whose open fails with `ENOMEDIUM` and
  which mount would only answer with `Can't open blockdev`. The size in
  `/sys/class/block/<name>/size` is no test for this, because an empty optical drive reports a
  guessed size rather than 0. The node is not recorded, so a disc that is still spinning up is
  tried on a later pass.
- A node that opens is read before anything mounts it: `dd` takes the five bytes at 32769, where
  an ISO9660 primary volume descriptor carries `CD001`, through a plain read that claims nothing.
  A node without them is `not ISO 9660` and is never mounted. A mount opens its device
  exclusively, and Ventoy's hook builds `/dev/mapper/ventoy` over the stick while the scan runs:
  a mount of the stick or of its exFAT partition at that moment makes the mapping fail with
  `Device lookup failed (-EBUSY)`, and the boot is left without a medium. Only a node with the
  descriptor is mounted, and a mount that still fails records `ISO 9660 that does not mount`.
- `CHECKED` lists, space-separated, every node that has mounted, and every node other than an
  optical drive (`/dev/sr*`) without the descriptor or whose ISO9660 mount failed. A node on that
  list is skipped on later passes, because a filesystem without `system.sfs` will not grow one and
  a disk that is not ISO9660 will not become one. An optical drive is read again, for a disc still
  spinning up.
- `SEEN` lists every node the scan has come across. The first time a node is seen, on whichever
  pass, the scan prints `Checking <dev>...`, and that first mount alone prints its errors.
- `WHY`, an associative array, keeps what each node turned out to be: `no medium`,
  `not ISO 9660`, `ISO 9660 that does not mount`, `ISO 9660 without system.sfs` or
  `another KDOS medium (<id>)`, where `<id>` is
  `none` for a medium without `kdos-medium.id`.
- An unmount that fails sets `STUCK` and ends the scan, because a second mount on a busy `/mnt/iso`
  would stack on top of the first.

`FOUND=0` at the end, after the time is up or after a stuck unmount, prints one line,
`No KDOS media; tried:` and each node of `SEEN` with its `WHY`, and keeps the same list in
`RESCUE_TRIED` one node to a line (`  <dev>: <WHY>`, cut to 80 columns), at most eight lines and
then `  ...and N more; the serial line lists every one.`, or `  no block device appeared` when
`SEEN` is empty; `rescue` shows it on `tty1` under a serial console. It then opens the rescue shell
with `Failed to find the KDOS live medium.` and the hint `MEDIA_HINT`, `Write the whole ISO with dd,
Rufus DD mode or Ventoy; copied files cannot boot.` The scan relies on the medium's ISO9660 filesystem being
at the start of the whole-disk node; [The ISO](110-iso.md#xorriso) shows how the image keeps it
there.

### System image

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1407-1428 pin=342c388bc0e9 -->

`losetup -f` prints the first unused loop device, with `/dev/loop0` as the fallback. `losetup -r`
attaches `system.sfs` to it read-only, since it sits on a read-only filesystem, and the device is
mounted at `/mnt/system` as [squashfs]{.idx term="SquashFS"}. A failure of either opens the rescue shell, the first with
`Failed to set up a loop device for system.sfs` and the second with `Failed to mount system.sfs`.

### Overlay root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1430-1514 pin=c7851769605c -->

The root of a live session is an [overlay]{.idx}: `system.sfs` as the read-only lower layer, and a writable
upper layer where every change lands. The upper layer comes from one of two places.

1. Unless the command line has `nopersist`, `find_persist` looks for a filesystem labelled
   `KDOS_PERSIST`, waiting for USB storage as
   [Waiting for USB storage](#waiting-for-usb-storage) describes. When one exists and is not of a type that cannot hold an upper layer (vfat, exfat, ntfs,
   iso9660, squashfs), it is mounted at `/mnt/persist`, and its `upper` and `work` directories are
   used. A store on the disk in `MEDIUM` is mounted through a loop device that `persist_dev` sets
   up at the partition's offset and length, with direct I/O, because the medium's whole-disk mount
   makes the kernel refuse to open any partition on that disk; a store on another disk is
   mounted directly. If the mount or the overlay over it fails, the store is unmounted, the loop
   is detached, and the script goes on to the second choice.
2. Otherwise a tmpfs is mounted at `/mnt/overlay` and its `upper` and `work` directories are used,
   and the session lasts until the machine is switched off.

Every way the store can fail ends in the second choice and never in the rescue shell. The tmpfs
overlay mount is not checked. What the store is and how `kdos persist` makes one is in
[The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence).

### Moving the mounts

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1516-1583 pin=f42f252c86aa -->

The mount points the real init expects are made in the new root. The splash draws
`SWITCHING ROOT`, and the stage is settled here, before `/dev` moves, since the FIFO that carries
the splash's commands is in `/dev`. A missing `/sbin/init` fails the stage, prints what `ls -l`
can see of it and opens the rescue shell while `/dev`, `/proc` and both of the image's mounts are
still where the initramfs left them. Once `/sbin/init` has been found, `umount -l` detaches the
squashfs at `/mnt/system` and, on a boot in RAM, the tmpfs at `/mnt/overlay`, and `rmdir` removes
both mount points. The overlay took private copies of its layers when it was mounted, so the root
keeps reading them. The detach comes before `/proc` moves, because util-linux `umount` finds its
target in `/proc/self/mountinfo` and without it detaches nothing. The stage is then marked done,
`sp_root_release` has the splash name the release of the overlay root, and `udev_release` stops
this `udevd`.

`/dev`, `/proc` and `/sys` are then moved into the new root. Two more mounts are moved: `/mnt/iso`,
the medium itself, guarded by a test for `system.sfs` on it, and `/mnt/persist` when a store is in
use. Both have to be reachable by name from the booted system.

### Switching root

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1586-1605 pin=dc14930c2ff2 -->

`switch_root` starts the real `/sbin/init` from the overlay, as on the installed path. Before it
does, it deletes the initramfs's own files to give
back their memory, and it never crosses into another mount: one mount still under `/mnt` would keep
`/mnt` from going, with `switch_root: failed to unlink mnt: Directory not empty` on the console,
which is why the squashfs and the tmpfs were detached above. On an `emergency` boot `sp_release`
has already stopped the splash, before `/dev` moved, leaving the console in text mode; on every other boot the
splash survives it, as the comment says. The last line of the script is a `rescue` too, so that `init` never runs off its end.

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

*Part XV. The image, chapter 380.* Previous: [379. The initramfs](090-initramfs.md) · [Contents](../README.md) · Next: [381. The pack store and the base pack](100-packs.md)
