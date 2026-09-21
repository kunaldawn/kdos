# Boot and init

The path from firmware to a login prompt, and every program that runs along it. This page is
what you read when a boot stops somewhere and you need to know which component owns that step.
For the user-facing view of the same path, see
[Getting started](../02-user-guide/getting-started.md).

## The path, in order

| # | Stage | Who runs it |
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
| 12 | The numbered service scripts | `rcS` |
| 13 | `kdos-bootctl mark-good` | `rcS`, last |
| 14 | `kdos-getty` on tty1 and tty2 | init, as `respawn` |
| 15 | `kdos-con-login`, which greets or autologins | `kdos-getty` on tty1 |
| 16 | The console desktop | `~/.bash_profile`, on tty1 only |

Step 16 is [the session](session.md). **The console desktop is the default one** — a login on tty1
reaches it without anyone typing a command, and it needs no Wayland, so it comes up on a machine
whose GPU driver does not. The graphical session is still started by hand, with `kdos-desktop`.

## Limine and the kernel command line

The boot loader is [Limine](https://limine-bootloader.org/), and it is the only one: the same
binary, the same configuration file and the same menu serve BIOS and UEFI, so what a machine
shows at power-on does not depend on how it started. On UEFI the firmware loads
`EFI/BOOT/BOOTX64.EFI`; on BIOS it runs the boot code in the first sector, which finds
`limine-bios.sys` by name in the root, `/boot`, `/limine` or `/boot/limine` of a volume it can
read.

**`BOOTIA32.EFI` sits beside it, and the two never compete.** A 64-bit CPU does not imply a 64-bit
firmware — the early Atom tablets and a few netbooks run this kernel and this userland and can load
only a 32-bit EFI binary — and firmware reads the one `EFI/BOOT/BOOT<arch>.EFI` it can execute and
never looks at the other. Both are built from one source by `ports/core/limine`, both are on the
ISO's ESP tree and on an installed machine's, and the El Torito UEFI record gathers whichever were
built with no second switch. It costs about a hundred kilobytes and one more freestanding build.

**An NVRAM entry names one path, so the installer has to choose.** The removable-media fallback
picks between the two by itself; `efibootmgr --create` cannot, and an entry pointing at a binary the
firmware cannot load is a boot option that fails rather than one that falls through. `kinstall`
reads `/sys/firmware/efi/fw_platform_size` and writes the matching path. A kernel too old to publish
it leaves the installer assuming 64 — nearly every machine, and where that is wrong the 32-bit
firmware still boots through the fallback rather than not at all.

The kernel and initramfs are placed where the loader is certain to read them — on the ISO9660
medium for the live image, and **on the ESP** for an installed system. Limine reads FAT and
ISO9660; it does not read the roots the installer offers, which include xfs, f2fs and anything
under LUKS. A kernel on the root filesystem would be a boot that depends on a driver the
bootloader does not have.

**Two programs write that configuration and nothing else does.**
`script/06_packaging/02_iso.sh` writes the live medium's `boot/limine/limine.conf`;
`kinstall` writes `limine.conf` at the root of the installed machine's ESP. That location is not
arbitrary: Limine looks beside its own EFI binary first and then at `/boot/limine/`, `/boot/`,
`/limine/` and `/` on each volume — and **only that last set is searched on BIOS**, so the root of
the ESP is the one path both firmwares find. A second copy beside `BOOTX64.EFI` would be the copy
that goes stale.

**The menu counts down for ten seconds**, on the live medium and on an installed system alike, and
a single keypress stops it and leaves it up. That countdown is wall time spent before the kernel
exists with nothing else running, so it is bought rather than free — but the recovery entries (the
verbose boot, the clean session, single user, memtest86+) are reachable from nowhere else, and a
countdown short enough to miss makes them unreachable on exactly the machine that needs them.
**`timeout: 0` does not draw a menu at all**; it boots the default entry immediately, which takes
those entries with it.

The menu is drawn as a character grid in **the accent in force**, in the console's own Terminus
face, over a dimmed full-bleed backdrop. See [Theming the boot menu](../02-user-guide/theming.md)
for what is configurable and [the design language](design-language.md) for where the colours come
from.

**`kcol_limine_conf()` emits the whole look and both writers call it.** The ISO step reaches it
through `kdos-bootctl theme --print`; `kinstall` links it. Two hand-copied sets of nine literals is
how a stick and the machine installed from it end up different colours, and the one that is wrong
is the one nobody is booting that day. The only lines the writers own are the two that name paths
on the medium — `wallpaper` and `term_font`.

**The menu's plate is OPAQUE and its backdrop is stretched, and both are legibility rather than
taste.** `term_background` carries a leading transparency byte. Transparent, there is no plate and
the artwork may be any size — but **Limine prints `linux: Loading kernel …` at the terminal's own
origin the moment an entry is picked**, and with no plate to draw it on those lines land across
the artwork. Photographed. Opaque, the loading text can only ever appear inside the plate. A
`centered` wallpaper is drawn at its own size in the middle of the screen, which is exactly where
the menu is, so the style is `stretched`. `term_font_scale` is `1x2`: doubling both axes of an
8x16 face fills a 1080-row screen with four entries.

**There is no placement that avoids the loading text at every resolution**, which is why the plate
is opaque rather than the artwork being moved. The text's position is in PIXELS from the margin;
the artwork's is a FRACTION of a stretched wallpaper. A banner that clears the text at 1080 lines
runs underneath it at 720.

**A margin that leaves the terminal under SIXTEEN ROWS makes Limine abandon the graphical terminal
altogether**, and that is the ceiling `term_margin` is set against. Measured by booting one ESP
per value: at 1280x800 with these keys, 144 keeps sixteen rows and themes correctly, and 152
leaves fifteen and comes up in Limine's own font, its own palette, its own branding and **no
wallpaper at all**. It is not a degraded theme, it is no theme. A row is `term_font_size` times
`term_font_scale` — 16 by 2 — so the ceiling is `(height − 512) / 2`: 144 at 800 lines, 104 at
720, 44 at 600. The shipped value is 100, which clears 720 and everything above it.

**`interface_branding` is emitted EMPTY, and empty is not the same as absent.** The artwork is the
branding, so the key would print "KDOS" in a font directly under a picture of it — but leaving the
key out does not remove it. Limine falls back to its own default and prints `Limine <version>
(x86-64, UEFI)` in its own cyan. Photographed.

**The artwork is the penguin and the wordmark, sized to fit the margin.** `genbackdrop.py` lays
them on a floor of the scheme's `deep`. Four things about it are measured rather than chosen, and
each fixes something that looked broken:

- **6.3% of the height, because the margin is 100 pixels.** The wallpaper is `stretched`, so the
  artwork scales with the screen while the margin does not: it has to clear 100 pixels on the
  tallest screen it will be seen on. 6.3% is 68 pixels at 1080 lines and 90 at 1440 — the largest
  resolution the tree references — and is covered by the plate above about 1580.
- **The captions are dropped.** At this height the two lines of small print are three pixels tall;
  including them costs the wordmark a third of its own height to render something illegible.
- **The ink is the MAX CHANNEL, not the luminance.** The wordmark is phosphor green — `(57,255,20)`
  — which Rec.601 puts at 169 while the penguin's white lands at 249, so a plain greyscale
  conversion leaves the letters visibly duller than the mascot beside them.
- **The black point is 64, and that is what stops the rectangle.** The banner carries an ambient
  green glow over its whole area — its corner pixel is `(18,64,31)`, not black — so composited
  onto the floor it lifts its own area above the field and draws a box around itself. 64 is the
  95th percentile of the border ring: subtracting it takes the border to exactly zero at a cost of
  2% of the strong ink.

The downscale is `BOX` — area averaging — because the wordmark is pixel art and a photographic
filter on hard edges gives blocks of different widths with soft jagged edges. There is no vignette
and no texture: both existed to make the border look deliberate, and what they contributed was
banding in the corners, because a wordmark folded onto itself reads as grain at full size and as
smudges at a sixth of it. A flat floor cannot band. It is a host script whose output is committed,
the arrangement `genbanner.py` and `genlogo.py` already use.

**`kdos-bootctl theme <accent>` restamps an installed machine in place.** It rewrites only the keys
the theme owns and leaves every entry, `cmdline`, `default_entry` and the A/B state alone — a
restamp that rewrote the file from a template would discard a slot somebody is mid-rollback on.
`wallpaper_style` and `term_font_scale` are among the keys it owns: they are layout, so a machine
installed before a retheme picks up the new arrangement rather than keeping `2x2` over `centered`.
The write is temp/fsync/rename/fsync-the-directory, because the ESP is FAT and a zero-length
`limine.conf` is a machine that shows no menu.

`kdos theme` reaches it through `kdos-powerd accent`, which also writes `/etc/kdos/accent` for the
splash. A machine with no writable ESP skips the boot half and says so; the desktop still retints.

Parameters KDOS itself reads:

| Parameter | Read by | Meaning |
|---|---|---|
| `root=UUID=…` | The initramfs | The filesystem to mount as root |
| `cryptdevice=UUID=…:<name>` | The initramfs | A LUKS container to open first |
| `bootstate=UUID=…` | `kdos-bootctl` | Which ESP holds the A/B state file |
| `console=` | The kernel | **The last one wins**, and it is a serial port here |

That last row explains two things that otherwise look like bugs. Because the final `console=` is
a serial port, kernel and initramfs messages go to the serial line and the display shows nothing —
which is exactly what makes the graphical splash possible. And it is why the encrypted-root
prompt must be drawn through the splash and read from `/dev/tty1` explicitly, rather than using
`/dev/console`, which nobody is looking at.

## Microcode

`CONFIG_MICROCODE=y` with late loading **off**, so the kernel's early loader is the only path
there is. It runs before any filesystem exists and scans the raw initrd for two literal paths:

```
kernel/x86/microcode/GenuineIntel.bin
kernel/x86/microcode/AuthenticAMD.bin
```

So the image the boot loader hands over is two archives concatenated: a plain, **uncompressed**
cpio containing those two files, followed by the gzipped initramfs.

Three properties that must hold:

- **Nothing in the microcode archive may be compressed** — not the archive and not the blobs
  inside it. The firmware package ships AMD's microcode compressed because the *runtime* loader
  can decompress; the early loader cannot, so the build expands them on the way in.
- **The Intel bundle is not curated.** It is upstream's whole set concatenated, minus the images
  upstream ships separately because they need coordinated firmware support. A per-family prune
  boots on the machines it covers and leaves the rest silently unpatched.
- **The Intel bundle must be consumed to exactly its last byte.** The kernel's scanner returns
  nothing at all if any trailing data remains, so one stray file in the directory means **no**
  microcode is loaded rather than "everything before the bad record". The build walks the records
  the same way and fails rather than shipping such an image.

An initramfs rebuilt without this step has **no symptom** — the processor keeps whatever the
firmware loaded — so `kdos doctor` runs the kernel's own search against the image and reports the
running revision.

## The initramfs

Generated by the build. It carries a shell, the module set, and the handful of programs the early
boot needs.

**The module list is load-bearing**, because a root filesystem whose driver is missing installs
perfectly and never boots again:

```
overlay squashfs isofs cdrom sr_mod loop sd_mod ata_piix ahci libahci
virtio virtio_blk virtio_pci virtio_scsi
xhci-pci xhci-hcd ehci-pci ehci-hcd ohci-pci ohci-hcd usb-storage uas
vfat nls_cp437 nls_iso8859-1
xfs f2fs
```

ext4 and btrfs are built into the kernel; xfs and f2fs are modules and appear here for exactly
that reason. **Every filesystem the installer offers must be in this list.**

`cryptsetup` and its libraries are carried only when they are installed, and the build says so
when they are not. A half-carried `cryptsetup` fails at the passphrase prompt rather than at build
time, which is the wrong place to find out.

**Both paths that look for a device poll, and both give up after ten seconds.** The disk path
asks `blkid` for the root UUID once a second; the live path rescans `/dev/sr* /dev/sd* /dev/vd*
/dev/nvme*` every 100 ms and mounts the first one holding `system.sfs`. Neither waits before its
first attempt — udev has already settled by then, so on a machine whose medium is enumerated the
first pass succeeds and costs nothing. The bound is what covers the slow cases, a USB stick or a
device behind a bridge, and **every pass walks every device class again**: the first node to
answer is not always the one holding the medium, and a class that has not appeared yet must still
get its chance. **A device that mounts is inspected once**, though: a filesystem without
`system.sfs` on it will not grow one, so it is remembered and skipped, and only nodes that have
not mounted yet are tried again — one mount/umount pair for a wrong disk across the whole scan
rather than one per pass. `/mnt/iso` is the only mount point the scan has, so **the umount is
checked**: a mount point left busy would take a second filesystem stacked on top of it and every
later test would read the wrong one, so a failed umount stops the scan and says so. Only the first
pass narrates each device it tried; a hundred repetitions of the same two lines would bury the
message that explains a failed boot.

## The splash

`kdos-splash` draws a CRT power-on directly to `/dev/fb0`, with glyphs scaled up from the shipped
console font. It is a static binary of about seven hundred lines.

It works because of the same `console=` fact above: nothing prints to `tty0`, and the kernel is
built with deferred framebuffer-console takeover, so the framebuffer is free for a process to draw
on.

Three facts constrain it:

- **Deferred takeover means nothing is being scanned out yet.** Writing to `/dev/fb0` before the
  display driver's own client does its mode set paints a buffer nobody is looking at. One byte
  written to `/dev/tty0` ends the deferral; the splash writes a clear-and-hide-cursor sequence to
  do that and hide the cursor in the same action.
- **Memory-mapped writes need an explicit flush.** They reach the display only when the deferred
  I/O worker gets round to it, so each frame ends with a pan-display call to the offset it is
  already at.
- **The process survives `switch_root` with a ghost root.** It is never `chroot`ed, so afterwards
  its `/` is the deleted initramfs root. Open file descriptors keep working — that is the whole
  trick, one process spanning both halves of boot — but every path it resolves *by name* after
  that points into the ghost. The `quit` client therefore detects the daemon by opening the FIFO
  and watching for a specific error, and does the cleanup itself from the real root.

**Adding a stage is one line on either side**: a step-and-ok pair in the generated init, or in
`rcS`, which already wraps every service script in one.

The progress total is **additive** — each phase adds its own step count as soon as it knows it —
so `done == total` happens at every phase boundary. The bar therefore **clamps one segment short
of full** until `quit` arrives. A boot that shows 100% before it has finished is a bug report
waiting to happen.

Iterate on it without booting:

```sh
kdos-splash preview 1280x800 0.35 out.ppm
```

## Unlocking an encrypted root

`cryptdevice=UUID=<container>:<name>` on the command line, using Arch's syntax because it is the
one already in people's heads. The generated init unlocks **before** it looks for a filesystem,
because the filesystem named by `root=` does not exist until then.

Three things the prompt gets right, each of which is a way this usually goes wrong:

- **It draws through the splash**, not on `/dev/console`, which is a serial port.
- **It reads keystrokes from `/dev/tty1`**, which is where the keyboard is.
- **It feeds the passphrase to `cryptsetup` on stdin**, never as an argument:
  `/proc/<pid>/cmdline` is world-readable for the life of the process.

Three attempts, then a shell rather than a reboot loop. There is no per-keystroke feedback,
because the splash owns the framebuffer and the shell owns the terminal — stated rather than
hidden.

## A/B slot selection

Two root partitions, a state file on the ESP, and a boot that can change its mind:

```
slot_a   = <filesystem uuid>
slot_b   = <filesystem uuid>
crypt_a  = <luks uuid>      the container that filesystem is inside, or empty
crypt_b  = <luks uuid>
active   = a                the slot known to work
try      = b                a candidate, or empty
attempts = 3                how many boots it gets
```

**The counting lives in the initramfs**, and that placement is the design. `rcS` is the wrong
place: a kernel that boots into a wedged userland must still spend an attempt, and the `rcS` in
that userland never runs to say so. `kdos-bootctl select` decides and decrements in one step,
before anything is mounted, and prints the UUID to boot.

`kdos-bootctl mark-good` is the other half and runs at the **end** of `rcS`, after every service
that was going to fail has had its chance. So a bad update boots its allotted number of times and
rolls itself back with no help from anything.

**The state file is on the ESP, which is FAT and has no journal.** A torn write there does not
fail an update, it bricks the machine — the initramfs cannot tell which slot to boot. So every
write is: temporary file, `fsync` the **file**, `fsync` the **directory**, then rename. The
directory `fsync` is the step people leave out, and without it the rename can be lost while the
data survives.

**A state file that does not parse is absent, never partial.** Absent means "use the `root=` the
command line already carries", which is what a single-root machine does anyway. A `try` pointing
at a slot with no root, or at the active slot, is refused rather than recorded. An unknown key is
skipped, which is what makes a state file written before `crypt_a` existed read as a machine with
no containers — because that is what it is.

### A slot knows its own container

**`slot_a` is a FILESYSTEM and `crypt_a` is what it is inside**, and keeping the two apart is what
joins A/B to encryption. On an encrypted machine the root filesystem lives in a LUKS container,
and the kernel command line can name exactly one `cryptdevice=`. Two slots inside two containers
cannot both be named there — so the second one is recorded per slot, here, and the initramfs asks
for it **after** `select` has chosen:

```sh
SEL=$(kdos-bootctl select)          # the filesystem, and one attempt spent
SLOT_CRYPT=$(kdos-bootctl crypt "$SEL")   # its container, if it has one
```

`crypt` is keyed by the **filesystem UUID** rather than by a slot name, so it is one call with
nothing carried between the two: `select` may have rolled back, and asking "which slot did that
turn out to be" would be a second decision that could disagree with the first. Both reads happen
while the ESP is still mounted — the second one reads the same file the first just wrote.

**A slot that names no container leaves `cryptdevice=` exactly as the command line set it**, which
is every unencrypted machine and every machine whose two slots share one container. The mapper
name is the initramfs's own and never the state file's: only one container is open at a time
there, so a per-slot name would disambiguate nothing, and a name read out of a file on the ESP is
a name somebody can edit into a path.

Without this, selecting slot B unlocks slot A's container and then looks for B's filesystem inside
it. There is nothing there, and the failure reads as a corrupt filesystem rather than as a lookup
that was never made.

## `blkid` must be util-linux's, not toybox's

Every lookup the initramfs makes is `blkid -U <uuid>`: the **root filesystem**, the **ESP** that
holds the A/B state, and the **LUKS container** an encrypted root lives inside. None of the three
has a fallback.

**toybox's applet implements neither half of that.** `-U` is not a lookup flag there — the applet
only reports on devices it is handed — and its prober knows ext, vfat, ntfs, btrfs, f2fs,
squashfs and swap but **not `crypto_LUKS`**. With it, an installed machine prints *"Root device
with UUID=… not found!"* and drops to a shell, A/B selection silently never engages, and an
encrypted root never reaches a passphrase prompt. A live ISO is unaffected, because it finds
`rootfs.squashfs` by scanning and never calls blkid at all.

**`/usr/bin/blkid` is toybox's name on the finished image** and `$PATH` puts `/usr/bin` ahead of
`/usr/sbin`, so the applet shadows the real tool for every caller — the initramfs, `kdos persist`
finding its store by label, and anybody typing the name. Two rules follow, and they are the same
two `switch_root` keeps:

- **toybox's `blkid` is switched off in the recipe**, beside `tar`, `getopt`, `patch`, `file`,
  `login` and `su`, so the name resolves to util-linux's everywhere.
- **The initramfs removes `bin/blkid` before copying.** The applet loop has already made it a
  symlink to `bin/toybox`, and `cp` writes *through* a symlink — overwriting `bin/toybox` while
  leaving `bin/blkid` pointing at the applet. The packaging step then refuses an initramfs whose
  `blkid` reports itself as a Toybox multicall binary.

## `file` must be the magic database's, not toybox's

Toybox's `file` applet is switched off in the recipe and in phase 1, so `/usr/bin/file` is the
`file` port's — the reference implementation, with `/usr/share/misc/magic.mgc` behind it.

The applet reads a handful of headers and refuses `--mime` outright. `lesspipe` asks
`file -L -s -b --mime` and **nothing else**: with no answer there it hands every file through
unchanged, so `less` on a `.tar.gz` shows the compressed bytes and the filter looks like it was
never installed. Two `file`s on one image would also be two answers to "what is this", which is the
question the handler tables, the thumbnailer and the pager all ask.

The cost is stated: `magic.mgc` is about ten megabytes.

## switch_root must be util-linux's

The initramfs installs `/usr/sbin/switch_root` over toybox's applet, and it must stay that way.

Toybox's `switch_root` wipes the initramfs and calls `chroot()`. It never performs the move-mount
that makes the new root the *mount namespace's* root. The namespace root then stays the emptied
initramfs with the real root parked at `/newroot`, and anything that **joins** a mount namespace —
entering a container, `nsenter -m` — gets that empty root as `/`, so every path fails to exist.
Creating a namespace still works, which is why the failure looks so strange: starting a container
is fine, entering one is not.

The tell-tale is that `readlink /proc/<pid>/root` prints `/newroot`. `kdos doctor` checks it.

## rcS and the service scripts

`init` runs `/etc/init.d/rcS` as its `sysinit` entry. `rcS`:

1. Counts the enabled service scripts and tells the splash its step total.
2. Mounts everything in `fstab`, then **`chmod 1777 /tmp`**, then `swapon -a`.
3. Makes the root mount shared, which containers need.
4. Runs each `NN_name.sh` in numeric order, logging each to `/run/kdos-init.<name>.log`, showing
   the splash a step per script and its failure detail if one fails.
5. Runs `kdos-bootctl mark-good`.
6. Quits the splash, which runs the power-off animation and leaves a clean framebuffer for the
   tty1 login.

   **The quit is synchronous**, and the console desktop depends on it twice: init starts the tty1
   login on a framebuffer nothing else owns, and `kdos-view`'s KMS modeset further down that chain
   acquires a device the splash has already released. A splash that quit asynchronously would race
   a modeset, and the loser of that race is a black screen with a running session behind it.

**A service is disabled by a marker file**, not by editing anything:

```sh
sudo touch /etc/service.disabled/cups
```

The convention for the scripts themselves, and the reason `ksvc` exists rather than a shell
supervisor, is in [Administration](../02-user-guide/administration.md#services) and
[The daemons](../04-programs/daemons.md).

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

**`/tmp` must be `mode=1777`, and the `chmod` in `rcS` is not redundant.** Mounting it with
default options gives a `0755` root-owned filesystem that *hides* the `1777` `/tmp` baked into
the image, so no ordinary user can write to `/tmp` at all. Every graphical application depends on
it — lock files, scratch space, font caches — and the failure presents as "the application is slow
or never opens". A tmpfs that is already mounted ignores a mode change on remount, so only the
explicit `chmod` fixes an already-mounted one.

**`/var/run` and `/var/lock` are symlinks into `/run`.** Several libraries still compile in the
pre-2011 path `/var/run/dbus/system_bus_socket`. With `/var/run` as a real empty directory, every
one of those clients fails to reach the system bus — and reports it as the *service* being
unreachable, while that service is running two processes away.

The installer **appends** to `fstab` rather than replacing it, because the shipped `/tmp` line is
exactly the thing above.

## The console

The framebuffer console is built with deferred takeover, and the takeover re-initialises every
terminal with the kernel's built-in font. A `setfont` in `rcS` is therefore silently wiped.

`kdos-getty` wraps both gettys in `/etc/inittab` and does the job in the right place: force the
takeover, load the font and palette, verify, then execute the getty.

- **Only a real glyph ends the deferral.** Escape sequences are consumed by the terminal's state
  machine, and even spaces are skipped by the render path. The wrapper prints one character and
  clears it.
- The takeover is scheduled work, so the wrapper waits for the kernel to report it and retries the
  font load until the console confirms the size.
- **The font is the KDOS console font**, built in the `terminus-font` port: a 512-glyph set with
  six spacing characters replaced by the double box-drawing glyphs the block logo needs.
- The palette is loaded **before** the final clear, or the screen ends up half pure black and half
  phosphor black.
- It traces to `/run/kdos-getty.<tty>.log`.

Do not move font or palette setup back into `rcS`.

`/etc/inittab` gives `tty1` to `kdos-con-login`, `tty2` an ordinary login, and `ttyS0` a serial
login on demand.

`kdos-con-login` is `kdos-con` under a third name, and it reads `greet` from
[`con.conf`](../06-reference/configuration.md):

- **`greet = no`** — the live medium's answer — executes `agetty --autologin kdos`. Going through
  agetty keeps utmp, lastlog and the shell profile on the path they take everywhere else, and a
  machine with one account and no password has nothing to ask. **`/bin/login` must be shadow's**:
  agetty's autologin calls `login -f -- USER`, and toybox's `login` reads the name as `-f`'s own
  argument, takes `--` for the account and refuses it — so toybox is built with `login` and `su`
  off and tty1 is left at a login prompt nobody can answer if they come back.
- **`greet = yes`** — what the installer writes — draws the login surface on the tty. It uses the
  **tty backend**, not a modeset: `kdos-getty` has already loaded the console font and palette, and
  a greeter that opened a DRM device would make the session binary depend on the one thing the
  session/view split exists to survive. The modeset is `kdos-view`'s, after the login.

The greeter never handles a password hash. On submit it forks, the child drops to the candidate
account, and it executes `kdos-checkpass` with the password on stdin — the same setuid helper the
lock screen uses, which takes no arguments and checks the caller's own real uid, so nothing on this
path can be aimed at root.

**`kdos-getty` falls back to the plain autologin getty** when the program named in `/etc/inittab`
cannot be executed. An image built without the console desktop still gives a console; without the
fallback, init would respawn a failing exec forever and there would be no way to log in at all. It
logs in the account `/etc/kdos/con.conf` names rather than a hardcoded one: the desktop's account
is named in one place, and a second copy here would log in a user a renamed installation does not
have.

`tty2` is the recovery console and stays a plain getty whatever tty1 does. **Reaching it from the
console desktop is `libkkms`'s job**, not the kernel's: once `libseat` puts tty1 into graphics mode
the kernel stops answering Ctrl+Alt+F<n>. xkb resolves that chord to an `XF86Switch_VT_<n>` keysym,
so `kkms_input.c` — which holds the seat and is the only place the keysym exists — calls
`libseat_switch_session`. A desktop that forwarded it instead would guarantee a recovery console
nothing can reach.

## The login banner

`kdos-banner` paints the banner one raster line at a time with a bright beam leading the fill,
then one frame of reverse video for a CRT thump. It falls back to a plain print when the output is
not a terminal, when `TERM` is dumb, when `KDOS_NO_ANIM` is set, or when the banner is taller than
the terminal; any keypress skips the rest.

It composes the banner itself and runs the system-information tool with its logo disabled. That
is not a style choice: asked to draw a logo, that tool prints the block, moves the cursor back up
over it and writes each line with an absolute column jump — output that is not a sequence of
raster lines, so replaying it a line at a time drifts one row per line and draws the block twice.

The logo is **generated** from the same image the boot splash draws, so the banner, the splash and
the mascot cannot drift apart.

![The login banner at the 512-glyph console font, on the first terminal](../../screenshots/tty-banner.png)
 Three constraints are baked into that generator: the console font
has full blocks and the double box characters but **no half blocks**, so one cell is one solid
block; character cells are twice as tall as wide, so the sampling grid must be about twice as wide
as tall or the image stretches; and the banner must stay under about thirty lines or it scrolls
off the screen.

## See also

- [Getting started](../02-user-guide/getting-started.md) — the same path from the user's side
- [Installation](../02-user-guide/installation.md) — what the installer writes to the ESP
- [The session](session.md) — everything after the login prompt
- [The daemons](../04-programs/daemons.md) — the services `rcS` starts
- [Configuration](../06-reference/configuration.md) — `fstab`, `inittab` and the rest
