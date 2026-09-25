# Boot and init

This page traces the path from firmware to a login prompt and names the
component that owns each step. It is what to read when a boot stops somewhere
and the question is which program to look at. For the same path described from
the user's side, see [Getting started](../02-user-guide/getting-started.md).

## The sequence

| # | Stage | Run by |
|---|---|---|
| 1 | Firmware, BIOS or UEFI | The machine |
| 2 | Limine — from the ESP on UEFI, from the MBR on BIOS | The firmware |
| 3 | The kernel, with its command line | Limine |
| 4 | Early microcode load | The kernel, before any filesystem exists |
| 5 | The initramfs `init` | The kernel |
| 6 | The splash | `kdos-splash`, from the initramfs |
| 7 | A/B slot selection | `kdos-bootctl` |
| 8 | Encrypted root unlock and volume-group activation, if any | `cryptsetup`, `lvm` |
| 9 | Find and mount the root | The initramfs `init` |
| 10 | `switch_root` | util-linux's, never toybox's |
| 11 | `/etc/init.d/rcS` | init (toybox), as `sysinit` |
| 12 | The numbered service scripts ([boot order](../02-user-guide/administration.md)) | `rcS` |
| 13 | `kdos-bootctl mark-good` | `rcS`, last |
| 14 | `kdos-getty` on tty1 and tty2 | init, as `respawn` |
| 15 | `kdos-login`, which hands the tty to agetty | `kdos-getty` on tty1 |
| 16 | The desktop | `~/.bash_profile` (or `~/.zprofile` for a zsh login), on tty1 only |

Step 16 is [the session](session.md), and a login on tty1 reaches it without
anyone typing a command. A machine whose GPU driver fails to come up lands on
`tty2`, which is a plain getty and is the recovery console.

## Limine and the kernel command line

