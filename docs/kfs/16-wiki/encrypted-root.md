# Encrypted root

An installed KDOS machine can keep its root filesystem inside a [LUKS]{.idx} [container]{.idx term="LUKS container"}, opened at
every boot with a passphrase typed at the boot splash. This page shows which pieces do that, the
files that carry the container's identity from the [installer]{.idx} to the boot, and the tasks that come
after an install: unlocking, adding and changing passphrases, keeping a copy of the header, and
opening the root by hand from the live medium. How the [initramfs]{.idx} finds, unlocks and mounts the
root, and why the prompt is drawn the way it is, is in the KDOS book's
[Unlocking an encrypted root](../../kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root);
this page does not repeat it.

## Installation

[cryptsetup](#port-cryptsetup) provides [`cryptsetup`]{.idx} and `libcryptsetup`, through the kernel's
device mapper. It is on the live medium as well as on an installed system, because three programs
use it:

- The installer, [kdos-installer](#port-kdos-installer) (built in
  [130_kinstall](../03-bootstrap/130-kinstall.md)), creates the container and opens it while it
  installs.
- The initramfs carries its own copy of `cryptsetup` and its libraries, put there by
  `script/phases/70_image/090_initramfs.sh`; see [The initramfs](../15-image/090-initramfs.md). An
  image built without it prints `Note: cryptsetup not installed — the initramfs cannot unlock a LUKS
  root` during that step.
- [kdos-mountd](#port-kdos-mountd) runs it to unlock an encrypted removable disk for the desktop;
  that task is on [Storage and removable media](storage-and-removable-media.md).

The kernel side is the `dm-crypt` and `dm-mod` modules of the [linux](#port-linux) port, which the
initramfs carries and loads.

No service script is involved. The root is unlocked by the initramfs before `/sbin/init` runs, and
nothing after [`switch_root`]{.idx} touches the container again; there is no `/etc/crypttab` and nothing
reads one. The live medium has no [encrypted root]{.idx} and never shows the prompt.

Two other ports encrypt at a different level. [gocryptfs](#port-gocryptfs) encrypts a directory
file by file, in user space through FUSE. [tpm2-tools](#port-tpm2-tools) and
[tpm2-tss](#port-tpm2-tss) are installed, but nothing in the boot path uses them; see
[Known gaps](#known-gaps).

## Configuration

Nothing is configured by hand. The installer writes the container's identity into two places on
the EFI system [partition]{.idx}, and the initramfs reads both.

**The kernel command line**, in the boot entries of `/boot/efi/limine.conf`. The installer adds a
`cryptdevice=` option naming the container beside the `root=` option naming the filesystem inside
it, from `src/system/kdos-installer/install.c`:

<!-- kfs:include src/system/kdos-installer/install.c lines=2123-2141 pin=e0a09f9a5198 -->

The two UUIDs are different. `cryptdevice=UUID=<container>:kdosroot` is the LUKS container on the
partition, and `root=UUID=<filesystem>` is the filesystem that appears only once the container is
open as `/dev/mapper/kdosroot`. The shape of the whole entry is in
[What the bootloader step writes](../../kdos/04-programs/kinstall.md#what-the-bootloader-step-writes),
and how to change a kernel option is on
[Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md).

**The boot state**, `/boot/efi/EFI/kdos/bootstate`, records the container again as `crypt_a`, beside
slot A's filesystem, from the same file:

<!-- kfs:include src/system/kdos-installer/install.c lines=2373-2386 pin=ab8cb81addd9 -->

The initramfs asks `kdos-bootctl crypt` for the container of the slot it chose, and uses
`cryptdevice=` only when the slot records none; see
[A slot knows its own container](../../kdos/03-architecture/boot-and-init.md#a-slot-knows-its-own-container).

**The unlock in the initramfs.** The generated init runs this block when a container has been
named, by the chosen slot's `crypt_` entry or by `cryptdevice=`, from
`script/phases/70_image/090_initramfs.sh` (the `\$` is the escape of the heredoc the init is
written from):

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=866-881 pin=093a88f36da4 -->

`unlock_root`, the function it calls, is walked through on
[The initramfs init](../15-image/091-initramfs-init.md#the-unlock-function).

**`/etc/fstab`** names the root by the UUID of the filesystem inside the container, never by the
container; see [Filesystems and fstab](filesystems-and-fstab.md).

## Usage

### Encrypting the root at install

Encryption is chosen on the installer's Layout page and only on the erase plan, which destroys
everything on the disk. The installer does not encrypt an existing installation in place.

1. Start the installer from the live medium and go to the Layout page.
2. Choose the erase plan and tick `Encrypt the root filesystem (LUKS2)`.
3. Type the passphrase in both fields. They must match.
4. Optionally tick `Put the root on LVM` as well; the volume group is then made inside the
   container, so one passphrase opens it; see [LVM and RAID](lvm-and-raid.md).
5. Check the Summary page and install. The Summary names the encryption only when LVM is on, in
   its `root` line as `LVM inside LUKS2`; without LVM it shows no encryption line.

The installer runs `cryptsetup luksFormat --type luks2` on the root partition and opens it as
`/dev/mapper/kdosroot`, with the passphrase on standard input both times; see
[An encrypted root](../../kdos/04-programs/kinstall.md#an-encrypted-root). The tick stays set if
you then switch to the reuse plan or to *Partition it myself*, and the root you choose there is
encrypted. The reuse plan shows no checkbox and the Summary no encryption line, so untick it on the
erase plan before switching; see [Encryption](../../kdos/02-user-guide/installation.md#encryption).

### Unlocking at boot

How far this path has been tested is recorded in
[Status](../../kdos/06-reference/status.md#boot-and-system).

1. Choose the boot entry. The splash shows `PASSPHRASE FOR kdosroot (attempt 1 of 3)`.
2. Type the passphrase and press Enter. Nothing is drawn as you type, not even asterisks.
3. The splash shows `UNLOCKED` and the boot goes on, or `WRONG PASSPHRASE` and asks again.
4. After three wrong attempts the boot stops in a shell; see [Troubleshooting](#troubleshooting).

### Finding the container

The container is the partition `cryptdevice=` names. On the running system:

```bash
cat /proc/cmdline
sudo cryptsetup status kdosroot
sudo blkid -t TYPE=crypto_LUKS
```

`cryptsetup status` prints the partition under `device:`, beside the container's type and cipher:

```text
/dev/mapper/kdosroot is active.
  type:    LUKS2
  cipher:  aes-xts-plain64
  keysize: 512 [bits]
  key location: keyring
  device:  /dev/nvme0n1p2
```

The commands below write it as `/dev/<part>`; replace it with that partition.

### Adding a second passphrase

The installer makes one passphrase and no recovery key. A LUKS2 container has several keyslots,
and a second passphrase kept somewhere safe serves as one.

1. Add a keyslot. cryptsetup asks for an existing passphrase, then the new one twice:

   ```bash
   sudo cryptsetup luksAddKey /dev/<part>
   ```

2. List the keyslots in use:

   ```bash
   sudo cryptsetup luksDump /dev/<part>
   ```

   Its `Keyslots:` section has one numbered entry per passphrase, `0: luks2` and now `1: luks2`.

Either passphrase opens the root at the boot prompt: the initramfs hands what you type to
`cryptsetup open`, which tries it against every keyslot.

### Changing a passphrase

```bash
sudo cryptsetup luksChangeKey /dev/<part>
```

It asks for the passphrase to replace, then the new one twice. `sudo cryptsetup luksRemoveKey
/dev/<part>` removes a passphrase altogether; never remove the last one.

### Keeping a copy of the header

A damaged LUKS header makes the whole container unreadable, whatever the passphrase. Keep a copy
off the machine:

```bash
sudo cryptsetup luksHeaderBackup /dev/<part> --header-backup-file kdosroot-header.img
```

The file is the size of the container's header area, 16 MiB for the installer's LUKS2 container,
and is made readable by root alone. `sudo cryptsetup luksHeaderRestore /dev/<part>
--header-backup-file kdosroot-header.img` writes it back.
A header backup opens the container with any passphrase that was valid when the copy was taken,
even one removed since: `sudo cryptsetup open --test-passphrase --header kdosroot-header.img
/dev/<part>` accepts it. Keep the file as safe as the passphrase itself (`cryptsetup-luksHeaderBackup(8)`).

### Opening the root from the live medium

To repair or copy an installed system that will not boot:

1. Boot the KDOS live medium on the machine.
2. Open the container:

   ```bash
   sudo cryptsetup open /dev/<part> kdosroot
   ```

3. If the install used [LVM]{.idx}, activate the volume group inside the container. The live boot
   activated its volume groups (`/etc/init.d/03_lvm.sh`) before the container was open, so it did
   not see this one:

   ```bash
   sudo vgchange -aay
   ```

   The root is then `/dev/kdos/root_a`; without LVM it is `/dev/mapper/kdosroot`.

4. Check an ext4 root before mounting it, then mount it on a directory of its own. `/mnt` itself
   would hide the live session's boot medium at `/mnt/iso` and its persistence store at
   `/mnt/persist`. With LVM, use `/dev/kdos/root_a` in place of `/dev/mapper/kdosroot`:

   ```bash
   sudo e2fsck -f /dev/mapper/kdosroot
   sudo mkdir -p /mnt/target
   sudo mount /dev/mapper/kdosroot /mnt/target
   ```

   [Repairing from the live medium](recovering-an-unbootable-system.md#repairing-from-the-live-medium) goes on from here: the
   logs to read, and how to reach the installed system's EFI system partition.

5. When done, undo the steps in reverse:

   ```bash
   sudo umount /mnt/target
   sudo vgchange -an kdos          # only with LVM
   sudo cryptsetup close kdosroot
   ```

### An encrypted second slot

A second root slot in its own container is recorded with both UUIDs, the filesystem's and the
container's: `sudo kdos-bootctl set-slot b <filesystem-uuid> <luks-uuid>`. The steps for making the
slot are on [A/B slots and rollback](ab-slots-and-rollback.md). What is known about booting one is
in [Nothing creates or first fills the second root slot](../../kdos/06-reference/known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot).

## Tips

- The initramfs loads no keyboard layout; `090_initramfs.sh` carries no `loadkeys`, and the layout
  in `/etc/keymap` is applied only after the root is mounted. The prompt therefore reads keys in
  the kernel's built-in layout, US English. A passphrase that was typed on another layout at
  install time has to be typed by the US positions of its keys; a passphrase whose characters sit
  on the same keys in both layouts, added with `luksAddKey`, avoids the question.
- Never pass a passphrase as a command-line argument. Any process can read another's arguments
  through `/proc/<pid>/cmdline`, which is why every KDOS program that feeds `cryptsetup` uses
  standard input. Typed at cryptsetup's own prompt, as in the procedures above, it is not exposed.
- Encryption protects the disk while the machine is off. Once the root is open, every process
  that can read a file reads it in the clear; see
  [What is not protected](../../kdos/03-architecture/security-model.md#what-is-not-protected).
- For a directory that is synchronised or backed up as files, gocryptfs keeps one encrypted file
  per plain file, so a changed file changes one file in the copy, not a whole container image.
- `kdos why cryptsetup` prints the reasons recorded against the port, including why the prompt is
  read from `tty1`.

## Troubleshooting

When the initramfs cannot finish, it puts the reason on the screen and opens a shell on `tty1`;
see [The rescue shell](../../kdos/03-architecture/boot-and-init.md#the-rescue-shell). On an
installed system, whose entries carry `console=tty0`, that shell is process 1, and leaving it
panics the kernel. The initramfs has no `reboot`; restart with:

```bash
sync; echo b > /proc/sysrq-trigger
```

**`Failed to unlock UUID=…:kdosroot` after three `WRONG PASSPHRASE`.** The container rejected
every attempt. Besides a mistyped passphrase, the keyboard layout can cause this (see
[Tips](#tips)). Restart and type the passphrase by its US key positions, or open the root from the
live medium, whose layout you can set, and add a passphrase that types the same on both.

**`cryptdevice: no device for UUID=…:kdosroot`.** No partition carries the container UUID the boot
asked for. In the [rescue shell]{.idx}, compare the two:

```bash
cat /proc/cmdline
blkid
```

A disk that is not connected, or a container recreated since the install, gives this. Boot the
live medium, mount the installed machine's EFI system partition on a directory of its own with
`sudo mkdir -p /mnt/esp` and `sudo mount /dev/<esp> /mnt/esp`, and correct the `cryptdevice=`
option in the entries of `/mnt/esp/limine.conf`, and `crypt_a` in `/mnt/esp/EFI/kdos/bootstate`,
to the UUID that `sudo blkid -t TYPE=crypto_LUKS` reports. Unmount it with `sudo umount /mnt/esp`.

**`cryptdevice: this initramfs has no cryptsetup`.** The image was built without cryptsetup, so its
initramfs cannot open a container. The step's `Note:` line is in the `70_image` build log; build
the image with the [cryptsetup](#port-cryptsetup) port installed.

**`cryptdevice: cannot parse '…'`.** The option is neither `UUID=<uuid>:<name>` nor
`/dev/<node>:<name>`. Correct it in `limine.conf` from the live medium, as above.

**`UNLOCKED`, then the boot stops at `ROOT DEVICE` with `Disk boot failed: no root at UUID=… with
/sbin/init on it`.** The container opened but no filesystem with the `root=` UUID appeared inside
it. In the rescue shell, `blkid` lists
`/dev/mapper/kdosroot` with the UUID it carries. On an LVM install the splash shows whether the
`VOLUME GROUPS` stage after the unlock failed; see [LVM and RAID](lvm-and-raid.md).

**`ROOT FILESYSTEM HAS ERRORS - RUN e2fsck`.** The root opened and its check left errors. Repair it
from the live medium as in [Opening the root from the live medium](#opening-the-root-from-the-live-medium),
running `e2fsck -f` before you mount.

## Known gaps

- [A disk-encryption key is typed, not sealed in the TPM](../../kdos/06-reference/known-gaps.md#a-disk-encryption-key-is-typed-not-sealed-in-the-tpm)
- [Nothing creates or first fills the second root slot](../../kdos/06-reference/known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot),
  and what it means for an encrypted second slot
- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported)
- [An unattended install skips every check the pages make](../../kdos/06-reference/known-gaps.md#an-unattended-install-skips-every-check-the-pages-make),
  among them that the two passphrases match
- [Security](../../kdos/06-reference/known-gaps.md#security): disk encryption protects data at rest
  only
- [Status](../../kdos/06-reference/status.md#boot-and-system): how far the unlock path has been
  tested

## See also

- [Unlocking an encrypted root](../../kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root): the prompt, the two UUIDs and the order of the boot
- [A slot knows its own container](../../kdos/03-architecture/boot-and-init.md#a-slot-knows-its-own-container): why the container is recorded per slot
- [Encrypting the root](../../kdos/02-user-guide/installation.md#encrypting-the-root): the installer's side, in the user guide
- [An encrypted root](../../kdos/04-programs/kinstall.md#an-encrypted-root): the commands the installer runs
- [Encrypted volumes](../../kdos/04-programs/daemons.md#encrypted-volumes): unlocking a removable disk through `kdos-mountd`
- [The initramfs init](../15-image/091-initramfs-init.md): every stage of the boot before `switch_root`
- [LVM and RAID](lvm-and-raid.md), [Filesystems and fstab](filesystems-and-fstab.md), [A/B slots and rollback](ab-slots-and-rollback.md), [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md), [Backups](backups.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 376.* Previous: [375. Storage and removable media](storage-and-removable-media.md) · [Contents](../README.md) · Next: [377. LVM and RAID](lvm-and-raid.md)
