# The initramfs and the rescue shell

The [initramfs]{.idx} is the archive the kernel unpacks into memory and runs first; its [`init`]{.idx}
finds, unlocks and mounts the root and hands over to it. This page covers where a KDOS machine's
initramfs comes from, how to look inside one, what an installed machine does when a new kernel
arrives, and how to use the [rescue shell]{.idx} the initramfs opens when it cannot finish. What
`init` does at each stage, and why each program in it must be the one it is, is explained in
[The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs); this page gives the
procedures.

## Installation

The initramfs is not a package. `script/phases/70_image/090_initramfs.sh` generates it in the
image phase (see [Part XV — The image](../15-image/intro.md)) from programs other ports installed,
and `110_iso.sh` copies the result to the medium's `boot/` and to [`/boot/initramfs.cpio.gz`]{.idx} in the
image, so every installed system carries it. No package owns that file.

What it carries comes from these ports:

| Port | In the initramfs |
|---|---|
| [bash](#port-bash), [coreutils](#port-coreutils), [grep](#port-grep), [sed](#port-sed) | The shell `init` is written in, and the tools it and the rescue shell run |
| [util-linux](#port-util-linux) | [`blkid`]{.idx}, [`mount`]{.idx}, [`umount`]{.idx}, `losetup`, `switch_root`, `dmesg`, `setsid` |
| [eudev](#port-eudev), [kmod](#port-kmod) | `udevd`, `udevadm`, `modprobe` and the udev rules whose programs the initramfs carries |
| [linux](#port-linux) | The modules in the build's list |
| [e2fsprogs](#port-e2fsprogs) | [`e2fsck`]{.idx}, for the check an ext root gets before it is mounted |
| [mdadm](#port-mdadm), [cryptsetup](#port-cryptsetup), [lvm2](#port-lvm2), [thin-provisioning-tools](#port-thin-provisioning-tools) | RAID assembly, the LUKS unlock and volume-group activation, each when installed |
| [kdos-tools](#port-kdos-tools) | `kdos-bootctl`, for [A/B slot]{.idx term="A/B slots"} selection |
| [kdos-splash](#port-kdos-splash) | The boot splash |
| [intel-ucode](#port-intel-ucode), [linux-firmware](#port-linux-firmware) | The [microcode]{.idx}, in a separate archive in front; see [Microcode and firmware](microcode-and-firmware.md) |

On an installed machine, the [linux](#port-linux) package's postinstall builds a second file for
every kernel it installs, [`/boot/initramfs-kdos.cpio.gz`]{.idx}: the image's initramfs with one more
archive appended, holding that kernel's copies of the modules. `kdos-bootctl deploy` and the
installer copy that file to the ESP when it exists, and the image's own file otherwise.

## Configuration

Nothing about the initramfs is configured on a running machine. It is read-only, and there is no
`mkinitcpio.conf` or `dracut.conf` to edit. Its behaviour is set in the tree, by the image's build
script and the `linux` postinstall, and by one file on the machine.

The module list, in the build script, decides which disks and filesystems can hold a root:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=446-492 pin=e85bf30992e0 -->

The last line writes the same list to the image as [`/boot/initramfs.modules`]{.idx}. That file is the one
setting on the machine: the [`linux`]{.idx} postinstall reads it to decide which modules to append for a
new kernel. A module named there is carried for the next kernel installed into that root.

The command-line words the generated `init` reads are below (`\$` here is the generator's
escaping: the init is written by a heredoc, and the generated script has a plain `$`). The live
path checks for `nopersist` separately, to skip the persistence store. Every word is explained in
[Command-line parameters KDOS reads](../../kdos/03-architecture/boot-and-init.md#command-line-parameters-kdos-reads):

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=632-656 pin=0cddbb0c0606 -->

The rescue helper, which every failure ends in:

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=556-578 pin=17c2e4e0f539 -->

The new kernel's initramfs and its deploy, in the `linux` postinstall:

<!-- kfs:include ports/core/boot/linux/postinstall.sh lines=153-182 pin=ebd732b12a82 -->

## Usage

### Finding the initramfs a machine boots

| Machine | File |
|---|---|
| Live medium | `boot/initramfs.cpio.gz` on the medium, which the running live system sees as `/mnt/iso/boot/initramfs.cpio.gz` |
| Installed, booted | `/boot/efi/EFI/kdos/<slot>/initramfs.cpio.gz`, where `<slot>` is the `kdos_slot=` in `/proc/cmdline` |
| Installed, in the root | `/boot/initramfs.cpio.gz` (the image's) and, after a kernel update, `/boot/initramfs-kdos.cpio.gz`; the kernel beside them is `/boot/vmlinuz-kdos` |

Until a kernel update, the file in slot A's directory is a byte-for-byte copy of the root's
`/boot/initramfs.cpio.gz`, and the live medium's is the same file again.

### Looking inside

The image's file is the uncompressed microcode archive followed by one gzip-compressed archive; an
`initramfs-kdos.cpio.gz` has a second gzip-compressed archive after that. GNU `cpio`
(the [cpio](#port-cpio) port) stops at the end of the first archive, so on a machine with microcode
it lists only that archive. The ESP is mounted readable by root alone, so let `cpio` open the file
itself with `-F` rather than redirecting it from your own shell:

```bash
sudo cpio -t -F /boot/efi/EFI/kdos/a/initramfs.cpio.gz
```

On the shipped image that first listing is the `kernel/x86/microcode/` directory with
`AuthenticAMD.bin` and `GenuineIntel.bin` in it, and the count line reads `35184 blocks`.

To list the rest:

1. Note the `N blocks` line `cpio -t` prints on standard error for the first archive.
2. Skip that many 512-byte blocks and decompress what follows:

   ```bash
   sudo dd if=/boot/efi/EFI/kdos/a/initramfs.cpio.gz bs=512 skip=N | zcat | cpio -t
   ```

On a file with no microcode archive in front, the first `cpio -t` fails on the gzip header, and
`sudo zcat <file> | cpio -t` lists the archive directly.

Either listing stops at the end of the image's archive. In an `initramfs-kdos.cpio.gz`, the
modules the `linux` postinstall appended follow it as one more archive, which `cpio -t` does not
reach.

### A new kernel on an installed machine

1. Install or update the `linux` package, through `kdos update apply` or `kpkg`.
2. Its postinstall builds `/boot/initramfs-kdos.cpio.gz` in the root it installed into. When that
   root is the running system and `/boot/efi` is mounted, it runs `kdos-bootctl deploy /`, and the
   next boot uses the new pair.
3. When it installed into another root, the A/B update deploys after every package is in; see
   [A/B slots and rollback](ab-slots-and-rollback.md).
4. Check what each slot boots: `sudo kdos-bootctl status` prints the kernel version per slot.

### Using the rescue shell

When `init` cannot finish, the splash shows the reason and quits, and a shell opens on
`/dev/console` with the reason printed above it. On an installed machine `/dev/console` is `tty1`,
the screen. When it is the serial line, as on the live entries or on any command line whose last
`console=` is `ttyS0`, a second shell opens on `tty1` with the same reason, and the serial line
prints `A rescue shell is also open on tty1.` above its own prompt.

1. Read the reason. The table in [Troubleshooting](#troubleshooting) says what each one means.
2. Look around. `PATH` is `/bin`. `blkid`, `mount`, `umount`, `dmesg`, `modprobe` and the
   coreutils `init` uses (`ls`, `cat`, `cp`, `dd`, `mkdir` and the like) are always there;
   `cryptsetup`, `e2fsck`, `mdadm` and `lvm` are there when the image carried them, as the shipped
   image does. There is no editor, no `tty` and no network tool. The shell on `/dev/console` has
   no controlling terminal, so it has no job control and says so when it starts; `Ctrl+C` does not
   reach a program it runs.
3. Repair what you can: run `e2fsck` on an unmounted ext root, open a container with
   `cryptsetup open`, assemble an array with `mdadm --assemble --scan`.
4. Restart the machine: `sync; echo b > /proc/sysrq-trigger`. The initramfs carries no `reboot`.

Do not leave the shell on `/dev/console` with `exit`: it is process 1, and the kernel panics with
`Attempted to kill init`. On an installed machine that is the shell on `tty1`; on the live medium,
the one on the serial line.

## Tips

- `blkid` with no arguments lists every filesystem the initramfs can see, with its UUID; compare
  it with `root=` and `cryptdevice=` in `cat /proc/cmdline`.
- On an installed machine, the `KDOS (verbose)` entry prints every kernel message, which shows a
  missing driver before the splash reaches the stage that needs it.
- The init, the programs and the microcode in the image's initramfs are the medium's. A kernel
  update, through `kdos update` or `kpkg`, runs the `linux` postinstall, which appends modules and
  never rebuilds that base, so the copy in a root is the one it was installed or copied from.
- `kdos doctor` warns when the mount namespace's root is not `/`, the sign of a `switch_root`
  that only chrooted; see [`switch_root`](../../kdos/03-architecture/boot-and-init.md#switch_root).

## Troubleshooting

| What the screen says | Check | Fix |
|---|---|---|
| `Error: Root device with UUID=… not found!`, then `Disk boot failed: no root at UUID=… with /sbin/init on it` | `blkid` in the rescue shell: is the UUID there, and on which device? | A disk that is absent from `blkid` needs a driver the initramfs does not carry; add its module name to `/boot/initramfs.modules` in that root, and the next kernel the `linux` package installs there carries it. A changed UUID: record it with `kdos-bootctl set-slot <slot> <fs-uuid> [<luks-uuid>]`, where `<slot>` is `a` or `b`, and the menu's `root=` is regenerated from the boot state. On an encrypted slot give the container's UUID too: `set-slot` clears it when it is left out. On a machine with no boot state, correct `root=` in the `/KDOS` entries of `limine.conf` |
| `Error: /sbin/init not found on root device!`, then the same `Disk boot failed` line | `mount` the root by hand in the rescue shell and look at `/sbin/init` | The root is not a KDOS root, or is damaged; see [Recovering an unbootable system](recovering-an-unbootable-system.md) |
| `cryptdevice: no device for …` or `Failed to unlock …` | `blkid -t TYPE=crypto_LUKS` | A wrong container UUID on the command line or in the boot state. Repeated wrong passphrases end here too |
| `ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` on the splash, and the boot goes on | `e2fsck -p` left errors on an ext root | Run `e2fsck -f` on the root while it is not mounted, from the live medium |
| `Failed to find KDOS installation media.` (live) | Is the stick still in? Did it enumerate within the scan's ten seconds? | Try another port, or the `KDOS Live (verbose)` entry, and read the serial line |
| `Failed to find KDOS installation media.`, with `Cannot release /mnt/iso after <dev> — stopping the scan` before it on the serial line (live) | The scan's one mount point stayed busy | Reboot; report the device |
| `Failed to set up a loop device for system.sfs`, `Failed to mount system.sfs` or `Error: /sbin/init not found in new root!` (live) | `dmesg` in the rescue shell, for read errors on the medium | The medium was found but its system image cannot be read: write the ISO to the stick again, or use another stick |
| `Attempted to kill init` | The kernel panicked | After leaving the rescue shell, that was the shell exiting. Otherwise `init` itself died: boot the `KDOS (verbose)` entry and read the messages above the panic |

The stage the splash stopped on narrows it down; see
[When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops).

## Known gaps

- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only),
  which includes the initramfs the `linux` postinstall appends to
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [A disk-encryption key is typed, not sealed in the TPM](../../kdos/06-reference/known-gaps.md#a-disk-encryption-key-is-typed-not-sealed-in-the-tpm)

## See also

- [The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs) and
  [The rescue shell](../../kdos/03-architecture/boot-and-init.md#the-rescue-shell)
- [A new kernel](../../kdos/03-architecture/boot-and-init.md#a-new-kernel)
- [Unlocking an encrypted root](../../kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root),
  [Finding the root](../../kdos/03-architecture/boot-and-init.md#finding-the-root),
  [Checking the root](../../kdos/03-architecture/boot-and-init.md#checking-the-root)
- [Microcode and firmware](microcode-and-firmware.md): the microcode archive in front of the
  initramfs, and checking that it loaded
- [A/B slots and rollback](ab-slots-and-rollback.md): the slot the initramfs selects, and rolling
  an update back
- [Recovering an unbootable system](recovering-an-unbootable-system.md): every way into a broken
  machine, of which the rescue shell is one
- [Encrypted root](encrypted-root.md): unlocking at boot, and opening the root by hand
- [LVM and RAID](lvm-and-raid.md): volume groups and arrays the root can live on

<!-- book-nav -->
---

*Part XVI. System administration, chapter 370.* Previous: [369. Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md) · [Contents](../README.md) · Next: [371. Microcode and firmware](microcode-and-firmware.md)
