# Boot menu and kernel parameters

This page covers the [Limine]{.idx} [boot menu]{.idx} a KDOS machine shows at power-on: which entries it offers on
the live medium and on an installed machine, which file each menu is read from, how to add a
kernel parameter for one boot or for every boot, and what to check when the menu, an entry or a
parameter does not do what you expect. Why KDOS boots through Limine, where the kernel is kept and
what each parameter means to the initramfs are explained in
[Boot and init](../../kdos/03-architecture/boot-and-init.md#limine-and-the-kernel-command-line);
this page gives the procedures.

## Installation

The [boot loader]{.idx} is [limine](#port-limine), and it is the only one in the tree. Its recipe
builds the [BIOS]{.idx} stage, the BIOS and [UEFI]{.idx} El Torito images and both EFI loaders, `BOOTX64.EFI` and
`BOOTIA32.EFI`, from one source, and installs them under `/usr/share/limine`, together with the
`limine` program whose `bios-install` verb writes the BIOS boot code. Nothing runs at boot on the
running system; Limine has no service script.

Three programs write a `limine.conf`, and nothing else does:

| Writer | File | When |
|---|---|---|
| `script/phases/70_image/110_iso.sh` | `boot/limine/limine.conf` on the ISO9660 filesystem of the medium | Image build |
| `kinstall`, from [kdos-installer](#port-kdos-installer) | `limine.conf` at the root of the installed machine's EFI system partition (ESP), [`/boot/efi/limine.conf`]{.idx} on the running system | Install |
| [`kdos-bootctl`]{.idx}, from [kdos-tools](#port-kdos-tools) | The `/KDOS` entries, `default_entry` and the [theme]{.idx} keys of that same file; during a UEFI update trial, also a copy at `EFI/kdos/trial/limine.conf` | Every change of the [A/B]{.idx term="A/B slots"} boot state (`set-slot`, `try`, `deploy`, `select` in the initramfs, `mark-good` at the end of `rcS`), and every `kdos theme` |

The memory test entry is added only when [memtest86plus](#port-memtest86plus) is installed, and
the firmware boot entry labelled `KDOS` is written by the installer with
[efibootmgr](#port-efibootmgr) on a machine installed through UEFI.

## Configuration

### The live medium's menu

The medium's menu is a heredoc in the ISO step, written into the ISO tree and never into the root
filesystem:

<!-- kfs:include script/phases/70_image/110_iso.sh lines=311-339 pin=315b5e610a4f -->

`$BOOT_THEME` is the output of `kdos-bootctl theme --print` for the [accent]{.idx} the image is built in;
`$WALLPAPER` and `$FONT_LINES` name the backdrop and the converted Terminus face, when the build
found them. The memory test entry is appended after it:

<!-- kfs:include script/phases/70_image/110_iso.sh lines=353-367 pin=0c1a7fc58339 -->

The medium is ISO9660 and read-only, so this file cannot be edited on a written stick. A change
for one boot is made in Limine's editor (see [Usage](#usage)); a lasting change is a change to
`110_iso.sh` and a new image.

### The installed machine's menu

`kinstall` writes the installed menu at the root of the ESP. In each `cmdline:` line, the two `%s`
between `kdos_slot=a` and `root=` become `bootstate=UUID=<esp>` and, on an encrypted install,
`cryptdevice=UUID=<luks>:kdosroot`:

<!-- kfs:include src/system/kdos-installer/install.c lines=2261-2293 pin=49d43fc22fb4 -->

The file is `/boot/efi/limine.conf`. The ESP is mounted with `umask=0077`, as the installer's
`fstab` line says, so only root can read or change anything under `/boot/efi`.

The installed menu carries the medium's colours, backdrop (`EFI/kdos/wallpaper.png`) and face
(`EFI/kdos/font.bin`, with the `term_font` lines). The installer copies the face from
`/mnt/iso/boot/limine/font.bin` on the live medium, which stays readable because the target is
mounted beside it at `/mnt/kinstall`, or from the running system's own ESP when it is run from an
installed KDOS. Without either the menu is drawn in Limine's built-in face; the entries and colours
are unaffected.

### What kdos-bootctl rewrites

`kdos-bootctl` regenerates every entry whose title starts with `/KDOS`, and `default_entry`, from
the boot state, and writes the file only when the text differs. It builds every `/KDOS` command
line from the **first** `/KDOS` entry's, less the words it sets per entry:

<!-- kfs:include src/system/kdos-tools/bootctl.c lines=651-704 pin=5b3647ab713f -->

Every other word carries over, so a word you add to the first entry's `cmdline` reaches every
`/KDOS` entry the next time the menu is regenerated, and a word you add only to another entry is
dropped. `timeout`, `wallpaper`, `term_font`, the memory test entry and every line that is not part
of a `/KDOS` entry or a theme key are left as they are. The theme keys are listed in
[Restamping an installed machine](../../kdos/03-architecture/boot-and-init.md#restamping-an-installed-machine).

### Module options

A driver that is loaded as a module can be given its parameters by an `options` line in a file
under `/etc/modprobe.d/`, or kept from loading by a `blacklist` line. The file is read whenever the
module loads, and needs no change to the menu. The image ships one such file from `fs/`, a
blacklist; `nvdimm-security.conf` and `stlink_v1.conf` beside it come from the packages that own
those drivers' tools:

<!-- kfs:include fs/etc/modprobe.d/kdos-sdr.conf -->

The installer and `kdos-power timezone` write an `options` file, `/etc/modprobe.d/kdos-regdom.conf`,
described on [Microcode and firmware](microcode-and-firmware.md#the-regulatory-database-and-the-country).

## Usage

### Reaching the menu

1. Start the machine. The menu counts down for the number of seconds `timeout:` names.
2. Press any key during the countdown. The countdown stops and the menu waits.
3. Choose an entry with the arrow keys and press Enter.

| Live medium | Installed machine |
|---|---|
| `KDOS Live` | `KDOS` |
| `KDOS Live (clean session)`: adds `nopersist` | `KDOS (verbose)`: `loglevel=7` |
| `KDOS Live (verbose)`: `loglevel=7` | `KDOS (single user)`: `loglevel=7 single` |
| `Memory Test (memtest86+)`, UEFI only | `KDOS (slot <x>)`, when the other root slot has a root and a kernel |
| | `Memory Test (memtest86+)`, UEFI only |

The live medium's entries carry `console=tty0 console=ttyS0`. The kernel prints its messages on
both, and the last one named, the serial line, becomes `/dev/console`, where the initramfs and
`rcS` print theirs. An installed machine's entries carry `console=tty0` alone, so everything prints
on the screen. See
[the table of parameters](../../kdos/03-architecture/boot-and-init.md#command-line-parameters-kdos-reads)
for every word KDOS reads.

### Adding a parameter for one boot

This works on the live medium and on an installed machine alike, and changes no file.

1. Stop the countdown and highlight the entry.
2. Press `E`. Limine opens its editor on the entry, with the cursor on its first line,
   `comment:`.
3. Move down to the `cmdline:` line (the fifth), press `End`, and add the word, separated by a
   space. A line longer than the editor wraps, marked with arrows at both edges.
4. Press `F10` (or `Ctrl+X`) to boot the edited entry, or `Esc` to discard the edit.

The editor names these two keys at its top, as `ESC Discard and Exit` and `F10/CTRL+X Boot`. The menu itself shows no key list, because the KDOS
theme sets `interface_help_hidden: yes`.

### Adding a parameter to every boot of an installed machine

1. Open the menu as root, for example `sudo nano /boot/efi/limine.conf`.
2. Add the word to the `cmdline:` line of the **first** `/KDOS` entry. Leave `kdos_slot=`,
   `bootstate=`, `root=` and `cryptdevice=` as they are.
3. Save the file and run `sync`. The ESP is FAT and has no journal.
4. Reboot. When the boot reaches the end of `rcS`, `kdos-bootctl mark-good` regenerates the other
   `/KDOS` entries, and they carry the word as well. The regeneration puts the added word in
   front of the words it sets per entry, so `… console=tty0 quiet loglevel=3 foo=1` comes back as
   `… console=tty0 foo=1 quiet loglevel=3`.
5. Check it: `cat /proc/cmdline`.

Do not add `quiet`, `single` or a `loglevel=` there: `kdos-bootctl` removes those words from the
template and sets them per entry.

### Changing the countdown

Edit `timeout:` in `/boot/efi/limine.conf` as root. `kdos-bootctl` never rewrites it. A value of
`0` boots the default entry without drawing the menu at all, so the verbose, single-user and
memory test entries cannot be reached; Limine's `CONFIG.md` describes the key.

### Changing the colours

`kdos theme <accent>` repaints the menu along with the rest of the desktop: `kdos-powerd` runs
`kdos-bootctl theme <accent>` for it, which rewrites only the theme keys of the menu and the
console palette in `/etc/vtrgb`, and prints `text consoles are now <accent> at the next login prompt`
and `boot menu is now <accent>`. Between two accents only the seven colour lines differ
(`interface_help_colour`, `interface_help_colour_bright`, `backdrop`, `term_background`,
`term_foreground`, `term_palette`, `term_palette_bright`); run as root, the same command does the
same without the session. To see the block an accent would write without changing anything:

```bash
kdos-bootctl theme --print amber
```

See [Theming](../../kdos/02-user-guide/theming.md#the-boot-menu-the-splash-and-the-text-consoles)
for what can be changed and what cannot.

## Tips

- `cat /proc/cmdline` prints the command line the running kernel was given, which is the quickest
  way to tell which entry booted and whether an edit took.
- `default_entry` is regenerated by `kdos-bootctl` to point at the first `/KDOS` entry, so a hand
  edit of it lasts only until the next boot that reaches `mark-good`.
- On the live medium, the clean-session entry is the way past a persistence store that stops the
  desktop; see [Live persistence](live-persistence.md).
- `emergency` has no menu entry. Add it in the editor for one boot; see
  [Recovering an unbootable system](recovering-an-unbootable-system.md).
- The menu shows a memory test only on UEFI, because the payload is an EFI program and the entry
  carries `if_fw_type: UEFI`.
- The initramfs reads no `init=` word: it always hands over to `/sbin/init` on the root.

## Troubleshooting

**The firmware finds nothing to boot.** On UEFI, check that Secure Boot is off in the firmware
setup: Limine's EFI loader is unsigned. Then, from a working system or the live medium, check that
`EFI/BOOT/BOOTX64.EFI` exists on the ESP and that `efibootmgr` lists a `KDOS` entry, which the
installer puts first in `BootOrder`, its line ending in `KDOS HD(1,GPT,…)/\EFI\BOOT\BOOTX64.EFI`.
On BIOS, the boot code is written by `limine bios-install <disk> 2`, which puts Limine's stage into
partition 2, the 1 MiB BIOS boot partition of the installer's layout, and the second stage,
`limine-bios.sys`, is found by name at the root of the ESP, where the installer puts it; restoring
both is a step of
[Recovering an unbootable system](recovering-an-unbootable-system.md#putting-the-boot-loader-back).
`sudo sfdisk -d <disk>` lists that partition with
`type=21686148-6449-6E6F-744E-656564454649`; a GPT disk without one starts only on UEFI firmware.

**The menu appears in Limine's own font and colours, with no backdrop.** Limine falls back to its
own terminal when the theme leaves it fewer rows than it needs. Check `term_margin` against the
ceiling in [The margin ceiling](../../kdos/03-architecture/boot-and-init.md#the-margin-ceiling);
`kdos theme <accent>` writes the shipped value back. A missing `EFI/kdos/font.bin` or
`EFI/kdos/wallpaper.png` changes only the face or the backdrop, never the entries.

**The menu shows only `Limine <version> (x86-64, UEFI)` in cyan and `[config file contains no
valid entries]`.** `limine.conf` is missing or empty, which is what an ESP left unflushed by a
power cut can show after `rcS`'s `fsck.fat -a` has repaired it. Write the entries back from the
live medium; see
[Putting the boot loader back](recovering-an-unbootable-system.md#putting-the-boot-loader-back).

**The menu is gone: the machine boots straight into KDOS.** `timeout:` is `0`. Set it back as
root in `/boot/efi/limine.conf`.

**A parameter added to the file is missing from `/proc/cmdline`.** It was added to an entry other
than the first `/KDOS` entry, or it is one of the words `kdos-bootctl` sets per entry, and the
regeneration at the end of the last boot replaced it. Add it to the first `/KDOS` entry.

**The menu shows `KDOS (slot b)`, or its comment speaks of an update.** An A/B trial is in
progress, or slot B exists. See [A/B slots and rollback](ab-slots-and-rollback.md).

**`kdos-bootctl theme` reports no bootloader configuration and an unchanged boot menu.** It printed
`no bootloader configuration at /boot/efi/limine.conf — boot menu unchanged` (and still exits 0,
after repainting the consoles): it found no `limine.conf` there, which is the case on the live
medium and on a machine whose ESP is not mounted. Check `findmnt /boot/efi`.

## Known gaps

- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported)
- [32-bit UEFI is built and has never been booted](../../kdos/06-reference/known-gaps.md#32-bit-uefi-is-built-and-has-never-been-booted)
- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only),
  which covers the menu `kdos-bootctl` regenerates

## See also

- [Limine and the kernel command line](../../kdos/03-architecture/boot-and-init.md#limine-and-the-kernel-command-line):
  where the kernel is kept, the menu, every parameter KDOS reads
- [The boot menu](../../kdos/02-user-guide/installation.md#the-boot-menu): the installed menu,
  from the installer's side
- [kdos-bootctl](../../kdos/04-programs/kdos-command.md#kdos-bootctl): every verb
- [Theming](../../kdos/02-user-guide/theming.md#the-boot-menu-the-splash-and-the-text-consoles)
- `ab-slots-and-rollback.md`: [A/B slots and rollback](ab-slots-and-rollback.md)
- `initramfs-and-rescue.md`: [The initramfs and the rescue shell](initramfs-and-rescue.md)
- `recovering-an-unbootable-system.md`: [Recovering an unbootable system](recovering-an-unbootable-system.md)
- `modules-and-sysctl.md`: [Modules and sysctl](modules-and-sysctl.md)
- `init-and-runlevels.md`: [Init and runlevels](init-and-runlevels.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 369.* Previous: [368. Users and groups](users-and-groups.md) · [Contents](../README.md) · Next: [370. The initramfs and the rescue shell](initramfs-and-rescue.md)
