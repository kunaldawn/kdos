# Recovering an unbootable system

This page is the procedure for an installed KDOS machine that does not reach a working desktop:
how to tell where the boot stopped, which way in each case leaves you (the recovery console, the
[rescue shell]{.idx}, [single-user]{.idx term="single-user mode"} and emergency mode, the live medium), and how to repair the common
causes from there: a failing service, a damaged filesystem, a kernel missing from the EFI system
partition ([ESP]{.idx term="EFI system partition"}), a boot loader that is gone, an update that will not boot. The boot path itself is
explained in [Boot and init](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops); this
page gives the procedures.

## Installation

Every way in ships on the image; nothing has to be installed beforehand. One thing has to be set
up in advance: a root password, for single-user and emergency mode.

| Way in | Provided by | Reached |
|---|---|---|
| The recovery console, a plain login on `tty2` | `fs/etc/inittab`, [`agetty`]{.idx} from [util-linux](#port-util-linux) | `Ctrl+Alt+F2` from the desktop, `Alt+F2` from a text console |
| The rescue shell | The initramfs `init` (`script/phases/70_image/090_initramfs.sh`) | By itself, when the initramfs cannot find, unlock or mount the root |
| Single-user mode | [sysvinit](#port-sysvinit) and [`sulogin`]{.idx} from util-linux | The `KDOS (single user)` menu entry |
| Emergency mode | The same | `emergency` added to an entry in Limine's editor |
| The live medium | The KDOS stick the machine was installed from, or any later one | The firmware's boot menu |
| A memory test | [memtest86plus](#port-memtest86plus) | The `Memory Test (memtest86+)` menu entry, UEFI only |

The live medium carries every tool the procedures below use: `cryptsetup`, `lvm` and `vgchange`,
`mdadm`, `e2fsck`, [`fsck.fat`]{.idx}, `limine`, `efibootmgr` and `kdos-bootctl`.

## Configuration

The consoles and single-user mode are entries of `/etc/inittab`, shipped from `fs/etc/inittab`:

<!-- kfs:include fs/etc/inittab lines=11-40 pin=c1172d0a1a26 -->

`sulogin` asks for **root's** password. The installer locks root unless it was asked to set a root
password (`root_locked` in an answer file, see
[The keys](../../kdos/02-user-guide/installation.md#the-keys)), and a locked root leaves
single-user and emergency mode with no shell: `sulogin` prints
`Cannot open access to console, the root account is locked.` and `Press Enter to continue.`, and
after Enter the boot carries on to runlevel 3. Set one while the machine works:

```bash
sudo passwd root
```

The service scripts are run by this loop in `rcS`, which writes each script's output to a log and
skips any script with a marker under `/etc/service.disabled/`:

<!-- kfs:include fs/etc/init.d/rcS lines=134-173 pin=403936cc08ac -->

## Usage

### Finding where it stopped

The splash names each stage as it runs, so the last name on it says where the boot stopped.

| Stopped at | Go to |
|---|---|
| Nothing on screen, or the firmware reports no boot device | [Putting the boot loader back](#putting-the-boot-loader-back) |
| The menu appears; the chosen entry stops at once | [Putting a kernel back on the ESP](#putting-a-kernel-back-on-the-esp) |
| An initramfs stage, then a shell on `tty1` | [The rescue shell](#the-rescue-shell) |
| `ROOT FILESYSTEM HAS ERRORS - RUN e2fsck` | [Repairing from the live medium](#repairing-from-the-live-medium), step 5 |
| A service stage marked failed, or a stage that never ends | [A service that fails or hangs](#a-service-that-fails-or-hangs) |
| The splash finishes; then a black screen | [The desktop does not start](#the-desktop-does-not-start) |
| The first boot after an update | [After an update](#after-an-update) |

The full table of stages and their logs is in
[When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops).

### The desktop does not start

1. Press `Ctrl+Alt+F2` (`Alt+F2` if the screen is a text console) and log in on `tty2`.
2. Run `kdos doctor`, and read its Session and Desktop sections.
3. Read the service logs of this boot: `ls /run/kdos-init.*.log`, and
   `/run/kdos-svc.<name>.log` for a supervised daemon.
4. Read the console's own log for `tty1`: `/run/kdos-getty.tty1.log`.
5. The session itself is described in [The session](../../kdos/03-architecture/session.md); a
   session that fails leaves a shell on `tty1`, not a black screen.

### A service that fails or hangs

A failed script does not stop the boot; its stage is marked failed, the last lines of its output
are on the splash, and the whole output is in `/run/kdos-init.<name>.log`. A script that never
returns stops the boot at its stage, and no login prompt appears, because the consoles start only
after `rcS` finishes.

1. If a login prompt appears, log in on `tty2` and read `/run/kdos-init.<name>.log`.
2. If the boot hangs, reboot into emergency mode (below), which opens a root shell before `rcS`
   runs.
3. Disable the script by its name, without the number and the `.sh`; for `45_avahi.sh` that is
   `avahi`:

   ```bash
   touch /etc/service.disabled/<name>
   ```

4. Leave the shell, from emergency mode, or reboot. `rcS` skips the script, prints
   `[KDOS] Skipping disabled service: <name>` on the console, and writes no
   `/run/kdos-init.<name>.log` for it. On the live medium the console is the serial line; on an
   installed machine it is the screen, and the line does not reach `/var/log/messages`.
5. Repair the service from the running system, then remove the marker to enable it again.

### Single-user mode

1. Stop the boot menu's countdown and choose `KDOS (single user)`.
2. `rcS` runs as on every boot, so every enabled service starts and every mount in `/etc/fstab`,
   the ESP included, is made.
3. `sulogin` asks for root's password on the console:
   `Enter root password for system maintenance` and `(or press Control-D to continue):`. A root
   shell opens, with no getty on any console and no desktop; `runlevel` prints `N S`.
4. Leaving the shell continues to the running system: init prints `INIT: Entering runlevel: 3`
   and the consoles start.

From a running system, `sudo telinit S` reaches the same state after ending the consoles and the
session.

### Emergency mode

1. Stop the countdown, highlight `KDOS`, and press `E`.
2. Add `emergency` to the `cmdline:` line, and press `F10` to boot it. The editor is described
   in [Adding a parameter for one boot](boot-menu-and-kernel-parameters.md#adding-a-parameter-for-one-boot).
3. `sulogin` runs before `rcS`, with the same prompt as single-user mode: only the initramfs's
   mounts exist, which are the root, `/proc`, `/sys`, `/dev` with `/dev/pts` and `/dev/shm`, and
   no service has run (`/run/kdos-init.*.log` does not exist yet; `runlevel` prints `unknown`).
4. Leaving the shell continues the boot with `rcS`, and from there as on any boot.

### The rescue shell

The initramfs opens it when it cannot find the root, cannot unlock its container, or finds no
`/sbin/init` on it; the reason is printed above the prompt. What each reason means, the tools the
shell has and how to restart from it are on
[The initramfs and the rescue shell](initramfs-and-rescue.md#using-the-rescue-shell). The two
facts to keep in mind: leaving the shell with `exit` panics the kernel, and
`sync; echo b > /proc/sysrq-trigger` restarts the machine.

### Repairing from the live medium

This is the way in that needs no password on the machine and works whatever state the installed
system is in. Steps 2 and 4 to 6 have been run against an installed virtual machine with an
unencrypted root on LVM, where `vgchange -aay` reports `2 logical volume(s) in volume group "kdos" now active`
for a machine with two slots; step 3 has not, because that machine has no LUKS container.

1. Boot the KDOS stick and open a terminal. On the live medium root's password is `kdos`, and the
   `kdos` account can use `sudo` with its own password, which is also `kdos`.
2. Find the disk and its partitions: `lsblk -f`, or `sudo blkid`.
3. Open an encrypted root under the name the initramfs uses:

   ```bash
   sudo cryptsetup open /dev/sdX2 kdosroot
   ```

4. Activate any volume groups: `sudo vgchange -aay`.
5. Check the filesystems while they are not mounted: `sudo e2fsck -f <root device>` for an ext
   root, `sudo fsck.fat -a <esp device>` for the ESP. btrfs, xfs and f2fs check themselves at mount
   time.
6. Mount the root and its ESP:

   ```bash
   sudo mkdir -p /mnt/target
   sudo mount <root device> /mnt/target
   sudo mount <esp device> /mnt/target/boot/efi
   ```

7. Repair what the logs point at: read `/mnt/target/var/log/messages`, disable a service with
   `sudo touch /mnt/target/etc/service.disabled/<name>`, or follow the two procedures below.
8. Flush and unmount: `sync`, then `sudo umount /mnt/target/boot/efi /mnt/target`, and
   `sudo cryptsetup close kdosroot` if you opened one. Reboot without the stick.

### Putting a kernel back on the ESP

Each slot boots the kernel in `EFI/kdos/<slot>/` on the ESP, copied from that root's `/boot`. With
the root and its ESP mounted as above, `kdos-bootctl` can be pointed at them through
`KDOS_BOOTSTATE`, from which it also takes the ESP and `limine.conf`:

```bash
sudo KDOS_BOOTSTATE=/mnt/target/boot/efi/EFI/kdos/bootstate kdos-bootctl deploy /mnt/target a
```

It prints `slot a boots 7.2.11 from EFI/kdos/a` and regenerates the `/KDOS` entries of the ESP's
`limine.conf`. With `KDOS_BOOTSTATE` redirected and `KDOS_EFIVARS` unset, it writes no firmware
variable. It refuses, and writes nothing, when the ESP has no room for both files or the
initramfs carries no modules for the kernel.

When the boot state itself is gone, the same variable recreates it: `kdos-bootctl status` reports
`no boot state at …` and exits 1, and `set-slot a <filesystem-uuid>` writes a fresh state with
slot A active (`set-slot b <uuid>` then records a second slot). Both procedures have been run
from the live medium against an installed virtual machine whose `limine.conf` and boot state had
been lost, and the machine booted again afterwards.

### Putting the boot loader back

With the ESP mounted at `/mnt/target/boot/efi`, and the whole disk as `/dev/sdX`:

1. The UEFI loaders, at the path the firmware falls back to:

   ```bash
   sudo mkdir -p /mnt/target/boot/efi/EFI/BOOT
   sudo cp /usr/share/limine/BOOTX64.EFI /usr/share/limine/BOOTIA32.EFI /mnt/target/boot/efi/EFI/BOOT/
   ```

2. The BIOS second stage, at the root of the ESP, and the BIOS boot code:

   ```bash
   sudo cp /usr/share/limine/limine-bios.sys /mnt/target/boot/efi/
   sudo limine bios-install /dev/sdX 2
   ```

   The `2` is the BIOS boot partition, partition 2 of the layout the installer's erase plan
   writes, where Limine's stage goes on a GPT disk. Without the number Limine finds a BIOS boot
   partition by its type itself (`Autodetected partition 2 as BIOS boot partition.`), and on a
   dos (MBR) disk it needs none. A GPT disk with no BIOS boot partition gets
   `error: Installing to a GPT device, but no BIOS boot partition specified or detected.`, the
   command undoes its work, and the disk starts only through UEFI.

3. On a UEFI machine, the firmware entry the installer writes, naming the ESP's partition number:

   ```bash
   sudo efibootmgr --create --disk /dev/sdX --part <n> --loader '\EFI\BOOT\BOOTX64.EFI' --label KDOS
   ```

   The firmware's removable-media fallback starts `EFI/BOOT/BOOTX64.EFI` without an entry.
4. `limine.conf` itself must still be at the root of the ESP. If it is gone, no tool writes it
   from nothing; its shape is on [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md#the-installed-machines-menu).
   Writing the three `/KDOS` entries by hand, with the right `kdos_slot=`, `bootstate=UUID=` and
   `root=UUID=` words, is enough: the next `kdos-bootctl` write (a `set-slot`, a `deploy`, the
   `mark-good` at the end of the boot) regenerates the other entries, and, once the machine runs
   again, `kdos theme <accent>` (or `sudo kdos-bootctl theme <accent>`) puts the theme keys back.

These are the installer's own steps (`src/system/kdos-installer/install.c`), run by hand. Steps 1,
3 and 4 have been run from the live medium against an installed virtual machine, which then
booted through UEFI; step 2, on a disk with the erase plan's layout, made it start Limine and its
kernel on SeaBIOS.

### After an update

An A/B update that does not finish booting rolls itself back. To leave it sooner, pick the
confirmed slot's `KDOS (slot <x>)` entry; see [A/B slots and rollback](ab-slots-and-rollback.md).
An update applied with `--in-place` has no second slot to fall back to: boot the live medium and
repair the root as above.

## Tips

- `tty2` is a plain `agetty` with no automatic login, and nothing on the desktop's path can
  change that, so it is there whenever `rcS` has finished.
- With a root password set, most repairs can be made in single-user or emergency mode without the
  live medium. Set one before you need it.
- The machine's own boot logs under `/run` are gone at every reboot; `/var/log/messages` keeps the
  system log across boots.
- The initramfs ignores `init=`: there is no `init=/bin/sh` shortcut. Emergency mode is the
  nearest equivalent.
- Random crashes and a filesystem that keeps acquiring errors are a reason to run the memory test
  before anything else.

## Troubleshooting

**`sulogin` says the root account is locked.** Root has no password. Boot the live medium, mount
the root as in [Repairing from the live medium](#repairing-from-the-live-medium), give it the live
system's devices, and set one inside it:

```bash
sudo mount --bind /dev /mnt/target/dev
sudo chroot /mnt/target passwd root
sudo umount /mnt/target/dev
```

`passwd` asks `New password:` and `Re-enter new password:` on the terminal and prints
`passwd: password changed.`; `sudo chroot /mnt/target passwd -S root` reads `root L …` before and
`root P …` after. This has been run from the live medium against an installed virtual machine
whose root had been locked with `passwd -l root`, and root could log in on that machine
afterwards.

**The `KDOS (single user)` entry boots straight to the normal system.** `sulogin` gave up for the
reason above, and after Enter init printed `INIT: Entering runlevel: 3` and carried on.

**The machine powers on, and the firmware says there is no bootable device.** On UEFI, check that
Secure Boot is off; Limine's loader is unsigned. Otherwise
[put the boot loader back](#putting-the-boot-loader-back).

**The menu appears and every entry fails at once.** The kernel or initramfs the entry names is
missing from `EFI/kdos/<slot>/`. Mount the ESP from the live medium and look; then
[put a kernel back](#putting-a-kernel-back-on-the-esp).

**`fsck: errors left uncorrected` on the splash.** A filesystem other than the root has errors;
the report is in `/run/kdos-fsck.log`. On the ESP, repair it with `fsck.fat` from the live medium.

**The power button does nothing at a text console.** That is expected: it works only inside the
graphical session. Use `sudo poweroff`.

## Known gaps

- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported)
- [A disk-encryption key is typed, not sealed in the TPM](../../kdos/06-reference/known-gaps.md#a-disk-encryption-key-is-typed-not-sealed-in-the-tpm)
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)
- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only)

## See also

- [When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops) and
  [Single-user and emergency mode](../../kdos/03-architecture/boot-and-init.md#single-user-and-emergency-mode)
- [Diagnosing](../../kdos/02-user-guide/administration.md#diagnosing) and
  [Services](../../kdos/02-user-guide/administration.md#services)
- [What the installer writes](../../kdos/02-user-guide/installation.md#what-the-installer-writes)
- [The initramfs and the rescue shell](initramfs-and-rescue.md): the rescue shell's messages and
  what each one means
- [A/B slots and rollback](ab-slots-and-rollback.md): rolling an update back and booting the
  other slot once
- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md): reaching the menu and
  adding a parameter for one boot
- [Live persistence](live-persistence.md): booting the live medium past its persistence store
- [Init and runlevels](init-and-runlevels.md): `/etc/inittab`, the recovery console and
  single-user mode
- [Services](services.md): finding out why a service did not start, and keeping one from starting
- [Logs](logs.md): what a boot printed, and the system log across boots
- [Encrypted root](encrypted-root.md): opening an encrypted root by hand from the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 421.* Previous: [420. Desktop troubleshooting](desktop-troubleshooting.md) · [Contents](../README.md) · Next: [Part XVII. Appendices](../17-appendices/intro.md)
