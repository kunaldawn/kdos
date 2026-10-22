# Firmware updates

This page covers updating the [firmware]{.idx} inside a machine's devices with [fwupd]{.idx}: an SSD's
controller, a dock, a USB peripheral, and the system firmware itself (the BIOS or [UEFI]{.idx}, the
embedded controller) where the vendor publishes it as a UEFI capsule. The firmware files the
kernel loads for its drivers, and CPU microcode, are a different thing; they come with packages
and are covered in [Microcode and firmware](microcode-and-firmware.md). How fwupd reaches the EFI
system partition without udisks, and what it writes there, is explained in
[Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
and [Where the kernel is kept](../../kdos/03-architecture/boot-and-init.md#where-the-kernel-is-kept).

## Installation

| Port | Provides |
|:--|:--|
| [fwupd](#port-fwupd) | The daemon, `fwupdmgr` (its client), `fwupdtool` (the same engine run directly as root), the LVFS remotes and a daily metadata job |
| [fwupd-efi](#port-fwupd-efi) | `fwupdx64.efi`, the EFI program that hands a staged capsule to the firmware at the next boot |

Both are on the image, live and installed. Neither has a service script and nothing starts fwupd at
boot: the [system bus]{.idx} starts it the first time a client calls it, through the setuid
`dbus-daemon-launch-helper`. The recipe deletes the GRUB hook fwupd would install, since KDOS boots
with [Limine]{.idx}:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=44-46 pin=095c8c5973bf -->

## Configuration

### /etc/fwupd/fwupd.conf

The recipe writes fwupd's configuration whole. Its one setting says where the EFI system partition
is mounted:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=48-53 pin=6d571d1f5469 -->

[`/boot/efi`]{.idx} is where the installer mounts the ESP of an installed system, in the `fstab` it writes
(`src/system/kdos-installer/install.c`):

<!-- kfs:include src/system/kdos-installer/install.c lines=1515-1520 pin=d44ca257584d -->

fwupd normally finds the ESP by asking udisks. The recipe patches it to describe the FAT filesystem
mounted at `EspLocation` from sysfs instead
(`ports/core/boot/fwupd/esp-mount-path.patch`, first hunk):

<!-- kfs:include ports/core/boot/fwupd/esp-mount-path.patch lines=1-22 pin=ee4a4116b1a2 -->

### The metadata job

The list of firmware the LVFS offers is refreshed once a day by a [timer](timers.md) the recipe
installs as [`/etc/kdos/timers.d/30-fwupd-refresh.timer`]{.idx}:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=55-62 pin=8606213f3c62 -->

### The remotes

Where metadata and firmware come from is upstream's [`/etc/fwupd/remotes.d/`]{.idx term="/etc/fwupd/remotes.d"}, installed unchanged:
four remotes, of which `lvfs` (the LVFS stable remote) and `vendor-directory` (firmware a vendor
package drops under `/usr/share/fwupd/remotes.d/vendor/firmware`) are enabled, and `lvfs-testing`
and `lvfs-embargo` are not. The build turns off Passim, so nothing
is fetched from or shared with other machines on the local network, and the peer-to-peer policy is
`none`:

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

<!-- kfs:include fs/etc/init.d/rcS lines=81-89 pin=7bd15329fa62 -->

## Usage

Reading needs no privilege: `fwupdmgr get-devices`, `get-remotes`, `get-updates`, `get-history`
and `security` answer any account. Everything that changes something, `refresh` included, runs as
root: fwupd's polkit actions ask for an administrator, and with no session and no authentication
agent polkit refuses everyone but root, which `fwupdmgr` reports as `Failed to obtain auth`; see
[polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent).
fwupd needs the network to reach the LVFS.

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

1. Refresh the metadata, if the daily job has not run: `sudo fwupdmgr refresh`.
2. `fwupdmgr get-updates` lists the newer firmware the LVFS has for these devices, with the
   vendor's release notes. With nothing updatable it says `No updatable devices` and exits 2.
3. `sudo fwupdmgr update` downloads and applies it, device by device, and asks for confirmation.
4. A device updated while running, such as a dock, an SSD or a USB device, is done when the
   command returns; some ask to be unplugged or the machine restarted.
5. System firmware is staged, not applied: fwupd writes the capsule to the ESP under
   `EFI/kdos/fw/` (or `EFI/UpdateCapsule/`), copies `fwupdx64.efi` to `EFI/kdos/` and sets a
   one-time `BootNext`. Reboot with `sudo reboot`. That boot runs `fwupdx64.efi`, which hands the
   capsule to the firmware; `BootNext` lasts one boot, so the boot after it is the ordinary one,
   through Limine.
6. After the reboot, `fwupdmgr get-history` shows whether the update succeeded; before any update
   it says `No history`.

Step 5 has not been carried out on any machine; see [Known gaps](#known-gaps).

### Reading the host security report

`fwupdmgr security` prints fwupd's Host Security ID report: which platform protections the
firmware and the processor have turned on. fwupd is built with this report enabled. In a virtual
machine it prints only `HSI unavailable for unprivileged hypervisor` and exits 1.

### Running the metadata job at once

The job's name is `fwupd-refresh`; [Timers](timers.md#running-a-job-now) says how to run a job at
once. Running `sudo fwupdmgr refresh` does the same thing directly.

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
  its firmware is.
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
`enable-remote` and the like); run it with `sudo`.

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

**`get-updates` says there is no metadata.** The daily job has not run, or ran with no network.
`sudo fwupdmgr refresh`, and check the network.

## Known gaps

- [A UEFI firmware (capsule) update has not been applied](../../kdos/06-reference/known-gaps.md#a-uefi-firmware-capsule-update-has-not-been-applied)
- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)

## See also

- [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates):
  fwupd, the ESP route and the loader
- [Where the kernel is kept](../../kdos/03-architecture/boot-and-init.md#where-the-kernel-is-kept):
  what fwupd writes on the ESP
- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent)
- [Periodic jobs](../../kdos/02-user-guide/administration.md#periodic-jobs): the metadata job
- `microcode-and-firmware.md`: [Microcode and firmware](microcode-and-firmware.md)
- `timers.md`: [Timers](timers.md)
- `boot-menu-and-kernel-parameters.md`: [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 403.* Previous: [402. Laptops](laptops.md) · [Contents](../README.md) · Next: [404. kpkg](kpkg.md)
