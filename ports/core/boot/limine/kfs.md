## About

Limine is the [boot loader]{.idx}, and the only one KDOS has. One build serves both firmwares: the
same menu and the same `limine.conf` start the system on a BIOS machine and on a UEFI one, so what
a machine shows at power-on does not depend on how it started. The port installs Limine's boot
files in `/usr/share/limine` and the `limine` host program, which writes the BIOS boot code into a
disk's first sector and, on a GPT disk, Limine's stage into a BIOS boot partition (`limine
bios-install <disk> [<partition number>]`; `kinstall` lays out partition 2 for it).

Two programs read `/usr/share/limine`. `script/phases/70_image/110_iso.sh` copies the boot files
onto the ISO and writes its menu, and `kinstall` copies them onto an installed disk's EFI system
partition. How the boot works on each firmware is in
[Limine and the kernel command line](../../../../docs/kdos/03-architecture/boot-and-init.md#limine-and-the-kernel-command-line),
and what the installer writes in
[What the bootloader step writes](../../../../docs/kdos/04-programs/kinstall.md#what-the-bootloader-step-writes).

Limine is freestanding code that runs under the firmware with no C library. Its configure uses
`clang`, `ld.lld` and the `llvm-` binary tools for that code by default, whatever the host
compiler is, and assembles its BIOS and UEFI entry code with `nasm`; that is why
[clang](#port-clang), [llvm](#port-llvm), [lld](#port-lld) and [nasm](#port-nasm) are in
`depends`.

## Flags

- `--enable-bios` — Builds the BIOS port: `limine-bios.sys`, the second stage the BIOS boot code
  finds by name on a volume and loads, and that boot code itself, which is compiled into the
  `limine` program for `limine bios-install` to write. It also turns on, by default, the BIOS CD
  image and the PXE image, `limine-bios-pxe.bin`, which is installed and which nothing in KDOS
  uses.
- `--enable-bios-cd` — Builds `limine-bios-cd.bin`, the El Torito image a BIOS boots the ISO
  from.
- `--enable-uefi-x86-64` — Builds `BOOTX64.EFI`, the loader 64-bit UEFI firmware runs.
- `--enable-uefi-ia32` — Builds `BOOTIA32.EFI`, the loader for 32-bit UEFI firmware on a 64-bit
  processor, as on early Atom tablets. The ISO and an installed disk carry it beside `BOOTX64.EFI`
  in `EFI/BOOT/`, and the firmware runs whichever of the two it can execute.
- `--enable-uefi-cd` — Builds `limine-uefi-cd.bin`, the UEFI El Torito image of the ISO, a small
  FAT image holding the EFI loaders that were built. configure needs `mcopy` to make it, which is
  why [mtools](#port-mtools) is in `depends`.

## Notes

The menu is not this port's: `110_iso.sh` writes the ISO's `limine.conf`, its font and its
wallpaper, and `kinstall` writes an installed disk's. `110_iso.sh` copies `limine-bios.sys`,
`limine-bios-cd.bin`, `limine-uefi-cd.bin`, `BOOTX64.EFI` and `BOOTIA32.EFI` by name under
`set -e`, and every target is off unless a switch turns it on, so a build that drops one of the
UEFI switches leaves a file missing from `/usr/share/limine` and the image phase stops. The two
BIOS switches cover for each other: configure turns the CD image on by default when the BIOS port
is on, and the BIOS port on when the CD image is asked for, so only dropping both loses
`limine-bios.sys` and `limine-bios-cd.bin`.
