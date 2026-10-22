# Modules and sysctl

This page covers the two ways of changing the running kernel without rebuilding it: loading
[kernel modules]{.idx term="kernel module"} and passing them options, and setting kernel parameters through
[sysctl]{.idx}. It says which files KDOS reads for each at boot, how to make a change for this boot
only or for every boot, and how to find out why a driver did not load. Building a different kernel
is in [The kernel](../14-kernel/intro.md). Every command on this page runs on a running KDOS machine.

## Installation

- [linux](#port-linux) installs the kernel and its modules under `/lib/modules/<version>`. Which
  drivers are modules and which are built in is set by the port's `kdos.config`.
- [kmod](#port-kmod) installs `kmod`, with [`modprobe`]{.idx}, `insmod`, `rmmod`, `lsmod`, [`modinfo`]{.idx} and
  `depmod` as links to it, and the manual pages `modprobe(8)` and `modprobe.d(5)`.
- [eudev](#port-eudev) loads the driver for each device it sees, through libkmod.
- [procps-ng](#port-procps-ng) installs `sysctl` and the manual pages `sysctl(8)` and
  `sysctl.conf(5)`.
- The base filesystem supplies the boot scripts that apply both at boot, `01_udev.sh`,
  `02_modules.sh` and `10_sysctl.sh`, and the files they read.
- Ports add module configuration: [hwdata](#port-hwdata) installs
  `/usr/lib/modprobe.d/dist-blacklist.conf`, [stlink](#port-stlink) installs
  [`/etc/modprobe.d/stlink_v1.conf`]{.idx}, and [ndctl](#port-ndctl) installs upstream's
  `/etc/modprobe.d/nvdimm-security.conf`, which loads the NVDIMM keys before `libnvdimm`.
- The installer, and `kdos-power timezone` on a running system, write
  `/etc/modprobe.d/kdos-regdom.conf`, the Wi-Fi country.

Every service involved is enabled on every image. On the live medium a file added under `/etc` is
kept only when a persistence store exists (`kdos persist`).

## Configuration

### /etc/modules-load.d/

`02_modules.sh` loads every module named in `/etc/modules-load.d/*.conf`, one name per line, `#`
starting a comment. It reads no other directory: a file under `/usr/lib/modules-load.d` or
`/run/modules-load.d` is not read. The script:

<!-- kfs:include fs/etc/init.d/02_modules.sh -->

The four files the image ships, each with its reason:

<!-- kfs:include fs/etc/modules-load.d/kdos-containers.conf -->

<!-- kfs:include fs/etc/modules-load.d/kdos-drm.conf -->

<!-- kfs:include fs/etc/modules-load.d/kdos-hwmon.conf -->

<!-- kfs:include fs/etc/modules-load.d/kdos-i2c.conf -->

### /etc/modprobe.d/

`modprobe` reads module options, aliases and blacklists from every `*.conf` file in these
directories, in this order, the first of two files with the same name winning: [`/etc/modprobe.d`]{.idx},
`/run/modprobe.d`, `/usr/local/lib/modprobe.d`, [`/usr/lib/modprobe.d`]{.idx} and `/lib/modprobe.d`, which
is the same directory as `/usr/lib/modprobe.d` on KDOS. The format is `modprobe.d(5)`. The base
filesystem ships one file:

<!-- kfs:include fs/etc/modprobe.d/kdos-sdr.conf -->

The [hwdata](#port-hwdata) port installs upstream's blacklist of drivers that are not to load on
their own:

<!-- kfs:include ports/core/hardware/hwdata/build.sh lines=12-15 pin=cc44ca8f7308 -->

The installer's file is written from the time zone's country:

<!-- kfs:include src/system/kdos-installer/install.c lines=1694-1702 pin=5a0dbdc98892 -->

### Udev and coldplug

`01_udev.sh` starts `udevd` and replays every device already present, which is what loads the
drivers for the hardware the machine boots with:

<!-- kfs:include fs/etc/init.d/01_udev.sh lines=33-43 pin=bcc339581ee6 -->

### /etc/sysctl.conf

`10_sysctl.sh` applies [`/etc/sysctl.conf`]{.idx} with `sysctl -p` at boot, and reads no [`/etc/sysctl.d`]{.idx}.
The file, with the reason for each setting:

<!-- kfs:include fs/etc/sysctl.conf -->

What each setting changes is also listed in
[Services](../../kdos/02-user-guide/administration.md#services), under `10_sysctl`.

One more parameter is set at boot outside this file: `12_zram.sh` sets `vm.page-cluster` to 0 while
the compressed swap device is active, and back to the kernel's default when it stops; see
[/etc/kdos/zram.conf](../../kdos/06-reference/configuration.md#etckdoszramconf).

## Usage

### Looking at modules

1. List the loaded modules:

   ```bash
   lsmod
   ```

2. Show what a module is, its options and its firmware:

   ```bash
   modinfo i2c-dev
   ```

### Loading a module

1. Load it now, with what it depends on. `snd-aloop`, the ALSA loopback card, is a module KDOS
   builds and does not load:

   ```bash
   sudo modprobe -v snd-aloop
   ```

2. Unload it:

   ```bash
   sudo modprobe -r snd-aloop
   ```

3. Load it at every boot: write its name on a line of a new file, such as
   `/etc/modules-load.d/local.conf`, and check the file with the script that reads it:

   ```bash
   sudo /etc/init.d/02_modules.sh start
   ```

   It prints a line for every name in every file, such as `[KDOS]   loaded: snd-aloop`, and
   `[KDOS]   skip:   nct6775 (no matching hardware)` for one that did not load. Running it again
   is harmless: a module already loaded is reported `loaded:` once more.

### Passing a module options

1. Write an `options` line into a file under `/etc/modprobe.d/`, such as
   [`/etc/modprobe.d/local.conf`]{.idx}:

   ```text
   options <module> <parameter>=<value>
   ```

2. The options apply the next time the module loads. For a module that is loaded and not in use,
   reload it:

   ```bash
   sudo modprobe -r <module>
   sudo modprobe <module>
   ```

3. Check the value the module took, where it exports one:

   ```bash
   cat /sys/module/<module>/parameters/<parameter>
   ```

A module the initramfs loads, such as a filesystem or a disk controller driver, loads before the
root filesystem is mounted, and the initramfs carries no `/etc/modprobe.d`. Such a module takes its
options from the [kernel command line]{.idx} instead, as `<module>.<parameter>=<value>`; `modprobe` reads
those from `/proc/cmdline` too. How to add a word to the command line is in
[Limine and the kernel command line](../../kdos/03-architecture/boot-and-init.md#limine-and-the-kernel-command-line).

### Keeping a module from loading

1. Write a `blacklist` line, as `kdos-sdr.conf` does, into a file under `/etc/modprobe.d/`:

   ```text
   blacklist <module>
   ```

   This stops the module loading for a device; `modprobe <module>` by name still loads it.

2. For one boot, add `modprobe.blacklist=<module>` to the kernel command line in the boot menu's
   entry editor (`E` on an entry).

### Reading and setting a kernel parameter

1. Read one, or all:

   ```bash
   sysctl kernel.kptr_restrict
   sysctl -a
   ```

2. Set one for this boot:

   ```bash
   sudo sysctl -w vm.swappiness=20
   ```

3. Set it at every boot: add the line `vm.swappiness = 20` to `/etc/sysctl.conf`, then apply the
   file:

   ```bash
   sudo sysctl -p /etc/sysctl.conf
   ```

   `sudo service start sysctl` runs the same command.

## Tips

- `kdos why /etc/sysctl.conf` reports that no package owns the file. It comes from the base
  filesystem, so no package upgrade replaces an edit to it.
- `kdos explain coldplug` prints why `01_udev.sh` replays devices with `--action=add`.
- `02_modules.sh` discards `modprobe`'s error and prints `skip: <name> (no matching hardware)` for
  every failure. When a listed module should have loaded, run `sudo modprobe -v <name>` to see the
  real error.
- The kernel's modules are compressed with zstd; `modinfo` and `modprobe` read them as they are.
- Every key is a file under `/proc/sys`, with the dots as slashes: `sysctl -w vm.swappiness=20` is
  the same as writing `20` to `/proc/sys/vm/swappiness`.

## Troubleshooting

**A device is present but has no driver.** Check whether the kernel has a module for it:

```bash
lspci -k
sudo dmesg | tail -n 30
```

`lspci -k` names the module each PCI device can use and the one in use. If a module is named but
not loaded, `sudo modprobe -v <module>` shows why. A module that declares no modalias, such as the
sensor drivers in `kdos-hwmon.conf`, is never loaded for a device and has to be listed in
[`/etc/modules-load.d`]{.idx}.

**Coldplug loaded nothing at boot: no sound card, no network.** Replay the devices as `01_udev.sh`
does:

```bash
sudo udevadm trigger --action=add --type=subsystems
sudo udevadm trigger --action=add --type=devices
sudo udevadm settle
```

A plain `udevadm trigger` replays them as `change`, and the rule that loads drivers acts only on
`add`.

**A module refuses an option.** The kernel log says which parameter it did not know:
`sudo dmesg | grep <module>`. `modinfo -p <module>` lists the parameters it takes.

**A setting in `/etc/sysctl.conf` is not in effect after boot.** Read what `sysctl -p` printed:

```bash
cat /run/kdos-init.sysctl.log
```

A key the running kernel lacks is reported there. A setting a later script or program changes, as
`12_zram.sh` does for `vm.page-cluster`, wins over the file.

## Known gaps

- Root can load an unsigned module; see [Security](../../kdos/06-reference/known-gaps.md#security)
- [linux-firmware leaves out what an x86 kernel cannot load](../../kdos/06-reference/known-gaps.md#linux-firmware-leaves-out-what-an-x86-kernel-cannot-load)
- [Packet radio has no kernel network interface](../../kdos/06-reference/known-gaps.md#packet-radio-has-no-kernel-network-interface)
- [btop's GPU panel is empty on Intel graphics unless btop runs as root](../../kdos/06-reference/known-gaps.md#btops-gpu-panel-is-empty-on-intel-graphics-unless-btop-runs-as-root):
  `kernel.perf_event_paranoid` stays at the kernel's default

## See also

- [Services](../../kdos/02-user-guide/administration.md#services): `01_udev`, `02_modules` and `10_sysctl` in the boot order
- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules): who may open which device
- [Other machine files](../../kdos/06-reference/configuration.md#other-machine-files): `/etc/sysctl.conf`, `/etc/modules-load.d` and `/etc/modprobe.d`
- [The initramfs](../../kdos/03-architecture/boot-and-init.md#the-initramfs): which modules load before the root is mounted
- [Firmware](../../kdos/02-user-guide/administration.md#firmware): the firmware files drivers load
- [Services](services.md), [Init and runlevels](init-and-runlevels.md), [Logs](logs.md),
  [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md),
  [Microcode and firmware](microcode-and-firmware.md), [zram and swap](zram-and-swap.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 363.* Previous: [362. Logs](logs.md) · [Contents](../README.md) · Next: [364. zram and swap](zram-and-swap.md)
