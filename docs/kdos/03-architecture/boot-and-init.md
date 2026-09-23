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
| 8 | Encrypted root unlock, if any | `cryptsetup` |
| 9 | Find and mount the root | The initramfs `init` |
| 10 | `switch_root` | util-linux's, never toybox's |
| 11 | `/etc/init.d/rcS` | init (toybox), as `sysinit` |
| 12 | The 30 numbered service scripts | `rcS` |
| 13 | `kdos-bootctl mark-good` | `rcS`, last |
| 14 | `kdos-getty` on tty1 and tty2 | init, as `respawn` |
| 15 | `kdos-login`, which hands the tty to agetty | `kdos-getty` on tty1 |
| 16 | The desktop | `~/.bash_profile`, on tty1 only |

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

Two programs write the loader configuration and nothing else does.
`script/06_packaging/02_iso.sh` writes the live medium's
`boot/limine/limine.conf`; `kinstall` writes `limine.conf` at the root of the
installed machine's ESP. That location is not arbitrary. Limine looks beside
its own EFI binary first and then at `/boot/limine/`, `/boot/`, `/limine/` and
`/` on each volume — and only that last set is searched on BIOS, so the root of
the ESP is the one path both firmwares find. A second copy beside `BOOTX64.EFI`
would be the copy that goes stale.

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

`cryptsetup` and its libraries are carried only when they are installed, and
the build says so when they are not. A half-carried `cryptsetup` fails at the
passphrase prompt rather than at build time, which is the wrong place to find
out.

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
SEL=$(kdos-bootctl select)                 # the filesystem, and one attempt spent
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

## Three tools that must not be toybox's

Toybox provides applets under names that also belong to full implementations,
and `$PATH` puts `/usr/bin` ahead of `/usr/sbin`. Where the applet is not a
drop-in, the recipe and phase 1 both switch it off, so the name resolves to the
real tool everywhere. Both are needed: phase 1 installs toybox outside the
package database, so a name it plants is owned by no package — the later port
install replaces `/usr/bin/toybox` without removing the symlink, and the orphan
sweep works from the database and never sees it.

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

The initramfs installs `/usr/sbin/switch_root` over toybox's applet, and it
must stay that way.

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
3. Mounts everything in `fstab`, then runs `chmod 1777 /tmp`, then `swapon -a`.
4. Makes the root mount shared, which containers need.
5. Runs each `NN_name.sh` in numeric order, logging each to
   `/run/kdos-init.<name>.log`, showing the splash a step per script and up to
   six lines of failure detail if one fails.
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
boot answering "not running", does not end the walk.

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
```

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
