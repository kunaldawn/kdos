# Firmware updates

This page covers updating the [firmware]{.idx} inside a machine's devices with [fwupd]{.idx}: an SSD's
controller, a dock, a USB peripheral, and the system firmware itself (the BIOS or [UEFI]{.idx}, the
embedded controller) where the vendor publishes it as a UEFI capsule. The firmware files the
kernel loads for its drivers, and CPU microcode, are a different thing; they come with packages
and are covered in [Microcode and firmware](microcode-and-firmware.md). How fwupd finds the EFI
system partition when udisks does not name it, and what it writes there, is explained in
[Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
and [Where the kernel is kept](../../kdos/03-architecture/boot-and-init.md#where-the-kernel-is-kept).

## Installation

| Port | Provides |
|:--|:--|
| [fwupd](#port-fwupd) | The daemon, `fwupdmgr` (its client), `fwupdtool` (the same engine run directly as root), and the LVFS remotes, every download remote switched off |
| [fwupd-snapshot](#port-fwupd-snapshot) | The LVFS metadata as it stood when the release was cut, which the `kdos-snapshot` local remote reads, so firmware is listed with no network |
| [fwupd-efi](#port-fwupd-efi) | `fwupdx64.efi`, the EFI program that hands a staged capsule to the firmware at the next boot |
| [discover](#port-discover) | Discover's firmware backend, which lists the same updates on its Updates page and installs them through the daemon |

Both are on the image, live and installed. Neither has a service script and nothing starts fwupd at
boot: the [system bus]{.idx} starts it the first time a client calls it, through the setuid
`dbus-daemon-launch-helper`. The recipe deletes the GRUB hook fwupd would install, since KDOS boots
with [Limine]{.idx}:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=45-47 pin=095c8c5973bf -->

## Configuration

### /etc/fwupd/fwupd.conf

The recipe writes fwupd's configuration whole. Its one setting says where the EFI system partition
is mounted:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=49-54 pin=6d571d1f5469 -->

[`/boot/efi`]{.idx} is where the installer mounts the ESP of an installed system, in the `fstab` it writes
(`src/system/kinstall-core/install.c`):

<!-- kfs:include src/system/kinstall-core/install.c lines=1952-1957 pin=d44ca257584d -->

fwupd asks udisks for the ESP. When udisks names none, the recipe's patch describes the FAT
filesystem mounted at `EspLocation` itself, from sysfs and the udev database; a path that is not a
FAT mount point is refused, so a machine with nothing mounted there has no ESP rather than a wrong
one (`ports/core/boot/fwupd/esp-mount-path.patch`, first hunk):

<!-- kfs:include ports/core/boot/fwupd/esp-mount-path.patch lines=1-22 pin=ee4a4116b1a2 -->

### The remotes

Where metadata comes from is a set of remotes. The one KDOS enables is `kdos-snapshot`, a local
remote that reads the LVFS metadata the release carries, from
`fs/usr/share/fwupd/remotes.d/kdos-snapshot.conf`:

<!-- kfs:include fs/usr/share/fwupd/remotes.d/kdos-snapshot.conf -->

Nothing in the system downloads metadata on its own: there is no refresh timer, and the LVFS's own
download remote, [`/etc/fwupd/remotes.d/lvfs.conf`]{.idx term="/etc/fwupd/remotes.d"}, is
installed switched off:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=43-43 pin=a473138dd843 -->

Discover's firmware backend refreshes every enabled download remote each time it starts, so an
enabled LVFS remote would be a download at every start of the software centre. Upstream's
`vendor-directory` remote, for firmware a vendor package drops under
`/usr/share/fwupd/remotes.d/vendor/firmware`, stays enabled; `lvfs-testing` and `lvfs-embargo`
stay off. Installing an update fetches its cabinet from `FirmwareBaseURI`, a download somebody
started. The build turns off Passim, so nothing is fetched from or shared with other machines on
the local network, and the peer-to-peer policy is `none`:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=27-28 pin=8fa943ab7d97 -->

### The EFI loader

`fwupd-efi` builds the capsule loader with a KDOS SBAT entry:

<!-- kfs:include ports/core/boot/fwupd-efi/build.sh lines=12-18 pin=aaf697f075bc -->

It is unsigned. KDOS requires [Secure Boot]{.idx} to be off in any case; see
[Known gaps](#known-gaps).

### UEFI variables

A capsule update is armed through a UEFI variable, and fwupd reads the firmware's own update table
(the ESRT) through [`/sys/firmware/efi`]{.idx}. `rcS` mounts `efivarfs` on every UEFI boot
(`fs/etc/init.d/rcS`):

<!-- kfs:include fs/etc/init.d/rcS lines=86-94 pin=66ff50c97107 -->

## Usage

Reading needs no privilege: `fwupdmgr get-devices`, `get-remotes`, `get-updates`, `get-history`
and `security` answer any account. polkit answers everything that changes something from the
caller's elogind session: fwupd's own rule lets a member of `wheel` do it, and
`/etc/polkit-1/rules.d/50-kdos.rules` holds the system firmware flash,
`org.freedesktop.fwupd.update-internal`, at an administrator's password even at the machine.
Plasma's authentication agent asks for it, in Discover and for `fwupdmgr` in Konsole alike. A
command from an SSH login or another account with no session at the machine runs with `sudo`. See
[sudo, PAM and polkit](sudo-pam-polkit.md). Listing needs no network; installing an update fetches
its file from the LVFS.

Every `fwupdmgr` command on a machine with no ESP mounted, the live medium among them, starts with
`WARNING: UEFI ESP partition not detected or configured`, and every one on KDOS with
`WARNING: This package has not been validated, it may not work properly.`, fwupd's notice for a
build its upstream has not validated. Neither stops the command.

### Seeing what fwupd can update

1. `fwupdmgr get-devices` lists every device fwupd recognises, its current firmware version,
   and whether it can be updated. Devices it can only identify, such as a virtual DVD drive or the
   processor's microcode version, are listed too, with no *Updatable* flag.
2. `fwupdmgr get-remotes` lists the remotes and whether each is enabled.

### Updating

From the desktop, Discover's **Updates** page lists a device's newer firmware beside the other
updates, with the vendor's release notes, and installs it. From a terminal:

1. `fwupdmgr get-updates` lists the newer firmware the snapshot names for these devices, with the
   vendor's release notes. With nothing updatable it says `No updatable devices` and exits 2.
2. `fwupdmgr update` downloads and applies it, device by device, and asks for confirmation.
3. While a device is being flashed, fwupd holds an elogind shutdown and sleep inhibitor, so a
   power-off or suspend asked for meanwhile waits for it.
4. A device updated while running, such as a dock, an SSD or a USB device, is done when the
   command returns; some ask to be unplugged or the machine restarted.
5. System firmware is staged, not applied: fwupd writes the capsule to the ESP under
   `EFI/kdos/fw/` (or `EFI/UpdateCapsule/`), copies `fwupdx64.efi` to `EFI/kdos/` and sets a
   one-time `BootNext`. `fwupdmgr` offers to restart the machine itself, through elogind; or
   reboot from the desktop. That boot runs `fwupdx64.efi`, which hands the
   capsule to the firmware; `BootNext` lasts one boot, so the boot after it is the ordinary one,
   through Limine.
6. After the reboot, `fwupdmgr get-history` shows whether the update succeeded; before any update
   it says `No history`.

Step 5 has not been carried out on any machine; see [Known gaps](#known-gaps).

### Reading the host security report

`fwupdmgr security` prints fwupd's Host Security ID report: which platform protections the
firmware and the processor have turned on. fwupd is built with this report enabled. In a virtual
machine it prints only `HSI unavailable for unprivileged hypervisor` and exits 1.

### Firmware published since the release

The snapshot is as current as the release. To list firmware the LVFS has published since:

1. `sudo fwupdmgr enable-remote lvfs`. The LVFS remote then outranks the snapshot, and Discover
   refreshes its metadata each time it starts.
2. `fwupdmgr refresh`, to fetch it now.

`sudo fwupdmgr disable-remote lvfs` goes back to the snapshot alone.

### The live medium and an installed system

The live medium mounts no ESP, so fwupd offers nothing that needs one: only devices updated over
their own bus, such as USB devices and docks. There `sudo fwupdtool esp-list` says
`No ESP or BDP found` and exits 3. An installed system booted through UEFI has the ESP
at `/boot/efi`, and capsule updates are offered there too. A machine booted through BIOS has no
UEFI variables and no ESRT, so it has no capsule updates at all.

## Tips

- `sudo fwupdtool esp-list` prints the ESP fwupd found, which is the quickest way to see that the
  `EspLocation` route works on this machine.
- A device whose vendor does not publish to the LVFS is not listed by `get-updates`, however old
  its firmware is, and neither is firmware published after the release while the LVFS remote is
  off.
- `kdos doctor` checks the setuid bit of `dbus-daemon-launch-helper`; without it the system bus
  cannot start fwupd at all.
- fwupd writes nothing into Limine's configuration, and the recipe removes the GRUB hook it would
  otherwise install (see [Installation](#installation)).

## Troubleshooting

**`fwupdmgr` cannot connect to the daemon.** The bus failed to start it.

1. Run `kdos doctor` and look for the `dbus-daemon-launch-helper` line in its Desktop section. A
   missing setuid bit, or the wrong group, stops every bus-started system service.
2. Check the system bus: `service status dbus`.
3. Run the engine without the daemon: `sudo fwupdtool get-devices`. If that works, the problem is
   the activation, not fwupd.

**`fwupdmgr` says `Failed to obtain auth`.** The command changes something (`refresh`, `update`,
`enable-remote` and the like) and polkit refused it: the caller is not in `wheel`, the password
prompt was cancelled, or the command ran outside the session at the machine. Run it with `sudo`.

**No UEFI device is listed on an installed machine.**

1. `findmnt /boot/efi`: the ESP must be mounted there.
2. `findmnt /sys/firmware/efi/efivars`: `rcS` mounts it on a UEFI boot. No output on a machine
   that booted through UEFI (`/sys/firmware/efi` exists) means the mount failed; mount it by hand
   with the line from `rcS` above.
3. `ls /sys/firmware/efi/esrt/entries`: no entries means the firmware publishes nothing fwupd can
   update.
4. `sudo fwupdtool esp-list`: no ESP here means the patched lookup did not describe the mount;
   check that [`/etc/fwupd/fwupd.conf`]{.idx} still says `EspLocation=/boot/efi`.

**The capsule was staged but nothing happened at reboot.** Check that Secure Boot is off in
firmware setup; with it on, the firmware refuses to start the unsigned `fwupdx64.efi`. Then
`sudo fwupdmgr get-history` for fwupd's record of the attempt.

**`get-updates` says there is no metadata.** `fwupdmgr get-remotes` shows whether `kdos-snapshot`
is enabled; its files are in `/usr/share/fwupd/remotes.d/kdos-snapshot/`, which the
[fwupd-snapshot](#port-fwupd-snapshot) package installs.

## Known gaps

- [A UEFI firmware (capsule) update has not been applied](../../kdos/06-reference/known-gaps.md#a-uefi-firmware-capsule-update-has-not-been-applied)
- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)

## See also

- [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates):
  fwupd, the ESP route and the loader
- [Where the kernel is kept](../../kdos/03-architecture/boot-and-init.md#where-the-kernel-is-kept):
  what fwupd writes on the ESP
- [sudo, PAM and polkit](sudo-pam-polkit.md): who is asked for a password, and where
- [Offline by construction](../../kdos/01-philosophy/principles.md#offline-by-construction): why
  nothing refreshes the metadata on its own
- [Microcode and firmware](microcode-and-firmware.md): the firmware files drivers load, and CPU
  microcode
- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md): the Limine menu the boot
  after a staged capsule returns to

<!-- book-nav -->
---

*Part XVI. System administration, chapter 429.* Previous: [428. Laptops](laptops.md) · [Contents](../README.md) · Next: [430. kpkg](kpkg.md)
