# Boot and init

This page follows a KDOS machine from power-on to the login prompt, and names
the program that owns each step. It is for anyone whose machine stops somewhere
along that path and who needs to know which program to look at, and for
contributors changing the boot loader, the initramfs, the splash, the A/B root
slots, `rcS` or the login.

If you only want to know what you see during a boot and which menu entry to
pick, read [Getting started](../02-user-guide/getting-started.md) first; it
describes the same path from the user's side. Read [The sequence](#the-sequence)
and [When a boot stops](#when-a-boot-stops) here, then go to the section for
the step that failed. Some sections are reference for contributors changing
the build, and nothing in them changes how a running machine behaves; someone
troubleshooting a boot can skip them. They are
[Why the plate is opaque](#why-the-plate-is-opaque),
[The margin ceiling](#the-margin-ceiling), [The artwork](#the-artwork), the
list of checks that stop an initramfs build in
[The initramfs](#the-initramfs), and
[Tools that must not be toybox's](#tools-that-must-not-be-toyboxs). After this page you can say which stage a boot reached,
where that stage writes its log, what each kernel command-line word does, and
how the A/B root slots decide which root to boot and when to roll back.

A few terms used throughout:

| Term | Meaning |
|---|---|
| ESP | The EFI system partition: a small FAT partition the firmware reads. On an installed KDOS machine it holds the boot loader, the kernels, the boot menu and the A/B boot state. |
| initramfs | The small archive the kernel unpacks into memory and runs first. Its `init` script finds, unlocks and mounts the real root, then hands over to it. |
| Root slot | One of two root partitions, `a` and `b`, on a machine set up for A/B updates. One is the *active* slot, known to work; the other can hold an update on *trial*. Not the same thing as a colour [slot](../06-reference/glossary.md). |
| Live medium | The KDOS ISO, booted from a USB stick or optical disc, running from a compressed image with an in-memory (or persistent) overlay on top. |

## The sequence

| # | Stage | Run by |
|---|---|---|
| 1 | Firmware, BIOS or UEFI | The machine |
| 2 | Limine, from the ESP on UEFI or from the MBR on BIOS | The firmware |
| 3 | The kernel, with its command line | Limine |
| 4 | Early microcode load | The kernel, before any filesystem exists |
| 5 | The initramfs `init`: mounts `/proc`, `/sys`, `/dev`, starts the splash | The kernel |
| 6 | Device manager: `udevd`, trigger, settle | The initramfs `init` |
| 7 | Software RAID assembly (`mdadm --assemble --scan`), when `mdadm` is carried | The initramfs `init` |
| 8 | Filesystem modules: `loop`, `isofs`, `squashfs`, `overlay` | The initramfs `init` |
| 9 | A/B slot selection, when the command line names `bootstate=` | `kdos-bootctl select` |
| 10 | Volume-group activation, then the encrypted-root unlock, then activation again | `lvm`, `cryptsetup` |
| 11 | Find, check and mount the root (installed), or find the medium and build the overlay (live) | The initramfs `init` |
| 12 | `switch_root` | util-linux's `switch_root`, never toybox's |
| 13 | `/etc/init.d/rcS` | init (toybox), as its `sysinit` entry |
| 14 | The numbered service scripts ([boot order](../02-user-guide/administration.md)) | `rcS` |
| 15 | `kdos-bootctl mark-good` | `rcS`, last |
| 16 | The splash quits | `rcS` |
| 17 | `kdos-getty` on tty1 and tty2; a root shell on `ttyS0` when a key is pressed there | init, as `respawn` and `askfirst` |
| 18 | `kdos-login`, which hands tty1 to `agetty` | `kdos-getty` on tty1 |
| 19 | The desktop | `~/.bash_profile` (or `~/.zprofile` for a zsh login), on tty1 only |

Step 19 is [the session](session.md). A login on tty1 reaches it without anyone
typing a command. `tty2` is a plain getty and is the recovery console: a
machine whose GPU driver fails to come up still has a login prompt there.
Press `Ctrl+Alt+F2` to reach it from the desktop, or `Alt+F2` at a text
console, and `Alt+F1` to come back.

## When a boot stops

The splash names each stage as it runs, so the last line it shows is the stage
that stopped. The initramfs stages are, in order: `DEVICE MANAGER`, `RAID`,
`FILESYSTEM MODULES`, `BOOT SLOT`, `VOLUME GROUPS`, `UNLOCKING`, then
`ROOT DEVICE`, `MOUNTING ROOT` and `SWITCHING ROOT` on an installed machine, or
`BOOT MEDIA`, `SYSTEM IMAGE`, `OVERLAY ROOT` and `SWITCHING ROOT` on the live
medium. A stage appears only on a boot that runs it. After `switch_root`, `rcS`
adds `MOUNTING FILESYSTEMS`, one stage per enabled service script, named after
the script in capitals, and `STARTING SESSION`.

| Where it stopped | What happened | Where to look |
|---|---|---|
| Before the menu | The firmware found no boot loader | [Limine and the kernel command line](#limine-and-the-kernel-command-line) |
| An initramfs stage, then a shell | The root, the medium or the container could not be found or opened | On the live medium, the serial line or a virtual machine's serial output. On an installed machine, the screen itself (`tty0`), where the messages and the shell share the framebuffer with the splash. Then [Finding the root](#finding-the-root), [Unlocking an encrypted root](#unlocking-an-encrypted-root), [`blkid`](#blkid) |
| `ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` | `e2fsck -p` left errors on an ext root | [Checking the root](#checking-the-root) |
| `fsck: errors left uncorrected` | A non-root filesystem has errors | `/run/kdos-fsck.log` |
| A service stage marked failed | That service script exited non-zero; up to six lines of its output are on the splash | `/run/kdos-init.<name>.log`, and `/run/kdos-svc.<name>.log` for a supervised daemon |
| A login prompt with the wrong font or colours | The console setup did not complete | `/run/kdos-getty.<tty>.log` |
| A black screen after the splash | The session did not start | Press `Ctrl+Alt+F2` (`Alt+F2` at a text console) and log in on tty2; then [The session](session.md) |

Everything the initramfs prints goes to `/dev/console`, and where that is
depends on the boot (see [the `console=` row](#command-line-parameters-kdos-reads)):

- On the live medium it is the serial port, so a machine that dies behind the
  splash can still be read from a serial line or from a virtual machine's
  serial output.
- On an installed machine it is the first virtual terminal, the same screen
  the splash draws on. The splash keeps running when the init drops to a
  shell, so the messages and the shell prompt are written onto the screen it
  is still drawing.

## Limine and the kernel command line

The boot loader is [Limine](https://limine-bootloader.org/) (the `limine` port,
12.9.0), and it is the only one. The same binary, the same configuration file
and the same menu serve BIOS and UEFI, so what a machine shows at power-on does
not depend on how it started. On UEFI the firmware loads
`EFI/BOOT/BOOTX64.EFI`. On BIOS it runs the boot code in the first sector, which
finds `limine-bios.sys` by name in the root, `/boot`, `/limine` or
`/boot/limine` of a volume it can read.

`BOOTIA32.EFI` sits beside the 64-bit binary, and the two never compete. A
64-bit CPU does not imply a 64-bit firmware: early Atom tablets and a few
netbooks run this kernel and this userland but can load only a 32-bit EFI
binary. Firmware reads the one `EFI/BOOT/BOOT<arch>.EFI` it can execute and
ignores the other. Both are built from one source by `ports/core/limine`, both
are on the ISO's ESP and on an installed machine's, and the ISO's UEFI boot
record picks up whichever were built. The cost is about a hundred kilobytes and
one more freestanding build.

An NVRAM boot entry names exactly one path, so the installer has to choose. The
removable-media fallback picks between the two binaries by itself;
`efibootmgr --create` cannot, and an entry pointing at a binary the firmware
cannot load fails rather than falling through to the next option. `kinstall`
therefore reads `/sys/firmware/efi/fw_platform_size` and writes the matching
path. A kernel that does not publish that file leaves the installer assuming
64, which covers nearly every machine; where the assumption is wrong, a 32-bit
firmware still boots the disk through the removable-media fallback.

The entry also names a partition number. The installer reads it from the
kernel's `/sys/class/block/<node>/partition` for the ESP it actually wrote to,
and never derives it from the device name: only a wiped disk has its ESP at
index 1, and an install that reuses existing partitions uses whichever one was
chosen. Where the number cannot be read, the installer writes no entry and says
so. The removable-media fallback still starts the disk, while an entry naming
the wrong partition would be a boot option the firmware cannot load.

### Where the kernel is kept

The kernel and initramfs are placed where the loader is certain to read them:
on the ISO9660 filesystem for the live medium, and on the **ESP** for an
installed system. Limine reads FAT and ISO9660. It does not read most of the
root filesystems the installer offers, which include xfs, f2fs and anything
under LUKS, so a kernel on the root filesystem would make booting depend on a
driver the boot loader does not have.

Three programs write the loader configuration, and nothing else does:

| Writer | File | When |
|---|---|---|
| `script/06_packaging/02_iso.sh` | `boot/limine/limine.conf` on the live medium | At build time |
| `kinstall` | `limine.conf` at the root of the installed machine's ESP | At install time |
| `kdos-bootctl` | The `/KDOS` entries of that same file, plus `EFI/kdos/trial/limine.conf` during a UEFI update trial | Whenever the boot state changes; see [One kernel per slot](#one-kernel-per-slot) and [One boot through BootNext](#one-boot-through-bootnext) |

The root of the ESP is not an arbitrary location. Limine looks beside its own
EFI binary first and then at `/boot/limine/`, `/boot/`, `/limine/` and `/` on
each volume, and only that last set is searched on BIOS, so the root of the ESP
is the one path both firmwares find. A second copy beside `BOOTX64.EFI` would be
the copy that goes stale. The single exception is `EFI/kdos/trial/limine.conf`,
beside a copy of the loader, which `kdos-bootctl` writes from the root's file
for the length of a UEFI update trial and deletes afterwards.

`fwupd` never touches the loader configuration. It writes to the ESP only while
a firmware update is staged: a capsule under `EFI/kdos/fw/` with
`EFI/kdos/fwupdx64.efi` and a one-shot `BootNext` entry naming it, or a capsule
under `EFI/UpdateCapsule/` for firmware that reads one from disk. The boot
order is not changed, and the boot after the update is Limine's again.

### A new kernel

The `linux` package (kernel 7.2.7) installs its kernel as
`/boot/vmlinuz-kdos`. Nothing boots that file until `kdos-bootctl deploy` copies
it into its slot's directory on the ESP. Each root slot boots its own kernel
from `EFI/kdos/<slot>/`, because a kernel's modules exist only in the root it
was installed into.

The package's postinstall builds the new kernel's initramfs into the same root,
as `/boot/initramfs-kdos.cpio.gz`. It is the image's own
`/boot/initramfs.cpio.gz` with one more archive appended:

- The base carries the microcode, which the early loader finds only at the very
  start of the file, and the `init` script.
- The appended archive carries the new kernel's copies of the modules listed in
  `/boot/initramfs.modules` (the list `01_initramfs.sh` used, written beside
  the image's initramfs) and their dependencies. A root with no such list gets
  the set the base archive itself carries: every module under its
  `lib/modules/`, found by walking past the uncompressed microcode archive to
  the compressed part.

The kernel unpacks concatenated archives into one tree, and the init's
`modprobe` looks under `uname -r`, so it finds the new set. The init itself is
therefore always the one the image shipped. If the build fails, the postinstall
removes the previous `/boot/initramfs-kdos.cpio.gz` too, since it belongs to
the kernel just replaced, and a `deploy` of that root is then refused (see
below) until one is built.

Who deploys depends on the root the package went into:

| Installed into | Deployed by | When |
|---|---|---|
| The running system | The postinstall, with `kdos-bootctl deploy /` | At once; the next boot runs the new kernel |
| The inactive slot | `kdos update apply`, with `kdos-bootctl deploy <mount> <slot>` | After every package of the run is in, before `try` |

`deploy` behaves as follows:

- It writes the initramfs and then the kernel, each beside the old file,
  flushed and renamed over it, so the directory never holds a new kernel beside
  an initramfs without its modules.
- It refuses before writing anything when the ESP cannot hold both files.
- It takes `/boot/initramfs-kdos.cpio.gz` where the postinstall wrote one, and
  the image's `/boot/initramfs.cpio.gz` otherwise. Packaging deletes the first
  from the image, so an ISO carries only the pair built together.
- It refuses an initramfs that holds module trees for other kernels and none
  for this kernel's version, and writes nothing. That init could not load
  `vfat` to read the boot state, so it would count no attempt and never roll
  back, and it could not mount a root on xfs, f2fs, LUKS or md. The slot keeps
  the kernel it had, and `kdos update apply` does not try it. An initramfs with
  no module tree at all passes, as does one that cannot be read.
- On a machine with no boot state it writes into the directory that the first
  `/KDOS` entry's `path:` names: `EFI/kdos/a/` on anything `kinstall` wrote, or
  the flat `EFI/kdos/` on a menu whose entries boot the flat pair.

In every root, the postinstall keeps the module tree of every kernel on the ESP
(it reads each version out of the bzImage header) as well as the running
kernel's, and removes the rest. Until a root's own kernel is deployed, the next
boot of that root runs one of those kernels, and a root stripped of their
modules would boot with no GPU, network or sound driver.

### The menu

The menu counts down for ten seconds (`timeout: 10`), on the live medium and on
an installed system alike. Any keypress stops the countdown and leaves the menu
up. That countdown is time spent before the kernel exists, but it is what makes
the recovery entries reachable, and a countdown short enough to miss makes them
unreachable on exactly the machine that needs them. Setting `timeout: 0` does
not boot at once with a menu: it boots the default entry without drawing a menu
at all, and those entries go with it.

| Live medium | Installed machine |
|---|---|
| `KDOS Live` — start from the medium | `KDOS` — start this machine |
| `KDOS Live (clean session)` — adds `nopersist`, ignoring the persistence store | `KDOS (verbose)` — `loglevel=7`, every kernel message |
| `KDOS Live (verbose)` — `loglevel=7` | `KDOS (single user)` — `loglevel=7 single`: a verbose boot. Nothing in KDOS acts on `single`, so the services and the desktop start as usual |
| `Memory Test (memtest86+)` — UEFI only | `KDOS (slot <x>)` — the other root slot, when it has a root and a kernel |
| | `Memory Test (memtest86+)` — UEFI only |

The memtest86+ payload is an EFI binary, so its entry carries
`if_fw_type: UEFI` and is hidden on a BIOS boot, where it could not start. The
entry is left out when the payload
(`/usr/share/kdos/memtest86plus/memtest.efi`) is not installed.

The menu is drawn as a character grid in the
[accent](../06-reference/glossary.md) in force (the colour scheme the machine
is set to), in Terminus
`ter-i16n` (an 8x16 face in the CP437 encoding Limine indexes by), over a dimmed
full-screen backdrop. See
[Theming the boot menu](../02-user-guide/theming.md#the-boot-menu-the-splash-and-the-text-consoles)
for what you can change and [the design language](design-language.md) for where
the colours come from.

`kcol_limine_conf()` in libkcolor emits the whole look, and both writers call
it: the ISO step through `kdos-bootctl theme --print`, and `kinstall` directly.
One function means a USB stick and the machine installed from it cannot show
different colours. The only lines the writers own themselves are the two that
name files rather than colours, `wallpaper` and `term_font`, because those files
are in different places for each:

| File | On the live medium | On an installed machine |
|---|---|---|
| Font | `psf2limine.py` converts `ter-i16n` during packaging and writes `boot/limine/font.bin` straight into the ISO tree. It is not in the root filesystem. | `kinstall` copies `/boot/limine/font.bin`, or `/mnt/iso/boot/limine/font.bin` (where the initramfs mounts the medium), to `EFI/kdos/font.bin` |
| Backdrop | `/usr/share/kdos/boot/kdos-backdrop.png`, falling back to `kdos-banner.png`, copied to `boot/limine/wallpaper.png` | The same two files, which the `fs/` overlay ships in the root, copied to `EFI/kdos/wallpaper.png`, so an install run with no medium mounted still gets its artwork |

Either file missing is survivable: Limine draws its own face on a plain
backdrop, and the entries are unchanged.

### Why the plate is opaque

The menu's plate (the rectangle the entries are drawn on) is opaque, and its
backdrop is stretched. Both choices are about legibility.

`term_background` carries a leading transparency byte, emitted as `00`. With a
transparent plate the artwork could be any size, but Limine prints
`linux: Loading kernel …` at the terminal's own origin the moment an entry is
picked, and with no plate those lines land across the artwork. With an opaque
plate the loading text can only appear inside it. Limine's own default over a
wallpaper is `80`, half transparent, which makes the menu unreadable.

A `centered` wallpaper is drawn at its own size in the middle of the screen,
which is exactly where the menu is, so the style is `stretched`.
`term_font_scale` is `1x2`: doubling both axes of an 8x16 face would fill a
1080-row screen with four entries.

No placement avoids the loading text at every resolution, which is why the
plate is opaque rather than the artwork being moved. The text's position is in
pixels from the margin; the artwork's is a fraction of a stretched wallpaper. A
banner that clears the text at 1080 lines runs underneath it at 720.

### The margin ceiling

The artwork lives in the margin above the plate, and the margin cannot grow
freely. A margin that leaves the terminal under sixteen rows makes Limine
abandon the graphical terminal altogether. Measured by booting one ESP per
value at 1280x800 with these keys, a `term_margin` of 144 keeps sixteen rows and
themes correctly, while 152 leaves fifteen and comes up in Limine's own font,
its own palette, its own branding and no wallpaper at all. That is not a
degraded theme; it is no theme.

A row is `term_font_size` times `term_font_scale`, 16 by 2, so the ceiling is
`(height − 512) / 2`: 144 at 800 lines, 104 at 720, 44 at 600. The shipped value
is 100, which clears 720 lines and everything above. `term_margin_gradient` is
8, a short fade at the plate's edge that does not reach up into the artwork.

`interface_branding` is emitted **empty**, and empty is not the same as absent.
The artwork is the branding, so a value would print "KDOS" in a font directly
under a picture of it; but if the key is left out, Limine falls back to its own
default and prints `Limine <version> (x86-64, UEFI)` in its own cyan.
`interface_help_hidden: yes` removes the key-help block. The countdown line
still draws in the help colour, so `interface_help_colour` is set anyway, or
the countdown would be Limine's default green under every accent.

### The artwork

The backdrop is the penguin and the wordmark, sized to fit the margin.
`src/packages/kdos-splash/genbackdrop.py` lays them on a fixed near-black floor,
`(10,10,9)`, in greyscale. It is achromatic because Limine cannot retint a
wallpaper: this one file sits under all eight accents, and a green penguin would
stay green under amber. Four of the generator's constants are measured rather
than chosen, and each prevents something that would look like a rendering
defect:

- **Height is 6.3% of the screen, because the margin is 100 pixels.** The
  wallpaper is stretched, so the artwork scales with the screen while the
  margin does not; it has to clear 100 pixels on the tallest screen it will be
  seen on. 6.3% is 68 pixels at 1080 lines and 90 at 1440, the largest
  resolution the tree references, and the plate starts covering it above about
  1580.
- **The captions are dropped.** At this height the two lines of small print
  would be three pixels tall; including them would cost the wordmark a third of
  its own height to render something illegible.
- **The ink is the maximum channel, not the luminance.** The wordmark is
  phosphor green, `(57,255,20)`, which Rec.601 luminance puts at 169 while the
  penguin's white lands at 249, so a plain greyscale conversion leaves the
  letters visibly duller than the mascot beside them.
- **The black point is 64, and that is what stops the rectangle.** The source
  banner carries an ambient green glow over its whole area: its corner pixel is
  `(18,64,31)`, not black, so composited onto the floor it would lift its own
  area and draw a box around itself. 64 is the 95th percentile of the border
  ring. Subtracting it takes the border to exactly zero at a cost of 2% of the
  strong ink.

The downscale is `BOX` (area averaging) because the wordmark is pixel art, and a
photographic filter on hard edges gives blocks of different widths with soft,
jagged edges. There is no vignette and no texture: a wordmark folded onto itself
reads as grain at full size and as smudges at a sixth of it, and a flat floor
cannot band. The generator is a host script whose output is committed, the same
arrangement `genbanner.py` and `genlogo.py` use. Run it with `--check` to report
the current file without rewriting it.

### Restamping an installed machine

`kdos-bootctl theme <accent>` rewrites the look of the boot menu and the text
consoles in place:

- In `limine.conf` it touches only the keys the theme owns and leaves every
  entry, `cmdline`, `timeout`, `default_entry`, `wallpaper`, `term_font` and the
  A/B state alone. A restamp that rewrote the file from a template would discard
  a slot somebody is in the middle of rolling back. `wallpaper_style` and
  `term_font_scale` are among the keys it owns, so a restamp also sets the
  menu's layout.
- It writes the accent's sixteen console colours to `/etc/vtrgb`, which
  `kdos-getty` loads onto each virtual terminal before the login prompt. The
  change shows at the next login prompt.

Both writes are temporary file, fsync, rename, fsync the directory. The ESP is
FAT with no journal, and a zero-length `limine.conf` is a machine that shows no
menu. An accent name that is not one of the compiled-in schemes is refused with
exit status 2.

`kdos theme` reaches this through `kdos-powerd accent`, which also writes
`/etc/kdos/accent` for the splash. A machine with no `limine.conf` at the
default path (the live medium, for instance) retints its consoles, reports that
the boot menu is unchanged and exits 0; the desktop still retints.

### Command-line parameters KDOS reads

| Parameter | Read by | Meaning |
|---|---|---|
| `root=UUID=<fs-uuid>` | The initramfs | The filesystem to mount as root. The live medium passes `root=/dev/ram0`, which the initramfs ignores; with no `root=UUID=` it searches for the medium instead. |
| `cryptdevice=UUID=<luks-uuid>:<name>` | The initramfs | A LUKS container to open before looking for the root. `/dev/<node>:<name>` is also accepted, and an empty name becomes `kdosroot`. |
| `bootstate=UUID=<esp-uuid>` | The initramfs, for `kdos-bootctl` | Which ESP holds the A/B state file. Without it no slot selection runs. |
| `kdos_slot=a` or `b` | `kdos-bootctl select` and `mark-good` | The slot whose ESP directory this kernel came from. Every installed-machine entry carries one. |
| `nopersist` | The initramfs, live medium only | Ignore the persistence store; this session is not saved. |
| `panic=10` | The kernel | Added to the candidate's entry during a UEFI update trial, unless the command line already sets `panic=`. The kernel is built with no panic timeout, so without it a panicking kernel waits for the reset button. |
| `single` | Nothing | Carried by the installed menu's `(single user)` entry. The kernel passes it to init, and toybox init ignores its arguments, so that entry boots like `KDOS (verbose)`. KDOS has no single-user mode. |
| `quiet loglevel=3`, `loglevel=7` | The kernel | Normal and verbose entries. |
| `console=` | The kernel | Where kernel messages and `/dev/console` go. The last one named wins. |

The live medium's entries carry `console=tty0 console=ttyS0`, so there the last
`console=` is the serial port. Kernel and initramfs messages go to the serial
line and the display shows nothing, which is what leaves the framebuffer free
for the graphical splash. An installed machine's entries carry `console=tty0`
alone, so there `/dev/console` is the first virtual terminal. Either way the
encrypted-root prompt is drawn through the splash and read from `/dev/tty1`
explicitly, rather than through `/dev/console`.

## Microcode

The kernel is built with `CONFIG_MICROCODE=y` and late loading off, so its
early loader is the only way microcode reaches the processor. That loader runs
before any filesystem exists, and scans the raw initrd for two literal paths:

```
kernel/x86/microcode/GenuineIntel.bin
kernel/x86/microcode/AuthenticAMD.bin
```

The image the boot loader hands over is therefore two archives concatenated: a
plain, uncompressed cpio containing those two files, followed by the gzipped
initramfs. Three properties must hold, and breaking any of them means microcode
is silently never applied.

**Nothing in the microcode archive may be compressed**, neither the archive nor
the files inside it. `linux-firmware` ships AMD's microcode as `.zst` files
under `amd-ucode/` because the *runtime* firmware loader can decompress; the
early loader cannot, so the build expands them on the way in and concatenates
one container per family.

**The Intel bundle is not curated.** The `intel-ucode` port builds
`/usr/lib/firmware/intel-ucode.bin` from upstream's whole set, minus the images
upstream ships separately under `intel-ucode-with-caveats/` because they need
coordinated firmware support. A per-family prune would boot on the machines it
covers and leave every other one silently unpatched.

**The Intel bundle must be consumed to exactly its last byte.** The kernel's
scanner returns nothing at all if any trailing data remains, so one stray file
in the bundle means *no* microcode is loaded, not "everything before the bad
record". The port walks the records the same way when it builds and fails
rather than shipping such a bundle.

When a microcode source is missing, the build says so (for example
`Microcode: no amd-ucode blobs — AMD CPUs will run BIOS microcode`) and carries
on. An initramfs without microcode has no symptom, because the processor keeps
whatever the firmware loaded, so `kdos doctor` (the
[health check](../04-programs/kdos-command.md#kdos-doctor)) runs the kernel's own search
against `/boot/initramfs.cpio.gz` and reports the running revision.

## The initramfs

`script/06_packaging/01_initramfs.sh` generates it. It carries toybox and bash,
a module set, and the handful of programs early boot needs.

The module list decides which machines can boot at all, because a root
filesystem whose driver is missing installs perfectly and never boots again:

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
LVM targets        dm-snapshot dm-thin-pool dm-cache dm-cache-smq dm-writecache
```

Each module's dependencies are copied with it, and a module built into the
kernel is skipped. ext4 and btrfs are built in; xfs and f2fs are modules and
appear here for exactly that reason. **Every filesystem the installer offers
has to be in this list**: `ki_filesystems[]` in the installer is what offers
the choice, and this list is what makes the choice bootable.

- **LUKS ciphers.** These are the ones a LUKS2 default header uses. They are
  carried unconditionally because they are small, and on a kernel that has them
  built in the copy is a no-op.
- **RAID personalities.** They are listed individually because `md_mod` loads
  none of them. A machine whose data disks are an array needs them before udev
  settles; without `md_mod` the members are bare disks with a superblock nobody
  reads, which looks like an empty drive rather than a missing module.
- **LVM targets.** These are the ones a volume group can hold beyond the linear
  target built into `dm-mod`: a thin volume, a cached or write-cached one, a
  snapshot. A group holding one activates only with its target loaded.

The list is also written to the image as `/boot/initramfs.modules`, which is
what a later kernel's initramfs is built from; see [A new kernel](#a-new-kernel).

The programs carried, and the rule each follows:

| Program | Rule |
|---|---|
| `switch_root`, `mount`, `umount`, `losetup`, `dmesg` | Always util-linux's, never toybox's applets. The build stops if any copy reports itself as a Toybox multicall binary. See [`switch_root`](#switch_root). |
| `blkid` | Always util-linux's; see [`blkid`](#blkid). |
| `kdos-splash` | Carried with `/usr/share/kdos/splash.psf` when installed; otherwise the build warns and the machine boots without a splash. |
| `udevd`, `udevadm`, `kmod` (as `modprobe`, `insmod`, `depmod`) | Always, with the udev rules from `/usr/lib/udev` and `/etc/udev`. |
| `mdadm` | When installed. |
| `e2fsck` | With `libext2fs`, `libcom_err` and `libe2p`, or not at all. |
| `kdos-bootctl` | With `libpng16`, or not at all (below). |
| `cryptsetup` | With its libraries, only when installed. |
| `lvm`, `dmsetup`, `thin_check`, `cache_check` | Whenever lvm2 is installed (below). |

`cryptsetup` and its libraries are carried only when they are installed, and
the build says so when they are not. A half-carried `cryptsetup` would fail at
the passphrase prompt rather than at build time, which is the wrong place to
find out.

`lvm` and `dmsetup` are carried whenever lvm2 is installed, and lvm2 comes with
`cryptsetup` and `parted` on every image that has them. They are copied to
`/usr/sbin`, the paths lvm2's udev rules name, rather than to `bin/`. The rules
travel with the rest of `/usr/lib/udev`, and `95-dm-notify.rules` runs
`/usr/sbin/dmsetup udevcomplete` for every device-mapper change. `lvm`, and
`cryptsetup` opening a container, wait for that call with no timeout, so a
`dmsetup` the rule cannot find would hang the boot at the unlock or the
activation with nothing on screen.

Their libraries are not listed by hand. `copy_closure` reads each program's
`NEEDED` entries with `readelf` and copies the libraries they name, and theirs,
from `/usr/lib` or `/lib`. For `lvm` that is libdevmapper, libdevmapper-event,
libaio, libblkid, libudev, readline and libnvme with what libnvme links.
`thin_check` and `cache_check` come with them, as argv[0] links to
thin-provisioning-tools' single binary, `pdata_tools`, which is copied with its
closure; `lvm` runs the matching one before it activates a thin pool or a cache,
and refuses the volume without it. A generated `etc/lvm/lvm.conf` names those
two paths and nothing else.

The build stops, naming the problem, when:

- lvm2 is installed and `lvm`, `dmsetup`, `thin_check` or `cache_check` is
  missing;
- a udev rule names `dmsetup` or `lvm` anywhere but `/usr/sbin`;
- a library in a program's closure is installed nowhere;
- no working `readelf` is found. `readelf`, or `llvm-readelf` in its place,
  must find libblkid among `blkid`'s `NEEDED` entries. A missing reader would
  fail inside a command substitution, which `set -e` does not catch, and every
  program would be copied without its libraries;
- any program in `bin/` or `usr/sbin/`, or any library in `lib/`, names a
  library the initramfs does not carry. Without this check the failure is a
  kernel panic (`Attempted to kill init`) or a tool that says "not found" at the
  one moment it is needed.

`kdos-bootctl` follows the same rule, and it needs `libpng16`.
`/usr/bin/kdos-bootctl` is a symlink to `/usr/sbin/ksvc` (the multi-call
binary that also supervises the [daemons](../04-programs/daemons.md)), which links libpng for
`kdos theme`'s wallpaper retint, so the copy brings that dependency into an
initramfs whose other unconditional libraries are libc, libintl, libmount,
libsmartcols, libblkid, libuuid, libudev, libkmod, liblzma, libz, libzstd and
bash's readline, history and ncursesw. libpng16 is carried beside it (libz is
already there and musl's libm is inside libc, so the set is closed), and the
copy is skipped entirely when libpng is absent. A `kdos-bootctl` that cannot run
would mean A/B slot selection never happens, and the machine would look like one
whose slot was never marked good rather than one missing a library.

### Finding the root

On an installed machine (the command line names `root=UUID=`), the init asks
`blkid` for the root UUID once a second for up to ten seconds, activating any
volume group that has appeared in between. On the live medium it rescans
`/dev/sr* /dev/sd* /dev/vd* /dev/nvme*` every 100 ms, for up to ten seconds, and
uses the first device that mounts as ISO9660 and holds `system.sfs`. Neither
waits before its first attempt: udev has already settled by then, so on a
machine whose medium is already enumerated the first pass succeeds and costs
nothing. The ten-second bound covers the slow cases, such as a USB stick or a
device behind a bridge.

Every pass of the live scan walks every device class again, because the first
node to answer is not always the one holding the medium, and a class that has
not appeared yet must still get its chance. A device that mounts is inspected
only once, though: a filesystem without `system.sfs` on it will not grow one, so
it is remembered and skipped, and only nodes that have not mounted yet are
retried. That is one mount and unmount for a wrong disk across the whole scan,
rather than one per pass.

`/mnt/iso` is the only mount point the scan has, so the unmount is checked. A
mount point left busy would take a second filesystem stacked on top of it and
every later test would read the wrong one, so a failed unmount stops the scan
with `Cannot release /mnt/iso after <dev>`. Only the first pass reports each
device it tried; a hundred repetitions of the same two lines would bury the
message that explains a failed boot.

When the root, the medium or `/sbin/init` on it cannot be found, the init marks
the stage failed on the splash and starts a shell on `/dev/console`.

### The live medium and persistence

On the live medium the init mounts `system.sfs` read-only through a loop device
and puts an overlay on top of it as the new root. The overlay's writable upper
layer decides whether the session survives a power-off:

- A filesystem labelled `KDOS_PERSIST` is used as the upper layer when one is
  present, in `upper/` and `work/` directories on it. `kdos persist` creates
  such a store.
- With no store, the upper layer is a tmpfs and the session is gone at reboot.
- `nopersist` on the command line (the menu's `KDOS Live (clean session)`
  entry) ignores the store. That entry exists so a store holding a change that
  stops the desktop coming up can be bypassed without taking the stick to
  another machine.

The store must support extended attributes, hard links and file types in
directory entries, so a store on vfat, exfat, ntfs, iso9660 or squashfs is
refused by name. Any failure with the store (wrong type, will not mount,
overlay refused) falls back to the tmpfs upper layer, and the session comes up
as it would with no store at all.

Before `switch_root`, `/mnt/iso` and `/mnt/persist` are moved into the new root.
Left behind, they would be unreachable by name: `kdos rebuild` looks in
`/mnt/iso/sources`, `kdos persist` reports how full the store is, and the
shutdown's `umount -a -r` can only flush a store that is in the new root's
mount table.

### Checking the root

An ext2, ext3 or ext4 root is checked with `e2fsck -p` before it is mounted,
which is the only point at which nothing is using it. Exit status 1 and 2 mean
it repaired something, and since the root is not mounted there is nothing to
reboot for. Exit status 4 and above means errors were left behind: the splash
shows `ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` and the boot goes on. A shell
at this point would open on `/dev/console`, which the splash is still drawing
over, while a machine that boots shows the warning and leaves `e2fsck` to be
run from a working system. btrfs, xfs and f2fs
are not checked here; each checks itself at mount time, which is why the
installer gives them pass number 0 in `fstab`.

Every other filesystem with a pass number is checked before it is mounted:

- `rcS` runs `fsck -A -R -T -a` before `mount -a`, which checks every one whose
  device exists before udev has run in the real root. In practice that is the
  ESP, which is FAT with no journal and holds the A/B boot state. Errors it
  could not correct are reported on the splash.
- A filesystem on a logical volume the initramfs did not activate (any volume
  on a live boot, or on a disk that appeared after the initramfs ran) has no
  device until `03_lvm` activates its volume group, so `rcS` cannot check it.
  `03_lvm` runs `fsck -A -R -M -T -a` once the group is active, which checks
  only what is not yet mounted, then mounts it, and reports errors left
  uncorrected in its own log.

Both passes write to `/run/kdos-fsck.log`.

## The splash

`kdos-splash` draws a CRT power-on animation directly to `/dev/fb0`, with glyphs
scaled up from its own font (`/usr/share/kdos/splash.psf`). It is a static
binary of about 1,200 lines of C, and every failure path exits without drawing:
a boot animation must never be able to stop a boot.

| Command | Effect |
|---|---|
| `kdos-splash run` | Draw, and read commands until told to quit |
| `kdos-splash step "NAME"` | Begin a stage |
| `kdos-splash ok`, `kdos-splash fail` | Close the current stage |
| `kdos-splash msg "text"` | Show a plain line |
| `kdos-splash detail "text"` | Show a line of failure detail |
| `kdos-splash total N` | Add N to the expected number of stages |
| `kdos-splash accent NAME` | Repaint in that accent |
| `kdos-splash quit` | Run the power-off animation, then exit |
| `kdos-splash preview WxH T out.ppm` | Render the frame T seconds into the power-on animation to a PPM file, without a framebuffer; a T past the animation gives the settled picture |

The kernel is built with deferred framebuffer-console takeover, so nothing
claims the framebuffer until text is written to a virtual terminal, and a
process is free to draw on it. Three facts constrain the splash.

Deferred takeover means nothing is being displayed yet. Writing to `/dev/fb0`
before the display driver's own client does its mode set paints a buffer nobody
is looking at. One byte written to `/dev/tty0` ends the deferral; the splash
writes a clear-and-hide-cursor sequence, which ends it and hides the cursor in
one action.

Memory-mapped writes need an explicit flush. They reach the display only when
the deferred I/O worker gets round to it, so each frame ends with a pan-display
call to the offset it is already at.

The process survives `switch_root` with a ghost root. It is never `chroot`ed,
so afterwards its `/` is the deleted initramfs root. Open file descriptors keep
working, which is how one process spans both halves of boot, but every path it
resolves *by name* afterwards points into the ghost. Commands reach it through
a FIFO, `/dev/.kdos-splash`, which survives because `/dev` is moved into the new
root rather than remounted. The `quit` client detects the running splash by
opening the FIFO and watching for a specific error, and does the cleanup itself
from the real root.

The splash starts in the default accent, because no root filesystem exists when
the initramfs starts it. As soon as `/etc` is readable, `rcS` reads
`/etc/kdos/accent` and sends `accent`, so the splash repaints into the machine's
own scheme before the first service runs.

Adding a stage is one line on either side: a step-and-ok pair in the generated
init, or in `rcS`, which already wraps every service script in one.

The progress total is additive: each part of boot adds its own stage count as
soon as it knows it, so `done == total` happens at every boundary between
parts. The bar therefore stops one segment short of full until `quit` arrives,
so it never shows 100% before the boot has finished.

To work on the artwork without booting, render a frame to a PPM file:

```sh
kdos-splash preview 1280x800 0.35 out.ppm
```

## Unlocking an encrypted root

The kernel command line carries `cryptdevice=UUID=<container>:<name>`, using
Arch Linux's syntax because it is the one people already know. Two different
UUIDs are involved: `cryptdevice=` names the LUKS container, and `root=` names
the filesystem inside it, which does not exist until the container is open.
`kinstall` writes both. The generated init unlocks before it looks for a
filesystem for that reason.

The prompt is designed around three facts:

- It draws through the splash, not on `/dev/console`. On an installed machine,
  the only place the unlock runs, `/dev/console` is the first virtual terminal,
  and the splash owns that screen's framebuffer: a prompt printed there would
  be hidden under the splash or fight it for the screen.
- It reads keystrokes from `/dev/tty1`, which is where the keyboard is.
- It feeds the passphrase to `cryptsetup` on standard input, never as an
  argument: `/proc/<pid>/cmdline` is readable by every process for the life of
  the process.

The splash shows `PASSPHRASE FOR <name> (attempt N of 3)`, then `UNLOCKED` or
`WRONG PASSPHRASE`. After three wrong attempts the boot drops to a shell rather
than into a reboot loop. There is no per-keystroke feedback, because the splash
owns the framebuffer and the shell owns the terminal; nothing appears as you
type.

## Activating volume groups

A root on an LVM logical volume boots with the ordinary command line: `root=`
names the UUID of the filesystem on the volume, as it would for a partition.
The generated init activates volume groups before it looks for that UUID,
because a filesystem on a logical volume has no device node until its group is
active.

It activates on each side of the unlock, because LVM and LUKS stack both ways.
A volume group inside a container exists only after the unlock, and a container
on a logical volume needs its group active before the unlock. It activates
again on each pass of the ten-second wait for the root, so a physical volume on
a disk that enumerates late still brings up its group before the wait gives up.

Each activation first asks `blkid -t TYPE=LVM2_member` for the physical
volumes, and runs `lvm vgchange -aay --sysinit` only when that set is not empty
and differs from the set it last activated. So:

- A disk boot without LVM costs one `blkid` per call and never starts `lvm`.
- A live boot, which has neither `root=UUID=` nor `cryptdevice=`, makes no call
  at all; its groups are activated by `03_lvm` after `rcS` starts.
- A failed activation marks the `VOLUME GROUPS` stage failed on the splash and
  the boot goes on. If the root was in that group, the root lookup reports it
  missing.

`--sysinit` turns off dmeventd monitoring, background polling and locking
failures, none of which an initramfs can provide. The carried `lvm.conf` names
`thin_check` and `cache_check` and nothing else, so every other setting is the
compiled default and every group found is activated. The groups stay active
across `switch_root`, and `03_lvm`'s own `vgchange` leaves them as they are.

A root on a thin or a cached volume boots like any other. The first call that
finds a physical volume loads the thin, cache, write-cache and snapshot targets
before it runs `lvm`, because a group holding such a volume activates only with
its target present, and `lvm`'s own module loading uses whatever `modprobe`
path its configure step found at build time. `lvm` then runs `thin_check` or
`cache_check` on the pool's or the cache's metadata before activating it.

The installer creates this layout on its erase plan and offers an existing
logical volume as the root on its reuse plan; see
[kinstall](../04-programs/kinstall.md#a-root-on-lvm). Either way `root=` is the
filesystem's UUID and nothing on the command line names LVM. Under encryption
the installer puts the group inside the container, so the activation after the
unlock is the one that finds it.

## A/B slot selection

A machine set up for A/B updates has two root partitions, a state file on the
ESP, and a boot that can change its mind. The state file is
`EFI/kdos/bootstate` on the ESP (`/boot/efi/EFI/kdos/bootstate` on the running
system):

```
slot_a   = <filesystem uuid>
slot_b   = <filesystem uuid>
crypt_a  = <luks uuid>      the container that filesystem is inside, or empty
crypt_b  = <luks uuid>
active   = a                the slot known to work
try      = b                a candidate, or empty
attempts = 1                how many boots it has left
bootnext = yes              tried through UEFI BootNext, or empty
```

`kinstall` writes the first version: `slot_a` and `crypt_a` filled in,
`active = a`, no candidate. Slot B is described later with
`kdos-bootctl set-slot b <uuid> [<luks-uuid>]`, and `kdos update apply` installs
into it, deploys its kernel and calls `try`. Nothing in KDOS creates or first
populates slot B's partition; see
[Known gaps](../06-reference/known-gaps.md#boot-and-updates).

### `kdos-bootctl`

`kdos-bootctl` is one of the names of the `ksvc` multi-call binary from
`kdos-tools`. Its verbs:

| Command | What it does |
|---|---|
| `kdos-bootctl status [--json]` | Print what the state file says |
| `kdos-bootctl set-slot <a\|b> <uuid> [<luks-uuid>]` | Record where a slot's filesystem is, and which container it is inside |
| `kdos-bootctl crypt <fs-uuid>` | Print the container UUID that filesystem is inside, for the initramfs |
| `kdos-bootctl try <a\|b> [n]` | Put that slot on trial: one boot through UEFI `BootNext`, or `n` boots led by the menu where there is no `BootNext` (default 3, minimum 1) |
| `kdos-bootctl deploy <root> [<a\|b>]` | Put that root's kernel and initramfs in its slot's directory on the ESP |
| `kdos-bootctl select [<a\|b>]` | Decide which root to boot and spend an attempt (run by the initramfs); the argument is the entry's `kdos_slot=` |
| `kdos-bootctl mark-good` | Confirm that this boot worked (run at the end of `rcS`) |
| `kdos-bootctl theme <accent>` | Repaint the boot menu and the text consoles in that scheme |
| `kdos-bootctl theme --print [<accent>]` | Print the menu's theme block to standard output |
| `kdos-bootctl palette [<accent>]` | Print that scheme's `setvtrgb` table to standard output |

Environment variables point it at files other than the real ones, which is how
the selftest exercises it against fixtures without touching the machine running
it:

| Variable | Default | Stands in for |
|---|---|---|
| `KDOS_BOOTSTATE` | `/boot/efi/EFI/kdos/bootstate` | The state file |
| `KDOS_LIMINE_CONF` | `/boot/efi/limine.conf` | The boot menu |
| `KDOS_VTRGB` | `/etc/vtrgb` | The console palette |
| `KDOS_EFIVARS` | `/sys/firmware/efi/efivars` | The firmware variables. With `KDOS_BOOTSTATE` set to anything else and this unset, no firmware variable is written. |
| `KDOS_CMDLINE` | `/proc/cmdline` | The kernel command line. With `KDOS_BOOTSTATE` redirected and this unset, no `kdos_slot=` is read. |
| `KDOS_ESP_DISK` | Read from the mount table and `/sys/class/block` | `<image>:<partition>`, the disk and partition number of the ESP |

### Counting and confirming

The counting lives in the initramfs, and that placement is the design. `rcS`
would be the wrong place: a kernel that boots into a wedged userland must still
spend an attempt, and the `rcS` in that userland never runs to say so.
`kdos-bootctl select` decides and decrements in one step, before anything is
mounted, and prints the UUID to boot:

| State | `select` does |
|---|---|
| No candidate | Boots the active slot |
| A candidate with attempts left | Spends one and boots the candidate |
| A candidate with no attempts left | Rolls back: forgets the candidate and boots the active slot |

`kdos-bootctl mark-good` is the other half. It runs at the *end* of `rcS`,
after every service that was going to fail has had its chance. A bad update
therefore rolls itself back with no help: on UEFI after its one boot, on BIOS
after its allotted number. It confirms a candidate only on the candidate's own
kernel: when the command line's `kdos_slot=` names another slot, the
candidate's kernel never ran, and `mark-good` rolls it back instead. A command
line with no `kdos_slot=` is taken as the candidate's.

`try` picks between the two kinds of trial. Where the firmware has variables,
it writes a UEFI `BootNext` request and the candidate gets one boot; see
[One boot through BootNext](#one-boot-through-bootnext). A BIOS boot has no such
request, and neither does a UEFI machine where the request cannot be written;
there the menu leads with the candidate for `try`'s count of boots.

The state file lives on the ESP, which is FAT and has no journal. A torn write
there does not just fail an update, it leaves the machine unbootable: the
initramfs cannot tell which slot to boot. So every write is temporary file,
`fsync` the **file**, `fsync` the **directory**, then rename. The directory
`fsync` is the step most often left out, and without it the rename can be lost
while the data survives.

A state file that does not parse is treated as absent, never as partial. Absent
means "use the `root=` the command line already carries", which is what a
single-root machine does anyway. A `try` pointing at a slot with no root, or at
the active slot, is refused rather than recorded. An unknown key is skipped, and
a missing `crypt_a` or `crypt_b` reads as a slot with no container.

### A slot knows its own container

`slot_a` names a **filesystem** and `crypt_a` names what that filesystem is
inside. Keeping the two apart is what lets A/B work with encryption. On an
encrypted machine the root filesystem lives in a LUKS container, and the kernel
command line can name exactly one `cryptdevice=`. Two slots inside two
containers cannot both be named there, so each slot's container is recorded in
the state file and the initramfs asks for it *after* `select` has chosen:

```sh
SEL=$(kdos-bootctl select "$BOOT_SLOT")    # the filesystem, and one attempt spent
SLOT_CRYPT=$(kdos-bootctl crypt "$SEL")    # its container, if it has one
```

`crypt` is keyed by the filesystem UUID rather than by a slot name, so nothing
has to be carried between the two calls. `select` may have rolled back, and
asking "which slot did that turn out to be" would be a second decision that
could disagree with the first. Both reads happen while the ESP is still mounted
at `/esp` in the initramfs; the second reads the file the first just wrote. The
ESP is then unmounted at once, because the real root mounts it again at
`/boot/efi`.

A slot that names no container leaves `cryptdevice=` exactly as the command line
set it, which covers every unencrypted machine and every machine whose two slots
share one container. When a slot does name one, the initramfs opens it under
its own mapper name, `kdosroot`, never a name from the state file: only one
container is open at a time there, so a per-slot name would disambiguate
nothing, and a name read out of a file on the ESP is a name somebody can edit
into a path.

Without this split, selecting slot B would unlock slot A's container and then
look for B's filesystem inside it. There is nothing there, and the failure
would read as a corrupt filesystem rather than as a lookup that was never made.

### One kernel per slot

Each slot boots the kernel in its own ESP directory, and the modules for that
kernel exist only in that slot's root:

```
EFI/kdos/a/vmlinuz              slot A's kernel
EFI/kdos/a/initramfs.cpio.gz    and the initramfs built with it
EFI/kdos/b/...                  slot B's, once an update has deployed it
EFI/kdos/bootstate              the state file above
```

Limine chooses the kernel before anything of KDOS's runs, and has no boot
counting, so the menu is part of the state. `kdos-bootctl` regenerates the
`/KDOS` entries of `limine.conf` on every change (`set-slot`, `try`, `deploy`,
`select`, `mark-good`) and writes the file only when the text differs:

| Entry | Boots |
|---|---|
| `/KDOS`, `/KDOS (verbose)`, `/KDOS (single user)` | The slot an unattended boot must reach: the active slot, except during a menu-led trial, where it is the candidate while it has attempts left |
| `/KDOS (slot <x>)` | The other slot, when it has a root and a kernel |

The first entry's comment says which: `Start this machine (slot a)`, or
`Try the update in slot b` during a menu-led trial. `default_entry` points at
the first `/KDOS` entry. Every other line of the file (the theme, the timeout,
memtest86+) stays where it is. The command line comes from the first `/KDOS`
entry: `root=` and `kdos_slot=` are set per slot, the verbosity per entry, and
every other word carries over, including anything you added by hand. Nothing is
written when the file has no `/KDOS` entry to learn a command line from, when no
slot has a directory of its own, or when the slot to lead with has no kernel.

Every entry carries `kdos_slot=<x>`, and the initramfs hands it to `select`.
That is how a hand-picked entry is recognised: its slot is not the one the menu
leads with. It boots its own slot and no other, because the running kernel is
that slot's.

- **Picking the confirmed slot while a candidate is on trial** abandons the
  candidate. On BIOS that entry is the way back from a candidate kernel that
  dies before the initramfs can count anything.
- **Any other hand-picked entry** is one boot of that root. No attempt is spent
  and nothing is confirmed, except a candidate picked after its last attempt,
  which is still the candidate and is confirmed by `rcS` as usual.

In a menu-led trial, spending the last attempt moves the menu's lead back to the
active slot. A candidate that fails that boot is therefore rolled back by the
next boot of the confirmed slot's own kernel, with no reboot in between.

`try` refuses a slot that has no kernel on the ESP once any slot has a
directory of its own. The menu could not lead with it, so the confirmed slot's
entry would boot, and `select` would read that as a hand pick and abandon the
candidate.

An ESP that no `deploy` has touched holds one flat pair, `EFI/kdos/vmlinuz` and
`EFI/kdos/initramfs.cpio.gz`, that either slot boots. `kdos-bootctl` leaves a
menu with no per-slot directory exactly as it is. A slot without a directory of
its own boots the flat pair, and the pair is deleted once no entry names it.
The next `kdos update` of each slot therefore gives that slot its own
directory.

The init inside an initramfs is the image's, and a `linux` update only appends
modules to it. A slot's init, and the `kdos-bootctl` copied beside it, are
replaced only when that slot itself is updated; no update rewrites the init of
a slot it is not installing into. A confirmed slot therefore runs the pair it
was installed with until its own next update, and that pair is the one that
carries out a rollback. Two consequences follow:

- `mark-good` regenerates the menu on every boot, confirmed or not, so a menu
  left leading with the wrong kernel is corrected on the first boot that
  reaches the end of `rcS`.
- If the menu still leads with a candidate kernel that dies, pick the confirmed
  slot's `KDOS (slot <x>)` entry by hand. That entry boots the slot's own kernel
  and root.

Two kernels and two initramfs images fit many times over in the 512 MiB ESP
`kinstall` creates. An install that reuses a smaller ESP gets a warning when
there is no room left for a second kernel.

### One boot through BootNext

A candidate kernel that panics or hangs before its initramfs runs cannot spend
an attempt, so a menu that leads with it would boot it again after every reset.
On UEFI the menu therefore never leads with a candidate. `try` leaves
`default_entry` on the confirmed slot and asks the **firmware** for one boot of
the candidate instead:

```
EFI/kdos/trial/BOOTX64.EFI      a copy of EFI/BOOT/'s loader (BOOTIA32.EFI on 32-bit firmware)
EFI/kdos/trial/limine.conf      limine.conf, with default_entry on /KDOS (slot <x>)
Boot####  "KDOS update trial"   a load option starting that loader, not in BootOrder
BootNext  = ####                one boot of it
```

Limine reads the `limine.conf` beside its own EFI binary before any other, so
the trial copy sees the trial menu and every other boot sees the root's. The
firmware deletes `BootNext` before it starts the loader, so whatever happens to
that boot (a panic, a hang and the reset button, a userland that never reaches
`mark-good`), the next boot starts `EFI/BOOT/` and the confirmed slot. While the
candidate is on trial, its entry in both menus carries `panic=10`, unless the
command line already sets `panic=`: the kernel is built with no panic timeout,
and without the word a panicking candidate waits for the reset button. Its
comment reads `The update on trial: one boot, then the confirmed root again`.

`select` sees the rest. The candidate's own entry spends its one boot; any other
entry while a candidate is on trial is the fallback boot, and rolls it back.
`mark-good` on the candidate's boot makes it the active slot, and only then does
it lead the menu. Every `mark-good` also deletes the `Boot####` option and any
`BootNext` still naming it, and every state that is not a pending trial deletes
`EFI/kdos/trial/`.

The option is written through `efivarfs`, which `rcS` mounts, rather than by
`efibootmgr`. It has a fixed shape: the ESP's partition entry, read from the GPT
itself (or from a primary MBR entry), and a path `kdos-bootctl` chose. Its
number is the lowest one no `Boot####` uses, or the one already carrying the
description.

Where the request cannot be made (no firmware variables, which is every BIOS
boot; no per-slot directory to build a trial menu from; or a firmware that
refuses the write), `try` says so and falls back to the menu-led trial. There, a
kernel that dies before its initramfs is recovered by hand, by picking the
confirmed slot's `/KDOS (slot <x>)` entry.

## Tools that must not be toybox's

[Toybox](https://landley.net/toybox/) is a single multi-call binary providing
many small commands (applets). Several of its applet names also belong to full
implementations that KDOS ships, and `$PATH` puts `/usr/bin` ahead of
`/usr/sbin`. The toybox recipe therefore switches off every applet whose name
another port on the image installs, so each name has one owner and resolves to
the real tool everywhere.

Phase 1 of the build (see [the build system](../05-developer/build-system.md))
also installs toybox, outside the package database, so a name it plants is
owned by no package: the later port install replaces `/usr/bin/toybox` without
removing the symlink, and the orphan sweep works from the database and never
sees it. Phase 1 therefore also switches off every applet whose real tool lives
in a different directory (`blkid`, `blkdiscard`, `rtcwake`, `nologin`, `lspci`,
`iotop`), `gunzip` and `zcat` (gzip's, installed just before toybox), `tar` and
`file`, and `netcat` and `ulimit`, which no port installs at any path. A name in
the same directory as the real tool needs no phase-1 change, because the real
port's install replaces the symlink.

| Switched off | Because the image has |
|---|---|
| `mount`, `umount`, `losetup`, `swapon`, `swapoff`, `mkswap`, `switch_root`, `blkid`, `blkdiscard`, `blockdev`, `dmesg`, `kill`, `linux32`, `nsenter`, `unshare`, `rtcwake`, `fsfreeze`, `hwclock`, `pivot_root`, `mountpoint`, `eject`, `fallocate`, `flock`, `logger`, `renice`, `ionice`, `chrt`, `taskset`, `uclampset`, `setsid`, `rfkill`, `rev`, `cal`, `mcookie`, `uuidgen`, `getopt`, `prlimit` | util-linux |
| `ps`, `top`, `free`, `pgrep`, `pkill`, `pidof`, `pmap`, `pwdx`, `sysctl`, `uptime`, `vmstat`, `w`, `watch` | procps-ng |
| `chvt`, `deallocvt`, `openvt` | kbd |
| `lspci`, `lsusb`, `killall`, `iotop`, `i2c*`, `gpiodetect`, `gpioget`, `gpioinfo`, `gpioset`, `partprobe`, `nc` | pciutils, usbutils, psmisc, iotop, i2c-tools, libgpiod, parted, netcat |
| `readelf`, `strings`, `cmp`, `clear`, `reset`, `setfattr`, `bunzip2`, `bzcat`, `gunzip`, `zcat`, `lsattr`, `chattr`, `insmod`, `lsmod`, `rmmod`, `modinfo` | binutils, diffutils, ncurses, attr, bzip2, gzip, e2fsprogs, kmod |
| `nologin`, `login`, `su`, `tar`, `patch`, `file` | shadow, tar, patch, file |
| `netcat`, `ulimit` | No such command: `nc` is the netcat port's, and `ulimit` is bash's builtin only (toybox's `prlimit` is part of its `ulimit` applet and goes with it) |

The applets are not drop-in replacements, and each difference is a feature the
machine would lose: toybox's `swapon` and `swapoff` refuse `-a`, so the swap the
installer writes into `fstab` would never be turned on; its `umount` has no
`-R`; its `mount` never runs a `mount.<type>` helper and does not know
`nofail`; its `lspci` and `lsusb` have no `-d`, which `airmon-ng`'s driver
detection relies on.

`sed`, `find`, `xargs`, `awk`, `expr` and `ln` stay in toybox. Every configure
script run between toybox and the GNU ports uses them, and phase 1 has no other
copy. The GNU ports come later in dependency order and take the names over; an
upgrade of toybox alone puts its applets back until those ports are
reinstalled.

### `blkid`

Every lookup the initramfs makes is `blkid -U <uuid>`: the root filesystem, the
ESP that holds the A/B state, and the LUKS container an encrypted root lives
inside. None of the three has a fallback.

Toybox's applet can do neither half of that. `-U` is not a lookup flag there
(the applet only reports on devices it is handed), and its prober knows ext,
vfat, ntfs, btrfs, f2fs, squashfs and swap but **not `crypto_LUKS`**. With it in
place, an installed machine prints *"Root device with UUID=… not found!"* and
drops to a shell, A/B selection silently never engages, and an encrypted root
never reaches a passphrase prompt. A live boot without a persistence store is
unaffected, because it finds `system.sfs` by mounting each device in turn and
resolves no UUID at all.

Two rules follow, the same two that `switch_root` keeps:

- Toybox's `blkid` is switched off in the recipe **and in phase 1**, beside
  `tar` and `file`, so neither build plants `/usr/bin/blkid` and the name
  resolves to util-linux's `/usr/sbin/blkid`. `getopt`, `patch`, `login` and
  `su` are switched off in the recipe alone, because util-linux, the `patch`
  port and shadow each install those names into `/usr/bin` and so replace
  whatever phase 1 left there. `blkid` is an `sbin` program and nothing would
  ever replace a `/usr/bin/blkid` link, which is why it has to be off in phase 1
  too.
- The initramfs removes `bin/blkid` before copying. With the applet compiled
  out, `./bin/toybox` does not list it and the applet loop never claims the
  name; the removal guards that, because `cp` writes *through* a symlink and a
  `bin/blkid` pointing at `bin/toybox` would take the copy and overwrite the
  multi-call binary. The packaging step then refuses an initramfs whose `blkid`
  reports itself as a Toybox multicall binary.

### `file`

Toybox's `file` applet is switched off in the recipe and in phase 1, so
`/usr/bin/file` is the `file` port's: the reference implementation, with
`/usr/share/misc/magic.mgc` behind it.

The applet reads a handful of headers and refuses `--mime` outright.
`lesspipe` asks `file -L -s -b --mime` and nothing else; with no answer it
passes every file through unchanged, so `less` on a `.tar.gz` would show the
compressed bytes and the filter would look as though it was never installed.
Two `file` implementations on one image would also mean two answers to "what is
this file", which is the question the handler tables, the thumbnailer and the
pager all ask.

The cost: `magic.mgc` is about ten megabytes.

### `switch_root`

The initramfs carries util-linux's `/usr/sbin/switch_root`, with its `mount`,
`umount`, `losetup` and `dmesg`. The applets are compiled out, and the packaging
step refuses an initramfs whose copy of any of the five reports itself as a
Toybox multicall binary, or whose programs name a library it does not carry.

Toybox's `switch_root` empties the initramfs and calls `chroot()`. It never
performs the move-mount that makes the new root the *mount namespace's* root.
The namespace root then stays the emptied initramfs, with the real root parked
at `/newroot`, and anything that **joins** a mount namespace (entering a
container, `nsenter -m`) gets that empty root as `/`, so every path fails to
exist. Every process on the machine is also left chrooted, and the kernel
refuses to create a user namespace for a chrooted caller, so no
[box](../06-reference/glossary.md) (the rootless container an application runs
in) can start.
The machine boots perfectly either way, which is why the check exists.

The tell-tale is that `readlink /proc/<pid>/root` prints `/newroot`.
`kdos doctor` checks for it.

## rcS and the service scripts

`init` runs `/etc/init.d/rcS` as its `sysinit` entry. In order, `rcS`:

1. Sets `PATH` to `/sbin:/usr/sbin:/bin:/usr/bin:/usr/local/sbin:/usr/local/bin`.
   toybox init hands its children `/sbin:/usr/sbin:/bin:/usr/bin`, and `kdos`
   lives under `/usr/local/bin`; without the addition, a service or system timer
   that runs `kdos` would be skipped as missing.
2. Counts the enabled service scripts and tells the splash its stage total.
3. Reads `/etc/kdos/accent`, when it exists, and tells the splash to repaint in
   that accent.
4. Checks every non-root `fstab` filesystem with a pass number whose device
   exists yet (one on a logical volume the initramfs did not activate is left to
   `03_lvm`; see [Checking the root](#checking-the-root)), then mounts
   everything in `fstab`, mounts `efivarfs` on a UEFI boot, creates `/run/lock`
   with mode `1777`, brings the loopback interface up, runs `chmod 1777 /tmp`
   and `swapon -a`, and links `/etc/localtime` to UTC when nothing is there. The
   `tzdata` package does not own `/etc/localtime`, so a `tzdata` upgrade leaves
   a chosen time zone alone. A machine whose installed `tzdata` manifest still
   lists the link loses it once, on the next upgrade; see
   [Known gaps](../06-reference/known-gaps.md#build-and-packaging).
5. Makes the root mount recursively shared (`mount --make-rshared /`), which
   rootless containers need for mount propagation.
6. Runs each executable `/etc/init.d/NN_name.sh` with `start`, in numeric
   order, skipping any with a marker in `/etc/service.disabled/`. Each script's
   output goes to `/run/kdos-init.<name>.log` and is then printed to the
   console; the splash shows one stage per script and, if one fails, up to six
   lines of its output. A supervised daemon's own output does not stay in that
   file: `ksvc` sends it to syslog and to `/run/kdos-svc.<name>.log`; see
   [The daemons](../04-programs/daemons.md#the-shape-they-share).
7. Runs `kdos-bootctl mark-good`.
8. Quits the splash, which runs the power-off animation and leaves a clean
   framebuffer for the tty1 login.

The quit is synchronous (`kdos-splash quit` returns only once the splash has
stopped drawing), and the desktop depends on that twice. Init starts the tty1
login on a framebuffer nothing else owns, and the compositor's mode set further
down that chain acquires a device the splash has already released. A splash
that quit asynchronously would race the mode set, and losing that race gives a
black screen with a running session behind it.

The base filesystem ships these service scripts in `/etc/init.d/`:

```
01_udev  02_modules  03_lvm  05_hostname  10_sysctl  12_zram  15_userdirs
18_timers  20_dmesg  22_syslog  25_nftables  30_network  35_chrony  40_dbus
41_polkitd  42_modemmanager  42_networkmanager  45_avahi  45_seatd
46_hostapd  47_pcscd  50_alsa  51_mdmonitor  52_smartd  53_xfs_healer
54_thermald  55_powerd  55_tlp  56_energyd  57_oomd  58_mountd  59_packd
60_bluetooth  70_sshd  80_cups  81_cups-browsed  82_ipp-usb
```

Ports on the image install more beside them:

| Script | Installed by |
|---|---|
| `43_boltd` | `bolt` |
| `63_gssd` | `nfs-utils` |
| `65_brltty` | `brltty` |
| `72_nfsd` | `nfs-utils` |
| `73_mosquitto` | `mosquitto` |
| `74_prosody` | `prosody` |
| `76_postgresql` | `postgresql` |
| `83_samba` | `samba` |

A package installed later can add its own, so `ls /etc/init.d` on the running
machine is the complete list.

A service is disabled by a marker file named after the script without its
number and extension, rather than by editing anything:

```sh
sudo touch /etc/service.disabled/cups
```

The conventions for the scripts themselves, and the reason `ksvc` exists rather
than a shell supervisor, are in
[Administration](../02-user-guide/administration.md#services) and
[The daemons](../04-programs/daemons.md).

### Shutdown

toybox init answers `reboot`, `poweroff` and `kdos-powerd` by running the
`::shutdown` entries of `/etc/inittab` in order, each to completion, and only
then signalling every process. It never runs a service script's `stop` itself,
so the first entry is `/etc/init.d/rcK`, which takes the scripts `rcS` would run
(executable, no marker under `/etc/service.disabled`) and runs each with `stop`
in reverse order, then runs `sync`. `swapoff -a` and `umount -a -r` follow, so
every stop action still has a writable filesystem to save to; `50_alsa` storing
the mixer levels is the plainest case. A stop that fails, such as a service
that was skipped at boot answering "not running", does not end the walk.

`25_nftables` is the one script `rcK` leaves out. Its stop deletes the
firewall's `inet filter` table, and it would run after the network scripts while
the interfaces are still configured, leaving the machine on the network with
nothing filtering until power-off. The ruleset goes when the kernel does.

Each supervised service costs `ksvc` a second to stop, so a shutdown takes
about as many seconds as there are daemons running. `kdos-powerd` signals init
(`SIGUSR2` for power-off, `SIGTERM` for reboot) and then waits sixty seconds
before calling `reboot(2)` itself. That is longer than `rcK` takes to reach
`55_powerd` and end it, so the fallback fires only under an init that ignored
the signal.

### One DHCP client on the link

Two scripts can bring an interface up, and only one of them may.
`30_network` starts `dhcpcd`; `42_networkmanager` starts NetworkManager, whose
DHCP client is internal. NetworkManager never defers to a running `dhcpcd`, so
a machine that started both would lease every interface twice: two default
routes installed and withdrawn on each renewal, and two writers of
`/etc/resolv.conf`.

`30_network` therefore stands down (`[SKIP] dhcpcd: NetworkManager owns the
interfaces`) when `/usr/sbin/NetworkManager` is executable and
`/etc/service.disabled/networkmanager` is absent. The marker is half of the
test on purpose: disabling the service leaves the binary on disk, and a check
on the binary alone would leave such a machine with no DHCP client at all.
Disabling NetworkManager hands DHCP back to `dhcpcd`, which makes `dhcpcd` the
fallback for a machine that runs no connection manager, such as a server
install or a recovery boot.

Neither script owns the loopback interface. NetworkManager brings `lo` up only
when it runs, and `dhcpcd` never touches it, so `rcS` brings it up before any
service starts. Otherwise a machine on the fallback would have no `127.0.0.1` or
`::1`, and CUPS, chrony's command socket and everything else that talks to
localhost would fail.

`dhcpcd` is supervised with `-B`, which keeps it in the foreground so `ksvc`
watches the daemon itself rather than a parent that has already exited. Its
lease database is `/var/lib/dhcpcd`, which is also the home directory of the
`dhcpcd` account (uid and gid 999) that its privilege-separated children run
as; the account ships in `/etc/passwd` and its group in `/etc/group`.
`25_nftables` runs before both scripts, because a firewall loaded after an
address is configured leaves a window in which the machine is on the network
with no policy.

## Mount points that must be right

`/etc/fstab` ships with these entries (the shipped file also carries comments):

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

The kernel mounts none of the last three. Without tracefs, every tracepoint
probe (`bpftrace`, `perf trace`, `trace-cmd`) finds an empty
`/sys/kernel/tracing`; without debugfs, the kernel's debug files
(`/sys/kernel/debug/dri/`, `wakeup_sources`, what `powertop` reads) are absent;
and without bpffs, no pinned BPF object outlives the process that made it.

`efivarfs` is mounted by `rcS` and not by `fstab`, because it exists only on a
UEFI boot and a BIOS boot would fail the line. Without it,
`/sys/firmware/efi/efivars` is an empty directory: `efibootmgr` reports no EFI
variables, the installer cannot write its NVRAM boot entry or read the Secure
Boot state, fwupd sees no UEFI devices, and `kdos-bootctl` can neither arm a
`BootNext` trial (it falls back to the menu-led one) nor clear one in
`mark-good`.

`/run` is a fresh tmpfs, so `/run/lock` (and `/var/lock`, a link to it) exists
only because `rcS` creates it, with mode `1777`. minicom and picocom take their
serial-port lock files there as the user: without the directory they lock
nothing and two programs can share one serial port, and with a `0755` one
picocom refuses to start with "cannot lock".

`/tmp` must carry `mode=1777`, and the `chmod` in `rcS` is not redundant.
Mounting a tmpfs with default options gives a `0755` root-owned filesystem that
*hides* the `1777` `/tmp` in the image, so no ordinary user can write to `/tmp`
at all. Every graphical application depends on it (lock files, scratch space,
font caches), and the failure looks like "the application is slow or never
opens". A tmpfs that is already mounted ignores a mode change on remount, so
only the explicit `chmod` repairs one.

`/var/run` and `/var/lock` are symlinks into `/run`, created by phase 1 of the
build. Several libraries compile in the path `/var/run/dbus/system_bus_socket`.
If `/var/run` were a real, empty directory, every one of those clients would
fail to reach the system bus, and report it as the *service* being unreachable
while that service is running.

The installer appends to `fstab` rather than replacing it, because of the
shipped `/tmp` line.

## The console

The framebuffer console is built with deferred takeover, and the takeover
resets every virtual terminal to the kernel's built-in font. A `setfont` in
`rcS` would therefore be silently undone.

`/etc/inittab` starts every console through `kdos-getty`, which does the job at
the right moment: force the takeover, load the font and palette, check them,
then run the real getty.

| Console | `inittab` entry | What you get |
|---|---|---|
| `tty1` | `respawn`: `kdos-getty tty1 /usr/local/sbin/kdos-login tty1` | The desktop's login, automatic when `login.conf` names an account |
| `tty2` | `respawn`: `kdos-getty tty2 /sbin/getty 38400 tty2` | A plain login prompt: the recovery console |
| `ttyS0` | `askfirst`: `/bin/bash -l` | A root shell on the serial line, started only when a key is pressed there |

What `kdos-getty` does, in order:

- It logs to `/run/kdos-getty.<tty>.log`, which `kdos doctor` can read when a
  terminal comes up with the wrong font.
- It ends the deferral. Only a real glyph does that: escape sequences are
  consumed by the terminal's state machine, and even spaces are skipped by the
  render path, so it prints one character and clears it.
- The takeover is scheduled work, so it waits for the kernel to log
  `fbcon: Taking over console`, then retries the font load until
  `showconsolefont` confirms the size is 16x32.
- The font is `ter-kdos32n`, built in the `terminus-font` port: Terminus's
  512-glyph face with six spacing diacritics replaced by the double box-drawing
  glyphs the block logo needs. It keeps λ, which the shell prompt and the
  message of the day print.
- It loads the palette from `/etc/vtrgb` (the accent's sixteen console colours,
  written by `kdos-bootctl theme`) **before** the final clear, or the screen
  ends up half pure black and half the palette's black.
- It loads the keyboard layout named in `/etc/keymap`, which the installer
  writes, with `loadkeys`. The layout applies to every console at once, so the
  second getty's load changes nothing. A console keyboard with the wrong layout
  starts with that file.
- It sets the terminal's window size from the grid the font actually produced,
  read from `/dev/vcsa<n>`. Without this, full-screen programs on tty1 would
  come up believing the screen is 80x24 and draw in its top-left corner.
- When tty1's account is known (from `login.conf`, below), it moves itself into
  `/sys/fs/cgroup/user.slice/user-<uid>/session`, so the session and every box
  under it get working memory limits. A password login and an ssh session are
  not moved.
- It raises the real-time limits every process below it inherits: `rtprio` 95,
  `nice` 39 (nice −19) and `memlock` 4 MiB, which the audio stack needs to run
  its threads at real-time priority. A limit the kernel refuses is logged, not
  fatal.
- It runs the getty named on its command line.

Loading the font and palette anywhere earlier, such as in `rcS`, would be undone
by the takeover.

`kdos-login` reads `autologin` from
[`/etc/kdos/login.conf`](../06-reference/configuration.md#etckdosloginconf) and
hands the terminal to `agetty` either way:

- **With the key** (`autologin = kdos` on the live medium), it runs
  `agetty --autologin <account> --noclear tty1 38400 linux`: a machine with one
  account and no password has nothing to ask. `/bin/login` must be shadow's for
  this to work. agetty's autologin calls `login -f -- USER`, and toybox's
  `login` reads the name as `-f`'s own argument, takes `--` for the account and
  refuses it. Toybox is therefore built with `login` and `su` off; otherwise
  tty1 would be left at a login prompt nobody can answer.
- **Without it** (commented out, which is what the installer writes unless an
  answer file asked otherwise, or a missing or unreadable file), it runs plain
  `agetty` and the ordinary password prompt appears. There is no greeter: no
  account chooser, no session chooser and no graphical surface of any kind
  before the shell.

Going through agetty either way keeps utmp, lastlog and the shell profile on
the same path they take everywhere else, and the profile is what starts the
desktop: `~/.bash_profile` (and `~/.zprofile`) runs `kdos-desktop` when the
login is on `/dev/tty1`, `WAYLAND_DISPLAY` is unset and `kdos-desktop` exists.
It is not `exec`ed, so a session that fails to start falls back to a shell
prompt.

`kdos-getty` falls back to a plain `agetty` when the program named in
`/etc/inittab` cannot be run. Without that fallback, init would respawn a
failing program forever and there would be no way to log in at all. The fallback
reads the same `login.conf` key rather than a built-in account name, so it logs
in the account the missing `kdos-login` would have, and otherwise asks for a
password. A second copy of the name would log in a user a renamed installation
does not have.

`tty2` is the recovery console and stays a plain getty whatever tty1 does.
Reaching it from the desktop is the compositor's job, not the kernel's: once
`libseat` puts tty1 into graphics mode the kernel stops answering
Ctrl+Alt+F<n>, so the session forwards the chord itself: `Ctrl+Alt+F2` from
the desktop reaches tty2, and `Alt+F1` at tty2 comes back.

## The login banner

`kdos-banner` runs from `/etc/bash.bashrc` on a top-level interactive shell
(`SHLVL` 1, standard output a terminal, not inside a container). It paints the
banner one raster line at a time with a bright beam leading the fill, then one
frame of reverse video for a CRT thump. Any keypress skips the rest.

| Command | Effect |
|---|---|
| `kdos-banner` | Animate, then leave the banner on screen |
| `kdos-banner --plain` | Print it with no animation |

It prints plainly, without animating, when standard output is not a terminal,
when `TERM` is `dumb`, when `KDOS_NO_ANIM` is set, or when the banner is taller
than the terminal.

It composes the banner itself: the logo from `/usr/share/kdos/logo.txt` (or the
file named by `KDOS_BANNER_LOGO`) beside the output of `fastfetch --logo none`.
Asked to draw a logo, fastfetch prints the block, moves the cursor back up over
it and writes each line with an absolute column jump. That output is not a
sequence of raster lines, so replaying it a line at a time would drift one row
per line and draw the block twice.

The logo is generated by `genlogo.py` from the same image the boot splash draws,
so the banner, the splash and the mascot cannot drift apart.

![The login banner at the 512-glyph VT font, on the first terminal](../../screenshots/tty-banner.png)

Three constraints are built into that generator. The VT font has full blocks
and the double box characters but **no half blocks**, so one cell is one solid
block. Character cells are twice as tall as they are wide, so the sampling grid
must be about twice as wide as it is tall or the image stretches. And the
banner must stay under about thirty lines, or it scrolls off the screen.

## See also

- [Getting started](../02-user-guide/getting-started.md) — the same path from the user's side
- [Installation](../02-user-guide/installation.md) — what the installer writes to the ESP
- [Theming](../02-user-guide/theming.md) — the boot menu, splash and console colours
- [The session](session.md) — everything after the login prompt
- [The daemons](../04-programs/daemons.md) — the services `rcS` starts
- [kinstall](../04-programs/kinstall.md) — the installer, its partition plans and LVM
- [Configuration](../06-reference/configuration.md) — `fstab`, `inittab`, `login.conf` and the rest
- [Known gaps](../06-reference/known-gaps.md) — what the boot path does not do yet
