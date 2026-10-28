# Microcode and firmware

This page covers the binary files the hardware needs from the operating system: CPU
[microcode]{.idx}, the [firmware]{.idx} blobs drivers load, the Intel [audio]{.idx} DSP firmware, the wireless
regulatory database, and firmware updates for the devices themselves through [`fwupd`]{.idx}. It says
which port provides each, where each lands, how to check that the machine is using it, and what to
do when a device comes up without it. Why microcode is carried the way it is, and what the early
loader requires, is explained in [Microcode](../../kdos/03-architecture/boot-and-init.md#microcode);
the firmware choices are in [Hardware](../../kdos/02-user-guide/administration.md#hardware).

## Installation

| Port | Provides | Where it lands |
|---|---|---|
| [intel-ucode](#port-intel-ucode) | Intel CPU microcode, as one bundle | `/usr/lib/firmware/intel-ucode.bin` |
| [linux-firmware](#port-linux-firmware) | Upstream's firmware tree, zstd-compressed, less the families no x86 kernel loads; AMD CPU microcode is part of it | [`/usr/lib/firmware/`]{.idx term="/usr/lib/firmware"}, with AMD's under `amd-ucode/` |
| [sof-firmware](#port-sof-firmware) | Intel Sound Open Firmware: the audio DSP firmware and its topology files | `/usr/lib/firmware/intel/` |
| [wireless-regdb](#port-wireless-regdb) | The signed table of radio channels and power levels per country | `/usr/lib/firmware/regulatory.db` and `regulatory.db.p7s` |
| [fwupd](#port-fwupd) | `fwupdmgr` and the daemon that updates device firmware from the LVFS | `/usr/bin/fwupdmgr`, [`/etc/fwupd/`]{.idx term="/etc/fwupd"} |
| [fwupd-efi](#port-fwupd-efi) | The EFI program that hands a UEFI capsule to the firmware at the next boot | Placed on the ESP by `fwupd` when it stages an update |

`/lib` is a link to `/usr/lib` on KDOS, so the two spellings name the same files. All of these are
installed on the image. None of them has a service script: the kernel reads firmware files when a
driver asks, the microcode is read before any filesystem exists, and `fwupd` is started by the
system bus on first use.

The microcode does not reach the processor from `/usr/lib/firmware`. The image build copies it
into the front of the initramfs, uncompressed, which is the only place the kernel's early loader
looks; late loading is off in the kernel.

## Configuration

### Microcode

The Intel bundle is the concatenation of upstream's `intel-ucode/` directory. The recipe then
walks it record by record and stops the build when a record's header is not microcode or a record
runs past the end of the file:

<!-- kfs:include ports/core/boot/intel-ucode/build.sh -->

The image build turns both vendors' files into the uncompressed archive the early loader scans
(`script/phases/70_image/090_initramfs.sh`):

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=1315-1379 pin=b6eaf412d8f2 -->

### Device firmware

[`linux-firmware`]{.idx} is installed with upstream's own script, so the alias links drivers ask for
exist, and then the firmware for hardware an x86 kernel cannot drive is removed:

<!-- kfs:include ports/core/boot/linux-firmware/build.sh lines=12-28 pin=14e9ca89db09 -->

The SOF firmware and topologies, without which an Intel DSP loads and makes no sound:

<!-- kfs:include ports/core/boot/sof-firmware/build.sh lines=12-16 pin=4ab5a2719e2a -->

### The regulatory database and the country

The database is rebuilt from upstream's `db.txt` and checked against upstream's signature, because
the kernel loads `regulatory.db` only when a valid `regulatory.db.p7s` sits beside it:

<!-- kfs:include ports/core/boot/wireless-regdb/build.sh lines=12-30 pin=8370dda29bc2 -->

The country is a module option, [`/etc/modprobe.d/kdos-regdom.conf`]{.idx}. No file in `fs/` ships it; the
installer writes it from the [time zone]{.idx} you choose, and `kdos-powerd` writes it whenever the time
zone is set, as a two-letter code taken from `zone.tab`:

<!-- kfs:include src/daemons/kdos-powerd/main.c lines=805-831 pin=07f9a54f24a7 -->

A zone with no country, such as `UTC`, removes the file, and the radio stays on the world domain,
`00`. On a machine installed with the zone `Europe/Berlin`, the file reads:

```text
# Written by the KDOS installer from the timezone (Europe/Berlin):
# the country the Wi-Fi radio's channels and transmit
# power are set for.
options cfg80211 ieee80211_regdom=DE
```

and after boot `iw reg get` begins `global` / `country DE: DFS-ETSI`, and on a machine with a
wireless device `kdos doctor` reports `[ ok ] regulatory.db signed and loaded (regdom DE)`.

### fwupd

The recipe writes `fwupd`'s configuration, which tells it where the ESP is mounted, and a daily
job that refreshes the LVFS metadata:

<!-- kfs:include ports/core/boot/fwupd/build.sh lines=48-62 pin=c9eb6e7687c7 -->

On the image these are `/etc/fwupd/fwupd.conf` (readable by root only), whose one setting is
`EspLocation=/boot/efi`, and `/etc/kdos/timers.d/30-fwupd-refresh.timer`, which `18_timers` starts at boot as
the supervised `kdos-timer-fwupd-refresh`. The job's line format is described in
[Periodic jobs](../../kdos/02-user-guide/administration.md#periodic-jobs).

## Usage

### Checking the microcode

1. Run `kdos doctor`. Its Kernel section reports whether the initramfs carries microcode for
   this processor's vendor, as `[ ok ] AMD microcode in the initrd` or
   `[ ok ] Intel microcode in the initrd`, followed by `(revision …)` when the kernel exposes the
   running revision.
2. For the revision alone: `cat /sys/devices/system/cpu/cpu0/microcode/version`. A virtual
   machine has no such file, and the doctor line then carries no revision.
3. For what the kernel did with it at boot: `dmesg | grep -i microcode`. On real hardware the
   early loader prints the revision it applied; a virtual machine prints no loader line at all,
   only the CPU-bug lines that mention microcode, such as
   `Transient Scheduler Attacks: Vulnerable: No microcode`.

`kdos doctor` searches `/boot/initramfs.cpio.gz`, the image's copy in the root. The copy an
installed machine boots is the one in its slot's directory on the ESP; to list its microcode
archive, see [Looking inside](initramfs-and-rescue.md#looking-inside).

### Getting newer microcode

The microcode the machine loads is the copy in the initramfs, and only the image build writes the
base initramfs. Installing a newer `intel-ucode` or `linux-firmware` package updates
`/usr/lib/firmware` and changes nothing the early loader reads; a kernel update appends modules to
the same base. Newer microcode reaches the processor only through an image built from a tree that
carries it: the live medium of that image loads it, and so does a machine installed from it.

### A device that needs firmware

1. Look for the driver's request: `dmesg | grep -i firmware`. A missing file is named there.
2. Check that the file is installed: `ls /usr/lib/firmware/<path>*`. Most files carry a `.zst`
   suffix; the kernel reads compressed firmware itself.
3. Find the package that owns it: `kdos why /usr/lib/firmware/<path>.zst`. It prints the package
   and version, and the recipe path, for example `package  wireless-regdb 2026.09.03-1` and
   `recipe   /ports/core/boot/wireless-regdb/kpkgbuild` for `regulatory.db`.
4. Run `kdos doctor` for the checks it makes of SOF audio firmware and of NVIDIA GSP firmware.

### Setting the Wi-Fi country

1. Set the time zone, which sets the country with it, as a member of `wheel`:
   `kdos-power timezone Europe/Berlin`. It writes `/etc/modprobe.d/kdos-regdom.conf`, headed
   `# Written by kdos-powerd from the timezone (Europe/Berlin)`, and runs `iw reg set` for this
   boot, so `iw reg get` reads `country DE: DFS-ETSI` straight away.
2. To set a country for this boot only: `sudo iw reg set FR`.
3. Check: `iw reg get` shows the country in force now. `kdos doctor` reads the country the
   kernel's `cfg80211` module was loaded with, which comes from the file, so it reports
   `[ ok ] regulatory.db signed and loaded (regdom DE)` from the next boot; until then, and after an
   `iw reg set` alone, it warns that the domain is `00` on a machine with a wireless device and
   reports `[skip]` on one with none.

On the live medium no time zone has been chosen, so the file is absent and the radio is on the
world domain until you set one: `iw reg get` begins `global` / `country 00: DFS-UNSET`.

### Updating device firmware

1. See what the LVFS offers for this machine: `sudo fwupdmgr get-updates`.
2. Apply it: `sudo fwupdmgr update`.
3. Firmware for the system itself, shipped as a UEFI capsule, is staged on the ESP, which the
   recipe's configuration names as [`/boot/efi`]{.idx}, and is applied at the next boot.

The live medium mounts nothing at `/boot/efi`, so a capsule cannot be staged from it. No capsule
update has been staged and booted
([Known gaps](../../kdos/06-reference/known-gaps.md#a-uefi-firmware-capsule-update-has-not-been-applied)).

## Tips

- An initramfs without microcode has no symptom: the processor keeps the revision the firmware
  loaded. `kdos doctor` is the check.
- The image build stops when a vendor's microcode is installed and yields nothing, and prints
  `Microcode: no …` and carries on when a vendor's source is absent altogether. Read those lines
  in the initramfs step's log, `build/logs/70_image/<N>_initramfs.sh.log`, when a doctor warning
  appears; a build that carried both prints `Microcode: Intel bundle carried` and
  `Microcode: AMD containers carried`.
- On a machine without the hardware, as in a virtual machine, `kdos doctor` still checks that the
  files are installed: it reports `[ ok ] SOF firmware installed (no SOF audio device here)` and
  the same for NVIDIA GSP firmware, and `[skip]` for the regulatory database when there is no
  wireless device and the kernel's `cfg80211` module has not been loaded with a country.
- `regulatory.db.p7s` must sit beside `regulatory.db`. Without the signature the kernel ignores
  the database.

## Troubleshooting

**`kdos doctor`: the initrd carries no Intel (or AMD) microcode.** The image was built without that
vendor's files. Check the image phase's log for the `Microcode:` lines from `090_initramfs.sh`. On
the machine there is nothing to repair; the next image carries it once the port is installed in
the build.

**No sound on an Intel laptop.** `kdos doctor` names the case: an SOF device bound with
`/usr/lib/firmware/intel/sof` absent, or the firmware present without `sof-tplg`. Check that the
package is installed (`ls /var/lib/kpkg/db/sof-firmware`), then `dmesg | grep -i sof` for the file
the driver asked for.

**Wi-Fi finds no 5 GHz networks, or only some channels.** The regulatory domain is `00`. Run `iw
reg get`; set the country as in [Setting the Wi-Fi country](#setting-the-wi-fi-country). If
`kdos doctor` says the signature is missing, the `wireless-regdb` package is damaged; reinstall it.

**`fwupdmgr` finds no UEFI device.** `fwupdmgr get-devices` then begins
`WARNING: UEFI ESP partition not detected or configured`, and `get-updates` ends in
`No updatable devices`. On the live medium that is expected. On an installed machine,
check that `/boot/efi` is mounted (`findmnt /boot/efi`) and that `rcS` mounted `efivarfs`
(`findmnt /sys/firmware/efi/efivars`); without it no firmware variable is visible.

**A GPU, Wi-Fi or Bluetooth adapter comes up without its firmware.** Read the file name from
`dmesg` and check it as in [A device that needs firmware](#a-device-that-needs-firmware). A file
that is absent although `linux-firmware` is installed is either one of the families the recipe
[leaves out](../../kdos/06-reference/known-gaps.md#linux-firmware-leaves-out-what-an-x86-kernel-cannot-load),
or newer than the upstream release the recipe pins.

## Known gaps

- [`linux-firmware` leaves out what an x86 kernel cannot load](../../kdos/06-reference/known-gaps.md#linux-firmware-leaves-out-what-an-x86-kernel-cannot-load)
- [A UEFI firmware (capsule) update has not been applied](../../kdos/06-reference/known-gaps.md#a-uefi-firmware-capsule-update-has-not-been-applied)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [Secure Boot is not supported](../../kdos/06-reference/known-gaps.md#secure-boot-is-not-supported),
  which also leaves `fwupdx64.efi` unsigned

## See also

- [Microcode](../../kdos/03-architecture/boot-and-init.md#microcode): the early loader and what
  it requires of the archive
- [Firmware](../../kdos/02-user-guide/administration.md#firmware),
  [Microcode](../../kdos/02-user-guide/administration.md#microcode) and
  [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
- [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor)
- [The initramfs and the rescue shell](initramfs-and-rescue.md): looking inside an initramfs with
  the microcode archive in front
- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md): adding a kernel parameter
  for one boot or for every boot
- [Updating](updating.md): checking, fetching and applying an update, which installs newer
  firmware packages under `/usr/lib/firmware`

<!-- book-nav -->
---

*Part XVI. System administration, chapter 371.* Previous: [370. The initramfs and the rescue shell](initramfs-and-rescue.md) · [Contents](../README.md) · Next: [372. A/B slots and rollback](ab-slots-and-rollback.md)