The boot loader is [Limine](https://limine-bootloader.org/), and it is the only
one. The same binary, the same configuration file and the same menu serve BIOS
and UEFI, so what a machine shows at power-on does not depend on how it
started. On UEFI the firmware loads `EFI/BOOT/BOOTX64.EFI`; on BIOS it runs the
boot code in the first sector, which finds `limine-bios.sys` by name in the
root, `/boot`, `/limine` or `/boot/limine` of a volume it can read.

`BOOTIA32.EFI` sits beside the 64-bit binary and the two never compete. A
64-bit CPU does not imply a 64-bit firmware — the early Atom tablets and a few
netbooks run this kernel and this userland and can load only a 32-bit EFI
binary — and firmware reads the one `EFI/BOOT/BOOT<arch>.EFI` it can execute,
never looking at the other. Both are built from one source by
`ports/core/limine`, both are on the ISO's ESP tree and on an installed
machine's, and the El Torito UEFI record gathers whichever were built with no
second switch. The cost is about a hundred kilobytes and one more freestanding
build.

An NVRAM entry names exactly one path, so the installer has to choose. The
removable-media fallback picks between the two by itself; `efibootmgr --create`
cannot, and an entry pointing at a binary the firmware cannot load is a boot
option that fails rather than one that falls through. `kinstall` reads
`/sys/firmware/efi/fw_platform_size` and writes the matching path. A kernel too
old to publish that file leaves the installer assuming 64 — which covers nearly
every machine, and where the assumption is wrong the 32-bit firmware still
boots through the removable fallback rather than not at all.

The entry also names a partition, and that number is measured from the kernel's
`/sys/class/block/<node>/partition` for the ESP the install actually wrote to,
never derived from the device name: only a wipe lays the ESP out at index 1, and
an install reusing existing partitions takes whichever one was chosen. Where the
index cannot be read the installer writes no entry and says so, because the
removable-media fallback still starts the disk while an entry naming the wrong
partition is a boot option the firmware cannot load.

### Where the kernel is kept

The kernel and initramfs are placed where the loader is certain to read them:
on the ISO9660 medium for the live image, and on the **ESP** for an installed
system. Limine reads FAT and ISO9660. It does not read the roots the installer
offers, which include xfs, f2fs and anything under LUKS, so a kernel on the
root filesystem would be a boot that depends on a driver the boot loader does
not have.

Three programs write the loader configuration and nothing else does.
`script/06_packaging/02_iso.sh` writes the live medium's
`boot/limine/limine.conf`; `kinstall` writes `limine.conf` at the root of the
installed machine's ESP, and `kdos-bootctl` rewrites that file's `/KDOS`
entries whenever the boot state changes — see
[One kernel per slot](#one-kernel-per-slot). That location is not arbitrary. Limine looks beside
its own EFI binary first and then at `/boot/limine/`, `/boot/`, `/limine/` and
`/` on each volume — and only that last set is searched on BIOS, so the root of
the ESP is the one path both firmwares find. A second copy beside `BOOTX64.EFI`
would be the copy that goes stale.

`fwupd` never touches the loader configuration. It writes the ESP only while a
firmware update is staged: a capsule under `EFI/kdos/fw/` with `EFI/kdos/fwupdx64.efi` and a one-shot
`BootNext` entry naming it, or a capsule under `EFI/UpdateCapsule/` for firmware
that takes one from disk. The boot order is not changed; the next boot after
the update is Limine's again.

### A new kernel

The `linux` package installs its kernel as `/boot/vmlinuz-kdos`, which nothing
boots until `kdos-bootctl deploy` copies it into its slot's directory on the
ESP. Each root slot boots its own kernel from `EFI/kdos/<slot>/`, because a
kernel's modules exist only in the root it was installed into.

The postinstall builds the new kernel's initramfs into the same root, as
`/boot/initramfs-kdos.cpio.gz`. It is the image's own `/boot/initramfs.cpio.gz`
with one archive appended. The base carries the microcode, which the early
loader finds only at the very start of the file, and the init. The appended
archive carries the new kernel's copies of the modules listed in
`/boot/initramfs.modules` — the list `01_initramfs.sh` carried, written beside
it — and their dependencies. A root with no such list gets the set the base
archive itself carries: every module under its `lib/modules/`, found by walking
past the uncompressed microcode archive to the compressed part. The kernel
unpacks concatenated archives into one tree and the init's `modprobe` looks
under `uname -r`, so it finds the new set. The init itself is therefore always
the one the image shipped.

Who deploys depends on the root:

| Installed into | Deployed by | When |
|---|---|---|
| The running system | The postinstall, with `kdos-bootctl deploy /` | At once; the next boot runs the new kernel |
| The inactive slot | `kdos update apply`, with `kdos-bootctl deploy <mount> <slot>` | After every package of the run went in, before `try` |

`deploy` writes the initramfs and then the kernel, each beside the old file,
flushed and renamed over it, so the directory never holds a new kernel beside an
initramfs without its modules. It refuses before writing anything when the ESP
cannot hold both files. It takes `/boot/initramfs-kdos.cpio.gz` where the
postinstall wrote one, and the image's initramfs otherwise; packaging deletes
the first from the image, so an ISO carries only the pair built together.

`deploy` also refuses an initramfs that holds module trees for other kernels and
none for this kernel's version, and writes nothing. That init could not load
`vfat` to read the boot state, so it would count no attempt and never roll back,
and it could not mount a root on xfs, f2fs, LUKS or md. The slot keeps the
kernel it had, and `kdos update apply` does not try it. An initramfs with no
module tree at all passes, as does one that cannot be read.

On a machine with no boot state, `deploy` writes into the directory that the
first `/KDOS` entry's `path:` names: `EFI/kdos/a/` on anything kinstall wrote,
the flat `EFI/kdos/` on an older menu.

In every root, the postinstall keeps the module tree of every kernel on the
ESP, which it reads out of each bzImage's header, as well as the running
kernel's. Until a root's own kernel is deployed, the next boot of that root runs
one of those, and a root stripped of its modules boots with no GPU, network or
sound driver.

### The menu

The menu counts down for ten seconds, on the live medium and on an installed
system alike, and a single keypress stops it and leaves it up. That countdown
is wall time spent before the kernel exists with nothing else running, so it is
bought rather than free. What it buys is reachability: the verbose boot and
memtest86+ on both menus, the clean session on the live medium and single user
on an installed machine, are reachable from nowhere else, and a countdown short
enough to miss makes them unreachable on exactly the machine that needs them.
Setting `timeout: 0` does not draw a menu at all — it boots the default entry
immediately, and takes those entries with it.

The menu is drawn as a character grid in the accent in force, in the console's
own Terminus face, over a dimmed full-bleed backdrop. See
[Theming the boot menu](../02-user-guide/theming.md) for what is configurable
and [the design language](design-language.md) for where the colours come from.

`kcol_limine_conf()` emits the whole look, and both writers call it. The ISO
step reaches it through `kdos-bootctl theme --print`; `kinstall` links it
directly. Two hand-copied sets of colour literals is how a stick and the
machine installed from it end up different colours, and the one that is wrong
is the one nobody is booting that day. The only lines the writers own are the
two that name files rather than colours — `wallpaper` and `term_font`.

Those two are the one place the writers cannot share code, because the files
are not in the same place for both. `psf2limine.py` converts the console face
during packaging and writes `font.bin` straight into the ISO tree, so it exists
on the ISO9660 volume and nowhere in the root filesystem: `kinstall` looks for
it at `/boot/limine/font.bin`, then at `/mnt/iso/boot/limine/font.bin`, which is
where the initramfs mounts the medium. The backdrop is the opposite — it is
shipped by the `fs/` overlay, so both writers read
`/usr/share/kdos/boot/kdos-backdrop.png` and fall back to `kdos-banner.png`, and
an install run from a system with no medium mounted still gets its artwork.
Either file missing is survivable: Limine draws its own face on a plain
backdrop and the entries are unchanged.

### Why the plate is opaque

The menu's plate is opaque and its backdrop is stretched. Both are legibility
decisions rather than aesthetic ones.

`term_background` carries a leading transparency byte. Made transparent, there
is no plate and the artwork may be any size — but Limine prints
`linux: Loading kernel …` at the terminal's own origin the moment an entry is
picked, and with no plate to draw it on those lines land across the artwork.
Opaque, the loading text can only ever appear inside the plate. A `centered`
wallpaper is drawn at its own size in the middle of the screen, which is
exactly where the menu is, so the style is `stretched`. `term_font_scale` is
`1x2`: doubling both axes of an 8x16 face fills a 1080-row screen with four
entries.

No placement avoids the loading text at every resolution, which is why the
plate is opaque rather than the artwork being moved. The text's position is in
pixels from the margin; the artwork's is a fraction of a stretched wallpaper. A
banner that clears the text at 1080 lines runs underneath it at 720.

### The margin ceiling

A margin that leaves the terminal under sixteen rows makes Limine abandon the
graphical terminal altogether, and that is the ceiling `term_margin` is set
against. Measured by booting one ESP per value: at 1280x800 with these keys,
144 keeps sixteen rows and themes correctly, and 152 leaves fifteen and comes
up in Limine's own font, its own palette, its own branding and no wallpaper at
all. That is not a degraded theme; it is no theme.

A row is `term_font_size` times `term_font_scale` — 16 by 2 — so the ceiling is
`(height − 512) / 2`: 144 at 800 lines, 104 at 720, 44 at 600. The shipped
value is 100, which clears 720 and everything above it.

`interface_branding` is emitted **empty**, and empty is not the same as absent.
The artwork is the branding, so a value would print "KDOS" in a font directly
under a picture of it — but leaving the key out does not remove it. Limine
falls back to its own default and prints `Limine <version> (x86-64, UEFI)` in
its own cyan.

### The artwork

The backdrop is the penguin and the wordmark, sized to fit the margin.
`genbackdrop.py` lays them on a floor of the scheme's `deep`. Four of its
constants are measured rather than chosen, and each fixes something that looks
like a rendering defect:

- **Height is 6.3% of the screen, because the margin is 100 pixels.** The
  wallpaper is `stretched`, so the artwork scales with the screen while the
  margin does not: it has to clear 100 pixels on the tallest screen it will be
  seen on. 6.3% is 68 pixels at 1080 lines and 90 at 1440 — the largest
  resolution the tree references — and is covered by the plate above about 1580.
- **The captions are dropped.** At this height the two lines of small print are
  three pixels tall; including them costs the wordmark a third of its own height
  to render something illegible.
- **The ink is the max channel, not the luminance.** The wordmark is phosphor
  green, `(57,255,20)`, which Rec.601 puts at 169 while the penguin's white
  lands at 249 — so a plain greyscale conversion leaves the letters visibly
  duller than the mascot beside them.
- **The black point is 64, and that is what stops the rectangle.** The banner
  carries an ambient green glow over its whole area: its corner pixel is
  `(18,64,31)`, not black, so composited onto the floor it lifts its own area
  above the field and draws a box around itself. 64 is the 95th percentile of
  the border ring. Subtracting it takes the border to exactly zero at a cost of
  2% of the strong ink.

The downscale is `BOX` — area averaging — because the wordmark is pixel art,
and a photographic filter on hard edges gives blocks of different widths with
soft jagged edges. There is no vignette and no texture: a wordmark folded onto
itself reads as grain at full size and as smudges at a sixth of it, which bands
in the corners. A flat floor cannot band. The generator is a host script whose
output is committed, the same arrangement `genbanner.py` and `genlogo.py` use.

### Restamping an installed machine

`kdos-bootctl theme <accent>` rewrites the theme in place. It touches only the
keys the theme owns and leaves every entry, `cmdline`, `default_entry` and the
A/B state alone — a restamp that rewrote the file from a template would discard
a slot somebody is mid-rollback on. `wallpaper_style` and `term_font_scale` are
among the keys it owns, because they are layout: a machine installed before a
retheme picks up the new arrangement rather than keeping `2x2` over `centered`.

The write is temp file, fsync, rename, fsync the directory. The ESP is FAT and
a zero-length `limine.conf` is a machine that shows no menu.

`kdos theme` reaches this through `kdos-powerd accent`, which also writes
`/etc/kdos/accent` for the splash. A machine with no writable ESP skips the
boot half and says so; the desktop still retints.

### Command-line parameters KDOS reads

| Parameter | Read by | Meaning |
|---|---|---|
| `root=UUID=…` | The initramfs | The filesystem to mount as root |
| `cryptdevice=UUID=…:<name>` | The initramfs | A LUKS container to open first |
| `bootstate=UUID=…` | `kdos-bootctl` | Which ESP holds the A/B state file |
| `console=` | The kernel | The last one wins, and it is a serial port here |

That last row explains two things that otherwise look like defects. Because the
final `console=` is a serial port, kernel and initramfs messages go to the
serial line and the display shows nothing — which is exactly what makes the
graphical splash possible. And it is why the encrypted-root prompt is drawn
through the splash and read from `/dev/tty1` explicitly, rather than through
`/dev/console`, which nobody is looking at.

## Microcode

The kernel is built `CONFIG_MICROCODE=y` with late loading off, so its early
loader is the only path there is. That loader runs before any filesystem exists
and scans the raw initrd for two literal paths:

```
kernel/x86/microcode/GenuineIntel.bin
kernel/x86/microcode/AuthenticAMD.bin
```

The image the boot loader hands over is therefore two archives concatenated: a
plain, uncompressed cpio containing those two files, followed by the gzipped
initramfs.

Three properties must hold.

Nothing in the microcode archive may be compressed — not the archive and not
the blobs inside it. The firmware package ships AMD's microcode compressed
because the *runtime* loader can decompress; the early loader cannot, so the
build expands them on the way in.

The Intel bundle is not curated. It is upstream's whole set concatenated, minus
the images upstream ships separately because they need coordinated firmware
support. A per-family prune boots on the machines it covers and leaves the rest
silently unpatched.

The Intel bundle must be consumed to exactly its last byte. The kernel's
scanner returns nothing at all if any trailing data remains, so one stray file
in the directory means *no* microcode is loaded rather than "everything before
the bad record". The build walks the records the same way and fails rather than
shipping such an image.

An initramfs rebuilt without this step has no symptom — the processor keeps
whatever the firmware loaded — so `kdos doctor` runs the kernel's own search
against the image and reports the running revision.

## The initramfs

The build generates it. It carries a shell, a module set, and the handful of
programs early boot needs.

The module list is load-bearing, because a root filesystem whose driver is
missing installs perfectly and never boots again:

```
storage and buses  overlay squashfs isofs cdrom sr_mod loop sd_mod
                   ata_piix ahci libahci
                   virtio virtio_blk virtio_pci virtio_scsi
                   xhci-pci xhci-hcd ehci-pci ehci-hcd ohci-pci ohci-hcd
                   usb-storage uas
the ESP            vfat nls_cp437 nls_iso8859-1
root filesystems   xfs f2fs
LUKS               dm-crypt dm-mod aes_generic aes_x86_64 aesni-intel xts
                   sha256_generic sha512_generic crypto_null algif_skcipher
software RAID      md_mod raid0 raid1 raid10 raid456 dm-raid
```

ext4 and btrfs are built into the kernel; xfs and f2fs are modules and appear
here for exactly that reason. **Every filesystem the installer offers must be
in this list** — `ki_filesystems[]` is what offers the choice and this is what
makes the choice bootable.

The LUKS ciphers are the ones a LUKS2 default header actually uses, carried
unconditionally: they are small, and a kernel that has them built in makes the
copy step a no-op. The RAID personalities are listed individually because
`md_mod` loads none of them, and a machine whose data disks are an array needs
them before udev settles — without `md_mod` the members are bare disks with a
superblock nobody reads, which looks like an empty drive rather than a missing
module.

The list is also written to the image as `/boot/initramfs.modules`, which is
what a later kernel's initramfs is built from — see [A new kernel](#a-new-kernel).

`cryptsetup` and its libraries are carried only when they are installed, and
the build says so when they are not. A half-carried `cryptsetup` fails at the
passphrase prompt rather than at build time, which is the wrong place to find
out.

`lvm` and `dmsetup` are carried whenever lvm2 is installed, and lvm2 arrives
with `cryptsetup` and `parted` on every image that has them. They are copied to
`/usr/sbin`, the paths lvm2's udev rules name, rather than to `bin/`: the rules
travel with the rest of `/usr/lib/udev`, and `95-dm-notify.rules` runs
`/usr/sbin/dmsetup udevcomplete` for every device-mapper change. `lvm`, and
`cryptsetup` opening a container, wait for that call with no timeout, so a
`dmsetup` the rule cannot find hangs the boot at the unlock or the activation
with nothing on screen. Their libraries are not listed by hand: `copy_closure`
reads each program's `NEEDED` entries with `readelf` and copies the libraries
they name, and theirs, from `/usr/lib` or `/lib`. For `lvm` that is
libdevmapper, libdevmapper-event, libaio, libblkid, libudev, readline and
libnvme with what libnvme links. The build stops when lvm2 is installed and
either program is missing, when a rule names `dmsetup` or `lvm` anywhere but
`/usr/sbin`, and when a library in the closure is installed nowhere. The reader
is proven first: `readelf`, or `llvm-readelf` in its place, must find libblkid
among `blkid`'s `NEEDED` entries, or the build stops. A reader that is missing
fails inside a command substitution, which `set -e` does not catch, and every
program would be copied without its libraries.

`kdos-bootctl` follows the same rule, and it needs `libpng16` to do so.
`/usr/bin/kdos-bootctl` is a symlink to `/usr/sbin/ksvc`, which links libpng
for `kdos theme`'s wallpaper retint, so the copy carries that dependency into
an initramfs whose other unconditional libraries are libc, libintl, libblkid,
libuuid, libudev, libkmod, liblzma, libz, libzstd and bash's three. libpng16 is
carried beside it — libz is already there and musl's libm is inside libc, so
those two close the set — and the copy is skipped entirely when libpng is
absent. A `kdos-bootctl` that cannot exec makes A/B slot selection never run,
and the machine then reads as one whose slot was never marked good rather than
one missing a library.

### Finding the root

Both paths that look for a device poll, and both give up after ten seconds. The
disk path asks `blkid` for the root UUID once a second. The live path rescans
`/dev/sr* /dev/sd* /dev/vd* /dev/nvme*` every 100 ms and mounts the first one
holding `system.sfs`. Neither waits before its first attempt: udev has already
settled by then, so on a machine whose medium is enumerated the first pass
succeeds and costs nothing. The ten-second bound covers the slow cases — a USB
stick, a device behind a bridge.

Every pass walks every device class again, because the first node to answer is
not always the one holding the medium and a class that has not appeared yet
must still get its chance. A device that mounts is inspected only once, though:
a filesystem without `system.sfs` on it will not grow one, so it is remembered
and skipped, and only nodes that have not mounted yet are retried. That is one
mount/umount pair for a wrong disk across the whole scan rather than one per
pass.

`/mnt/iso` is the only mount point the scan has, so the umount is checked. A
mount point left busy would take a second filesystem stacked on top of it and
every later test would read the wrong one, so a failed umount stops the scan
and says so. Only the first pass narrates each device it tried; a hundred
repetitions of the same two lines would bury the message that explains a failed
boot.

### Checking the root

An ext2, ext3 or ext4 root is checked with `e2fsck -p` before it is mounted,
which is the only point at which nothing is using it. `e2fsck` is carried with
`libext2fs`, `libcom_err` and `libe2p` or not at all. Exit 1 and 2 mean it
repaired something, and the root is not mounted, so there is nothing to reboot
for. Exit 4 and above left errors behind: the splash says so and the boot goes
on, because a shell on the serial console is one nobody at the screen can
reach. btrfs, xfs and f2fs are not checked here; each checks itself at mount
time, which is why the installer gives them pass number 0.

Every other filesystem with a pass number is checked before it is mounted.
`rcS` runs `fsck -A -R -T -a` before `mount -a`, which checks every one whose
device exists before udev — in practice the ESP, which is FAT with no journal
and holds the A/B boot state; errors it could not correct reach the splash. A
filesystem on a logical volume the initramfs did not activate — any volume on
a live boot, or on a disk that appeared after the initramfs ran — has no device
until `03_lvm` activates its volume group, so `rcS`'s pass cannot check it;
`03_lvm` runs
`fsck -A -R -M -T -a` once the group is active, which checks only what is not
yet mounted, then mounts it, and reports errors left uncorrected in its own
log. Both passes write to `/run/kdos-fsck.log`.

## The splash

`kdos-splash` draws a CRT power-on directly to `/dev/fb0`, with glyphs scaled
up from the shipped console font. It is a static binary of roughly 1,200 lines.

It works because of the `console=` fact above: nothing prints to `tty0`, and
the kernel is built with deferred framebuffer-console takeover, so the
framebuffer is free for a process to draw on.

Three facts constrain it.

Deferred takeover means nothing is being scanned out yet. Writing to `/dev/fb0`
before the display driver's own client does its mode set paints a buffer nobody
is looking at. One byte written to `/dev/tty0` ends the deferral; the splash
writes a clear-and-hide-cursor sequence, which ends it and hides the cursor in
one action.

Memory-mapped writes need an explicit flush. They reach the display only when
the deferred I/O worker gets round to it, so each frame ends with a pan-display
call to the offset it is already at.

The process survives `switch_root` with a ghost root. It is never `chroot`ed,
so afterwards its `/` is the deleted initramfs root. Open file descriptors keep
working — that is the whole trick, one process spanning both halves of boot —
but every path it resolves *by name* after that points into the ghost. The
`quit` client therefore detects the daemon by opening the FIFO and watching for
a specific error, and does the cleanup itself from the real root.

Adding a stage is one line on either side: a step-and-ok pair in the generated
init, or in `rcS`, which already wraps every service script in one.

The progress total is additive — each phase adds its own step count as soon as
it knows it — so `done == total` happens at every phase boundary. The bar
therefore clamps one segment short of full until `quit` arrives. A boot that
shows 100% before it has finished is a bug report waiting to happen.

Iterate on the artwork without booting:

```sh
kdos-splash preview 1280x800 0.35 out.ppm
```

## Unlocking an encrypted root

The kernel command line carries `cryptdevice=UUID=<container>:<name>`, using
Arch's syntax because it is the one already in people's heads. The generated
init unlocks before it looks for a filesystem, because the filesystem named by
`root=` does not exist until then.

Three things the prompt gets right, each of which is a way this usually goes
wrong:

- It draws through the splash, not on `/dev/console`, which is a serial port.
- It reads keystrokes from `/dev/tty1`, which is where the keyboard is.
- It feeds the passphrase to `cryptsetup` on stdin, never as an argument:
  `/proc/<pid>/cmdline` is world-readable for the life of the process.

Three attempts, then a shell rather than a reboot loop. There is no
per-keystroke feedback, because the splash owns the framebuffer and the shell
owns the terminal. That is stated on screen rather than hidden.

## Activating volume groups

A root on an LVM logical volume boots with the ordinary command line: `root=`
names the UUID of the filesystem on the volume, as it would for a partition.
The generated init activates volume groups before it looks for that UUID,
because a filesystem on a logical volume has no device node until its group is
active.

It activates on each side of the unlock, because LVM and LUKS stack both ways.
A volume group inside a container exists only after the unlock, and a container
on a logical volume needs its group active before the unlock. It activates again
on each pass of the ten-second wait for the root, so a physical volume on a disk
that enumerates late still brings up its group before the wait gives up. Each call first
asks `blkid -t TYPE=LVM2_member` for the physical volumes, and runs `lvm
vgchange -aay --sysinit` only when that set is not empty and differs from the
set it last activated. A disk boot without LVM therefore costs one `blkid` per
call and never starts `lvm`. A live boot, which has neither `root=` nor
`cryptdevice=`, makes no call at all, and its groups are activated by `03_lvm`
after `rcS`. A failed activation marks the stage failed on the splash and the
boot goes on; if the root was in that group, the root lookup reports it
missing.

`--sysinit` turns off dmeventd monitoring, background polling and locking
failures, none of which an initramfs can provide. No `lvm.conf` is carried, so
the compiled defaults apply and every group found is activated. The groups stay
active across `switch_root`, and `03_lvm`'s own `vgchange` leaves them as they
are.

A thin or cache volume cannot be the root. Activating either runs `thin_check`
or `cache_check`, and thin-provisioning-tools is not in the initramfs.

The installer does not create LVM and does not offer a logical volume as the
root: it lists whole disks and their partitions only. A root on LVM is set up by
hand.

## A/B slot selection

Two root partitions, a state file on the ESP, and a boot that can change its
mind:

```
slot_a   = <filesystem uuid>
slot_b   = <filesystem uuid>
crypt_a  = <luks uuid>      the container that filesystem is inside, or empty
crypt_b  = <luks uuid>
active   = a                the slot known to work
try      = b                a candidate, or empty
attempts = 3                how many boots it gets
```

The counting lives in the initramfs, and that placement is the design. `rcS` is
the wrong place: a kernel that boots into a wedged userland must still spend an
attempt, and the `rcS` in that userland never runs to say so. `kdos-bootctl
select` decides and decrements in one step, before anything is mounted, and
prints the UUID to boot.

`kdos-bootctl mark-good` is the other half and runs at the *end* of `rcS`,
after every service that was going to fail has had its chance. A bad update
therefore boots its allotted number of times and rolls itself back with no help
from anything.

The state file lives on the ESP, which is FAT and has no journal. A torn write
there does not fail an update, it bricks the machine: the initramfs cannot tell
which slot to boot. So every write is temporary file, `fsync` the **file**,
`fsync` the **directory**, then rename. The directory `fsync` is the step
people leave out, and without it the rename can be lost while the data
survives.

A state file that does not parse is treated as absent, never as partial. Absent
means "use the `root=` the command line already carries", which is what a
single-root machine does anyway. A `try` pointing at a slot with no root, or at
the active slot, is refused rather than recorded. An unknown key is skipped,
which is what makes a state file written before `crypt_a` existed read as a
machine with no containers — because that is what it is.

### A slot knows its own container

`slot_a` names a **filesystem** and `crypt_a` names what that filesystem is
inside. Keeping the two apart is what joins A/B to encryption. On an encrypted
machine the root filesystem lives in a LUKS container, and the kernel command
line can name exactly one `cryptdevice=`. Two slots inside two containers
cannot both be named there, so the second is recorded per slot in the state
file and the initramfs asks for it *after* `select` has chosen:

```sh
SEL=$(kdos-bootctl select "$BOOT_SLOT")    # the filesystem, and one attempt spent
SLOT_CRYPT=$(kdos-bootctl crypt "$SEL")    # its container, if it has one
```

`crypt` is keyed by the filesystem UUID rather than by a slot name, so it is
one call with nothing carried between the two. `select` may have rolled back,
and asking "which slot did that turn out to be" would be a second decision that
could disagree with the first. Both reads happen while the ESP is still
mounted; the second reads the same file the first just wrote.

A slot that names no container leaves `cryptdevice=` exactly as the command
line set it, which covers every unencrypted machine and every machine whose two
slots share one container. The mapper name is the initramfs's own and never the
state file's: only one container is open at a time there, so a per-slot name
would disambiguate nothing, and a name read out of a file on the ESP is a name
somebody can edit into a path.

Without this split, selecting slot B unlocks slot A's container and then looks
for B's filesystem inside it. There is nothing there, and the failure reads as
a corrupt filesystem rather than as a lookup that was never made.

### One kernel per slot

Each slot boots the kernel in its own ESP directory, and the modules for that
kernel exist only in that slot's root:

```
EFI/kdos/a/vmlinuz              slot A's kernel
EFI/kdos/a/initramfs.cpio.gz    and the initramfs built with it
EFI/kdos/b/...                  slot B's, once an update has deployed it
EFI/kdos/bootstate              the state file above
```

Limine chooses the kernel before anything of ours runs and has no boot
counting, so the menu is part of the state. `kdos-bootctl` regenerates the
`/KDOS` entries of `limine.conf` on every change — `set-slot`, `try`, `deploy`,
`select`, `mark-good` — and writes the file only when the text differs:

| Entry | Boots |
|---|---|
| `/KDOS`, `(verbose)`, `(single user)` | The slot `select` will choose next: the candidate while it has attempts left, else the active slot |
| `/KDOS (slot <x>)` | The other slot, when it has a root and a kernel |

`default_entry` points at the first `/KDOS` entry. Every other line of the file
— the theme, the timeout, memtest86+ — stays where it is. The command line comes
from the first `/KDOS` entry: `root=` and `kdos_slot=` are set per slot, the
verbosity per entry, and every other word carries over, including anything
added by hand.

Every entry carries `kdos_slot=<x>`, and the initramfs hands it to `select`.
That is how a hand-picked entry is recognised: its slot is not the one the menu
leads with. It boots its own slot and no other, because the running kernel is
that slot's.

- **The confirmed slot picked while a candidate is on trial** abandons the
  candidate. That entry is the way back from a candidate kernel that dies before
  the initramfs can count anything.
- **Any other hand-picked entry** is one boot of that root. No attempt is spent
  and nothing is confirmed, except a candidate picked after its last attempt,
  which is still the candidate and is confirmed by `rcS` as usual.

The last attempt moves the menu's lead back to the active slot. A candidate that
fails that boot is therefore rolled back by the next boot of the confirmed
slot's own kernel, with no reboot in between.

`try` refuses a slot that has no kernel on the ESP once any slot has a directory
of its own. The menu could not lead with it, so the confirmed slot's entry would
boot, and `select` would read that as a hand pick and abandon the candidate.

An ESP that no `deploy` has touched holds one flat pair, `EFI/kdos/vmlinuz` and
`EFI/kdos/initramfs.cpio.gz`, that either slot boots. `kdos-bootctl` leaves a
menu with no per-slot directory exactly as it is. A slot without a directory of
its own boots the flat pair, and the pair is deleted once no entry names it.
The next `kdos update` of each slot therefore moves that slot into its own
directory.

The init inside an initramfs is the image's, and a `linux` update only appends
modules to it. An init that does not read `kdos_slot=` still counts and rolls
back, but leaves the menu alone, so its rollback boots the confirmed root on the
candidate's kernel. `mark-good` regenerates the menu on every boot, confirmed or
not, so a root whose `kdos-bootctl` has this regeneration corrects the menu on
the first boot that reaches the end of `rcS`.

Two kernels and two initramfs images fit many times over in the 512 MiB ESP
`kinstall` creates. An install that reuses a smaller ESP gets a warning when
there is no room left for a second kernel.

## Tools that must not be toybox's

Toybox provides applets under names that also belong to full implementations,
and `$PATH` puts `/usr/bin` ahead of `/usr/sbin`. The toybox recipe switches
off every applet whose name another port on the image installs, so each name
has one owner and resolves to the real tool everywhere. Where toybox's
directory differs from the real tool's, phase 1 switches the applet off too:
phase 1 installs toybox outside the package database, so a name it plants is
owned by no package — the later port install replaces `/usr/bin/toybox`
without removing the symlink, and the orphan sweep works from the database and
never sees it. Phase 1 switches off `netcat` and `ulimit` for the same reason:
no port installs either name at any path, so a link planted there would outlive
the applet. A name in the same directory as the real tool needs no phase-1
change, because the real port's install replaces the symlink.

| Switched off | Because the image has |
|---|---|
| `mount`, `umount`, `losetup`, `swapon`, `swapoff`, `mkswap`, `switch_root`, `blkid`, `blkdiscard`, `blockdev`, `dmesg`, `kill`, `linux32`, `nsenter`, `unshare`, `rtcwake`, `fsfreeze`, `hwclock`, `pivot_root`, `mountpoint`, `eject`, `fallocate`, `flock`, `logger`, `renice`, `ionice`, `chrt`, `taskset`, `uclampset`, `setsid`, `rfkill`, `rev`, `cal`, `mcookie`, `uuidgen`, `getopt`, `prlimit` | util-linux |
| `ps`, `top`, `free`, `pgrep`, `pkill`, `pidof`, `pmap`, `pwdx`, `sysctl`, `uptime`, `vmstat`, `w`, `watch` | procps-ng |
| `chvt`, `deallocvt`, `openvt` | kbd |
| `lspci`, `lsusb`, `killall`, `iotop`, `i2c*`, `gpiodetect`, `gpioget`, `gpioinfo`, `gpioset`, `partprobe`, `nc` | pciutils, usbutils, psmisc, iotop, i2c-tools, libgpiod, parted, netcat |
| `readelf`, `strings`, `cmp`, `clear`, `reset`, `setfattr`, `bunzip2`, `bzcat`, `gunzip`, `zcat`, `lsattr`, `chattr`, `insmod`, `lsmod`, `rmmod`, `modinfo` | binutils, diffutils, ncurses, attr, bzip2, gzip, e2fsprogs, kmod |
| `nologin`, `login`, `su`, `tar`, `patch`, `file` | shadow, tar, patch, file |
| `netcat`, `ulimit` | no such command — `nc` is the netcat port's, `ulimit` is bash's builtin only |

The applets are not drop-ins, and each difference is a feature the machine
would lose: toybox's `swapon` and `swapoff` refuse `-a`, so with them the swap
the installer writes into `fstab` is never turned on; its `umount` has no
`-R`; its `mount` never runs a `mount.<type>` helper and does not know
`nofail`; its `lspci` and `lsusb` have no `-d`, which `airmon-ng`'s driver
detection rests on.

`sed`, `find`, `xargs`, `awk`, `expr` and `ln` stay in toybox. Every configure
script between toybox and the GNU ports runs them, and phase 1 has no other
copy. The GNU ports come later in dependency order and take the names over; an
upgrade of toybox alone puts its applets back until they are reinstalled.

### `blkid`

Every lookup the initramfs makes is `blkid -U <uuid>`: the root filesystem, the
ESP that holds the A/B state, and the LUKS container an encrypted root lives
inside. None of the three has a fallback.

Toybox's applet implements neither half of that. `-U` is not a lookup flag
there — the applet only reports on devices it is handed — and its prober knows
ext, vfat, ntfs, btrfs, f2fs, squashfs and swap but **not `crypto_LUKS`**. With
it in place, an installed machine prints *"Root device with UUID=… not found!"*
and drops to a shell, A/B selection silently never engages, and an encrypted
root never reaches a passphrase prompt. A live boot without a persistence store
is unaffected, because it finds `system.sfs` by mounting each device in turn
and resolves no UUID at all.

Two rules follow, and they are the same two `switch_root` keeps:

- Toybox's `blkid` is switched off in the recipe **and in phase 1**, beside
  `tar` and `file`, so neither build plants `/usr/bin/blkid` and the name
  resolves to util-linux's `/usr/sbin/blkid`. `getopt`, `patch`, `login` and
  `su` are switched off in the recipe alone, because util-linux, the `patch`
  port and shadow each install those names into `/usr/bin` and so reclaim
  whatever phase 1 left there; `blkid` is an `sbin` program and nothing ever
  reclaims it, which is why it has to be off in phase 1 too.
- The initramfs removes `bin/blkid` before copying. With the applet compiled
  out, `./bin/toybox` does not list it and the applet loop never claims the
  name; the removal is the guard on that, because `cp` writes *through* a
  symlink and a `bin/blkid` pointing at `bin/toybox` would take the copy and
  overwrite the multicall binary. The packaging step then refuses an initramfs
  whose `blkid` reports itself as a Toybox multicall binary.

### `file`

Toybox's `file` applet is switched off in the recipe and in phase 1, so
`/usr/bin/file` is the `file` port's — the reference implementation, with
`/usr/share/misc/magic.mgc` behind it.

The applet reads a handful of headers and refuses `--mime` outright.
`lesspipe` asks `file -L -s -b --mime` and nothing else: with no answer there
it hands every file through unchanged, so `less` on a `.tar.gz` shows the
compressed bytes and the filter looks like it was never installed. Two `file`
implementations on one image would also be two answers to "what is this", which
is the question the handler tables, the thumbnailer and the pager all ask.

The cost is stated: `magic.mgc` is about ten megabytes.

### `switch_root`

The initramfs carries util-linux's `/usr/sbin/switch_root`, with its `mount`,
`umount`, `losetup` and `dmesg`, and it must stay that way. The applets are
compiled out, and the packaging step refuses an initramfs whose copy of any of
the five reports itself as a Toybox multicall binary, or whose programs name a
library it does not carry.

Toybox's `switch_root` wipes the initramfs and calls `chroot()`. It never
performs the move-mount that makes the new root the *mount namespace's* root.
The namespace root then stays the emptied initramfs with the real root parked
at `/newroot`, and anything that **joins** a mount namespace — entering a
container, `nsenter -m` — gets that empty root as `/`, so every path fails to
exist. Creating a namespace still works, which is why the failure looks so
strange: starting a container is fine, entering one is not.

The tell-tale is that `readlink /proc/<pid>/root` prints `/newroot`. `kdos
doctor` checks it.

## rcS and the service scripts

`init` runs `/etc/init.d/rcS` as its `sysinit` entry. In order, `rcS`:

1. Appends `/usr/local/sbin:/usr/local/bin` to `PATH`, because toybox init
   hands its children `/sbin:/usr/sbin:/bin:/usr/bin` and `kdos` lives under
   `/usr/local/bin`. Without it a system timer that runs `kdos` is skipped as
   missing.
2. Counts the enabled service scripts and tells the splash its step total.
3. Checks every non-root `fstab` filesystem with a pass number whose device
   exists yet — one on a logical volume the initramfs did not activate is
   left to `03_lvm`
   ([Checking the root](#checking-the-root)) — mounts everything in `fstab`,
   mounts `efivarfs` on a UEFI boot, creates `/run/lock` `1777`, brings the
   loopback interface up, then runs `chmod 1777 /tmp` and `swapon -a`, and
   links `/etc/localtime` to UTC when nothing is there. The `tzdata` package
   does not own `/etc/localtime`, so an upgrade from one such version to the
   next leaves a chosen zone alone. A machine whose installed `tzdata` manifest
   still lists the link loses it once, on the next upgrade — see
   [Known gaps](../06-reference/known-gaps.md#build-and-packaging).
4. Makes the root mount shared, which containers need.
5. Runs each `NN_name.sh` in numeric order, logging each to
   `/run/kdos-init.<name>.log`, showing the splash a step per script and up to
   six lines of failure detail if one fails. A supervised daemon's own output
   does not stay in that file: `ksvc` sends it to syslog and to
   `/run/kdos-svc.<name>.log` — see [The daemons](../04-programs/daemons.md#the-shape-they-share).
6. Runs `kdos-bootctl mark-good`.
7. Quits the splash, which runs the power-off animation and leaves a clean
   framebuffer for the tty1 login.

The quit is synchronous, and the desktop depends on that twice. Init starts the
tty1 login on a framebuffer nothing else owns, and the compositor's modeset
further down that chain acquires a device the splash has already released. A
splash that quit asynchronously would race a modeset, and the loser of that
race is a black screen with a running session behind it.

A service is disabled by a marker file rather than by editing anything:

```sh
sudo touch /etc/service.disabled/cups
```

The convention for the scripts themselves, and the reason `ksvc` exists rather
than a shell supervisor, are in
[Administration](../02-user-guide/administration.md#services) and
[The daemons](../04-programs/daemons.md).

### Shutdown

toybox init answers `reboot`, `poweroff` and `kdos-powerd` by running the
`::shutdown` entries of `/etc/inittab` in order, each to completion, and only
then signalling every process. It never runs a service script's `stop` itself,
so the first entry is `/etc/init.d/rcK`: it takes the scripts `rcS` would run —
executable, no marker under `/etc/service.disabled` — and runs each with `stop`
in reverse order. `swapoff -a` and `umount -a -r` follow, so every stop action
still has a writable filesystem to save to; `50_alsa` storing the mixer levels
is the plainest case. A stop that fails, such as a service that was skipped at
boot answering "not running", does not end the walk. `25_nftables` is the one
script left out: its stop flushes the ruleset, and it would run after the network
scripts while the interfaces are still configured, leaving the machine on the
network with nothing filtering until power-off.

Each supervised service costs `ksvc` a second to stop, so a shutdown takes
about as many seconds as there are daemons running. `kdos-powerd` waits sixty
seconds before calling `reboot(2)` itself, which is longer than `rcK` takes to
reach `55_powerd` and end it; the fallback therefore fires only under an init
that ignored the signal.

### One DHCP client on the link

Two scripts can bring an interface up, and only one of them may. `30_network`
starts `dhcpcd`; `42_networkmanager` starts NetworkManager, whose DHCP client
is internal. NetworkManager never defers to a running `dhcpcd`, so a machine
that started both leases every interface twice — two default routes installed
and withdrawn on each renewal, and two writers of `/etc/resolv.conf`.

`30_network` therefore stands down when `/usr/sbin/NetworkManager` is
executable and `/etc/service.disabled/networkmanager` is absent. The marker is
half of the test on purpose: disabling the service leaves the binary on disk,
and a guard that read the binary alone would leave such a machine with no DHCP
client at all. Turning NetworkManager off hands DHCP back to `dhcpcd`, and that
is what makes `dhcpcd` the fallback for a machine that runs no connection
manager — a server install, or a recovery boot.

Neither script owns the loopback interface. NetworkManager brings `lo` up only
when it runs, and `dhcpcd` never touches it, so `rcS` brings it up before any
service starts. Otherwise a machine on the fallback has no `127.0.0.1` or `::1`,
and CUPS, chrony's command socket and everything else that talks to localhost
fails.

`dhcpcd` is supervised with `-B`, which keeps it in the foreground so `ksvc`
watches the daemon itself rather than a parent that has already exited. Its
lease database is `/var/lib/dhcpcd`, which is also the home directory of the
`dhcpcd` account, uid and gid 999, that its privilege-separated children run
as; the account ships in `/etc/passwd` and its group in `/etc/group`. `25_nftables` runs ahead of both scripts,
because a firewall loaded after an address is configured is a window during
which the machine is on the network with no policy.

## Mount points that must be right

`/etc/fstab` ships:

```
proc      /proc          proc     defaults                  0 0
sysfs     /sys           sysfs    defaults                  0 0
devtmpfs  /dev           devtmpfs defaults                  0 0
tmpfs     /tmp           tmpfs    mode=1777,nosuid,nodev    0 0
tmpfs     /run           tmpfs    mode=0755,nosuid,nodev    0 0
cgroup2   /sys/fs/cgroup cgroup2  nsdelegate                0 0
tracefs   /sys/kernel/tracing tracefs nosuid,nodev,noexec   0 0
debugfs   /sys/kernel/debug   debugfs nosuid,nodev,noexec   0 0
bpf       /sys/fs/bpf    bpf      nosuid,nodev,noexec,mode=0700 0 0
```

The kernel mounts none of the last three. Without tracefs every tracepoint
probe — `bpftrace`, `perf trace`, `trace-cmd` — finds an empty
`/sys/kernel/tracing`, without debugfs the kernel's debug files —
`/sys/kernel/debug/dri/`, `wakeup_sources`, what `powertop` reads — are
absent, and without bpffs no pinned BPF object outlives the process that made
it.

`efivarfs` is mounted by `rcS` and not by `fstab`, because it exists only on a
UEFI boot and a BIOS boot would fail the line. Without it
`/sys/firmware/efi/efivars` is an empty directory: `efibootmgr` reports no EFI
variables, the installer cannot write its NVRAM boot entry or read the Secure
Boot state, and fwupd sees no UEFI devices.

`/run` is a fresh tmpfs, so `/run/lock` — and `/var/lock`, a link to it — exists
only because `rcS` creates it, `1777`. minicom and picocom take their UUCP port
locks there as the user: without the directory they lock nothing and two
programs can share one serial port, and with a `0755` one picocom refuses to
start.

`/tmp` must carry `mode=1777`, and the `chmod` in `rcS` is not redundant.
Mounting it with default options gives a `0755` root-owned filesystem that
*hides* the `1777` `/tmp` baked into the image, so no ordinary user can write
to `/tmp` at all. Every graphical application depends on it — lock files,
scratch space, font caches — and the failure presents as "the application is
slow or never opens". A tmpfs that is already mounted ignores a mode change on
remount, so only the explicit `chmod` fixes an already-mounted one.

`/var/run` and `/var/lock` are symlinks into `/run`. Several libraries still
compile in the pre-2011 path `/var/run/dbus/system_bus_socket`. With `/var/run`
as a real empty directory, every one of those clients fails to reach the system
bus — and reports it as the *service* being unreachable, while that service is
running two processes away.

The installer appends to `fstab` rather than replacing it, precisely because of
the shipped `/tmp` line.

## The console

The framebuffer console is built with deferred takeover, and the takeover
re-initialises every terminal with the kernel's built-in font. A `setfont` in
`rcS` is therefore silently wiped.

`kdos-getty` wraps both gettys in `/etc/inittab` and does the job in the right
place: force the takeover, load the font and palette, verify, then execute the
getty.

- Only a real glyph ends the deferral. Escape sequences are consumed by the
  terminal's state machine, and even spaces are skipped by the render path. The
  wrapper prints one character and clears it.
- The takeover is scheduled work, so the wrapper waits for the kernel to report
  it and retries the font load until the VT confirms the size.
- The font is the KDOS VT font, built in the `terminus-font` port: a 512-glyph
  set with six spacing characters replaced by the double box-drawing glyphs the
  block logo needs.
- The palette is loaded **before** the final clear, or the screen ends up half
  pure black and half phosphor black.
- It traces to `/run/kdos-getty.<tty>.log`.

Font and palette setup must not move back into `rcS`.

`/etc/inittab` gives `tty1` to `kdos-login`, `tty2` an ordinary getty, and
`ttyS0` an `askfirst` root login shell — a serial console that costs nothing
until somebody presses a key on it.

`kdos-login` reads `autologin` from
[`login.conf`](../06-reference/configuration.md#etckdosloginconf) and hands the
tty to `agetty` either way:

- **With the key**, it executes `agetty --autologin <account>`, which is the
  live medium's answer: a machine with one account and no password has nothing
  to ask. `/bin/login` must be shadow's for this to work — agetty's autologin
  calls `login -f -- USER`, and toybox's `login` reads the name as `-f`'s own
  argument, takes `--` for the account and refuses it. Toybox is therefore built
  with `login` and `su` off, or tty1 is left at a login prompt nobody can answer.
- **Without it** — commented out, which is what the installer writes unless an
  answer file asked otherwise — it executes plain `agetty` and the ordinary
  password prompt appears. There is no greeter: no account chooser, no session
  chooser and no surface of any kind before the shell.

Going through agetty either way keeps utmp, lastlog and the shell profile on
the path they take everywhere else — and the profile is what starts the
desktop, so a second way in here would be a second place that has to remember
to.

`kdos-getty` falls back to the plain autologin getty when the program named in
`/etc/inittab` cannot be executed. Without that fallback, init would respawn a
failing exec forever and there would be no way to log in at all. It reads the
same `login.conf` key rather than a hardcoded account: the desktop's account is
named in one place, and a second copy here would log in a user a renamed
installation does not have.

`tty2` is the recovery console and stays a plain getty whatever tty1 does.
Reaching it from the desktop is the compositor's job, not the kernel's: once
`libseat` puts tty1 into graphics mode the kernel stops answering
Ctrl+Alt+F<n>, so a session that did not forward the chord would guarantee a
recovery console nothing can reach.

## The login banner

`kdos-banner` paints the banner one raster line at a time with a bright beam
leading the fill, then one frame of reverse video for a CRT thump. It falls
back to a plain print when the output is not a terminal, when `TERM` is dumb,
when `KDOS_NO_ANIM` is set, or when the banner is taller than the terminal. Any
keypress skips the rest.

It composes the banner itself and runs the system-information tool with that
tool's own logo disabled. That is not a style choice. Asked to draw a logo,
that tool prints the block, moves the cursor back up over it and writes each
line with an absolute column jump — output that is not a sequence of raster
lines, so replaying it a line at a time drifts one row per line and draws the
block twice.

The logo is generated from the same image the boot splash draws, so the banner,
the splash and the mascot cannot drift apart.

![The login banner at the 512-glyph VT font, on the first terminal](../../screenshots/tty-banner.png)

Three constraints are baked into that generator. The VT font has full blocks
and the double box characters but **no half blocks**, so one cell is one solid
block. Character cells are twice as tall as wide, so the sampling grid must be
about twice as wide as tall or the image stretches. And the banner must stay
under about thirty lines or it scrolls off the screen.

## See also

- [Getting started](../02-user-guide/getting-started.md) — the same path from the user's side
- [Installation](../02-user-guide/installation.md) — what the installer writes to the ESP
- [The session](session.md) — everything after the login prompt
- [The daemons](../04-programs/daemons.md) — the services `rcS` starts
- [Configuration](../06-reference/configuration.md) — `fstab`, `inittab` and the rest
