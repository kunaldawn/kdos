# udev and device groups

A device in `/dev` is usable without root only when two things agree: a [udev]{.idx} rule gives
the device node to a group, and the account is a member of that group. Neither works alone. This
page covers the device manager and how it starts, the groups the shipped account is in, KDOS's own
rules, modules loaded by name and the one blacklist, and the procedures for checking a device,
granting one to a group and writing a rule of your own.

## Installation

| Part | Port or file |
|:--|:--|
| The device manager, [`udevd`]{.idx} and `udevadm` | [eudev](#port-eudev) |
| Module loading, for udev and for `02_modules` | [kmod](#port-kmod) |
| Starting `udevd` and replaying the devices present at boot | `fs/etc/init.d/01_udev.sh` |
| Loading the modules no device asks for | `fs/etc/init.d/02_modules.sh` |
| KDOS's rules, modules and blacklist | `fs/etc/udev/rules.d`, `fs/etc/modules-load.d`, `fs/etc/modprobe.d` |
| The groups and the `kdos` account's memberships | `fs/etc/group` |

All of it is in every image, from the base filesystem (see
[The fs/ overlay](../01-preparing/fs-overlay.md)). The packages' own rules are installed in
[`/lib/udev/rules.d`]{.idx}, the same directory as `/usr/lib/udev/rules.d`. The hardware database is
compiled once over the finished tree, into `/etc/udev/hwdb.bin`, by the image step
[`060_udev_hwdb.sh`](../15-image/060-udev-hwdb.md); after that, kpkg recompiles it whenever a
package that installs a `.hwdb` file is added or removed (see
[Shared indexes](../../kdos/05-developer/writing-ports.md#shared-indexes)).

## Configuration

### 01_udev

`udevd` is started by the first service script. Before starting it, the script creates the nodes
of the modules that load only when their node is opened, `/dev/uhid`, `/dev/uinput`,
`/dev/snd/seq` and the rest; after it, it replays every device already present with the action
`add`, which is what loads their drivers:

<!-- kfs:include fs/etc/init.d/01_udev.sh -->

The service is named `udev`. It leaves no supervised process: `udevd` runs on its own once the
script has started it (`/usr/sbin/udevd --daemon`), and `service status udev` reports whether it is
running, as `[ OK ] udev is running`. What the script printed at boot is in
`/run/kdos-init.udev.log`: `Starting udev...`, then `udev ready`.

### The groups

The shipped [`/etc/group`]{.idx}. The `kdos` account is in `wheel` and `seat`, which administer and run the
machine, in `users`, and in the hardware groups `tty`, `dialout`, `audio`, `video`, `cdrom`,
`render`, `input`, `kvm` and `lpadmin`:

<!-- kfs:include fs/etc/group -->

On the image `id kdos` lists one group more, `brlapi`, which the brltty package creates and adds
the account to when it is installed, so that a screen reader can reach the braille display server.

What each hardware group makes usable, and what removing it stops, is in
[Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups).

### /etc/udev/rules.d

KDOS's rules are the `70-kdos-*.rules` files here, one per family of device. Each grants a
device node to a group with mode 0660. One more file, `99-qemu-tablet.rules`, grants nothing: it
marks QEMU's emulated USB tablet as a pointer. The serial rule is the shape of most of them:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-serial.rules -->

The complete list, with the group each file grants and the devices it covers, is in
[The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).
A rule of your own goes in this directory too; a file here with the same name as one in
`/lib/udev/rules.d` replaces the packaged file.

### /etc/modules-load.d and 02_modules

A driver that declares nothing udev can match is loaded by name. `02_modules` reads every `.conf`
file in [`/etc/modules-load.d`]{.idx}, one module per line, and only that directory:

<!-- kfs:include fs/etc/init.d/02_modules.sh -->

One of the shipped files, for the I2C character devices that `ddcutil` needs:

<!-- kfs:include fs/etc/modules-load.d/kdos-i2c.conf -->

### /etc/modprobe.d

One blacklist ships, which keeps the kernel's TV-tuner driver off an RTL2832U radio stick:

<!-- kfs:include fs/etc/modprobe.d/kdos-sdr.conf -->

Packages add files of their own beside it, none of them a blacklist: on the image they are
`stlink_v1.conf`, a `usb-storage` quirk for ST-Link programmers, and `nvdimm-security.conf`, which
loads NVDIMM keys before `libnvdimm`.

## Usage

### Checking why a device cannot be opened

1. Look at the node's owner, group and mode:

   ```bash
   ls -l /dev/ttyUSB0
   ```

2. Check your own groups:

   ```bash
   id
   ```

3. Show what udev knows about the device, and every attribute a rule can match on it and on each
   parent device:

   ```bash
   udevadm info /dev/ttyUSB0
   udevadm info -a /dev/ttyUSB0
   ```

4. `kdos doctor` checks each serial, video-capture and USBTMC node that is present
   (`/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/video*`, `/dev/usbtmc*`), lists the ones you cannot open
   and names the group that owns each:

   ```bash
   kdos doctor
   ```

### Adding an account to a group

1. Add the group to the account's list:

   ```bash
   sudo usermod -aG dialout alice
   ```

2. Log out and in again. Group membership is read at login, so a running session keeps the groups
   it started with.

### Watching devices arrive

```bash
udevadm monitor --udev --property
```

prints every event udev finishes processing, with its properties, as a device is plugged in or
removed. `Ctrl+C` stops it.

### Writing a rule

1. Find the device's identifiers with `udevadm info -a`, for example its USB `idVendor` and
   `idProduct`.
2. Write a file in `/etc/udev/rules.d`, with a number after the files it should follow, for
   example [`/etc/udev/rules.d/71-local.rules`]{.idx}:

   ```text
   SUBSYSTEM=="usb", ATTR{idVendor}=="1234", ATTR{idProduct}=="5678", GROUP="dialout", MODE="0660"
   ```

3. Load the rules and replay the devices of that subsystem:

   ```bash
   sudo udevadm control --reload
   sudo udevadm trigger --action=add --subsystem-match=usb
   ```

   Unplugging the device and plugging it in again does the same for that device.

4. Check the node with `ls -l`, as above.

### Loading a module at boot

1. Write the module's name in a file of `/etc/modules-load.d`, for example
   `/etc/modules-load.d/local.conf`.
2. Load it for this boot, or wait for the next:

   ```bash
   sudo modprobe <module>
   ```

   At boot, `02_modules` logs `loaded:` or `skip:` for each, in `/run/kdos-init.modules.log`.

### Keeping a driver off a device

Write `blacklist <module>` in a file of `/etc/modprobe.d`, then unload the module
(`sudo modprobe -r <module>`) or reboot.

### Adding to the hardware database

Put a `.hwdb` file in [`/etc/udev/hwdb.d`]{.idx}, then compile the database and replay the devices:

```bash
sudo udevadm hwdb --update
sudo udevadm trigger --action=add
```

## Tips

- Use only the groups that exist in `/etc/group`. udev turns a group it does not know into root:
  it logs `specified group '…' unknown` in `/var/log/messages` when it reads the rules, and the
  node still appears with the rule's mode, owned by `root:root`. A rule copied from elsewhere that names `plugdev` therefore gives the device to
  root. The upstream rules that grant through `plugdev` or `uaccess` are not installed for that
  reason.
- A device with no node in `/dev`, such as a [backlight]{.idx}, cannot take `GROUP=` or `MODE=`: eudev
  skips the whole line for it, `RUN+=` included, and reports nothing. The shipped backlight rule sets
  the permission with `chgrp` and `chmod` instead, in
  [`/etc/udev/rules.d/70-kdos-backlight.rules`]{.idx}.
- Match `ACTION=="add"` in a rule that should also run for devices present at boot: `01_udev`
  replays them with the action `add`, never `change`. `kdos explain coldplug` prints the reason.
- `kdos why /etc/udev/rules.d/70-kdos-serial.rules` says what provides a file; a rule from the
  base filesystem is reported as owned by no package.
- `sudo udevadm control --log-priority=debug` raises the running `udevd`'s log level, so it logs
  each rule it applies. The level lasts until `udevd` is restarted, or until you set another with
  `--log-priority=`.
- On the live medium, a rule, a group membership or a module file you add is in memory and is lost
  at power-off unless a persistence store exists
  ([`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist)). On an installed system it
  is on the root filesystem.

## Troubleshooting

**`Permission denied` opening a device, and `ls -l` shows the right group.** Your session predates
the membership. `id` lists the groups the current session has; log out and in again.

**The node belongs to `root:root` despite a rule.** The group the rule names does not exist, or the
rule did not match. `udevadm info -a` shows the attributes the device really has; compare them with
the rule's keys, reload, and trigger again.

**A device present at boot has no driver loaded.** Read what the coldplug printed:

```bash
cat /run/kdos-init.udev.log
grep udevd /var/log/messages
```

and check the module by hand with `sudo modprobe <module>` and `dmesg`. The system log holds
`udevd`'s own complaints, such as `failed to execute '…'` for a rule's program. The lines of that
kind logged in the first second of the boot, before the second `starting eudev` line, come from the
initramfs's `udevd`, which reads rules whose helper programs it does not carry; they are expected.

**A serial programmer, a board or an instrument looks like a broken cable.** That is the missing
group, not the cable. Run `kdos doctor`; it names the group that owns each node you cannot open.

**An RTL2832U stick is listed by `lsusb` and no SDR program can open it.** The TV-tuner driver has
claimed it. Check `lsmod | grep dvb_usb_rtl28xxu`; the shipped blacklist keeps it off, so a line
there means the module was loaded by hand or the blacklist was removed.

**`/dev/i2c-*` does not exist.** `i2c-dev` is loaded only by `02_modules`, from
`/etc/modules-load.d/kdos-i2c.conf`. Check `/run/kdos-init.modules.log`, or load it with
`sudo modprobe i2c-dev`.

**A keyboard or mouse added through Bluetooth or `uinput` gives no input.** The node is made by
`01_udev` before `udevd` starts. Check `ls -l /dev/uhid /dev/uinput`: both exist, owned by root with
mode 0600, since the programs that open them (`bluetoothd`, an input remapper) run as root; see
[Bluetooth](bluetooth.md#troubleshooting).

## Known gaps

- [LVM volume groups are activated only at boot](../../kdos/06-reference/known-gaps.md#lvm-volume-groups-are-activated-only-at-boot):
  lvm2's hotplug rule is not installed
- [`lsblk` shows a filesystem's type, label and UUID to root only](../../kdos/06-reference/known-gaps.md#lsblk-shows-a-filesystems-type-label-and-uuid-to-root-only)
- [A SPICE viewer redirects only the USB devices you can already open](../../kdos/06-reference/known-gaps.md#a-spice-viewer-redirects-only-the-usb-devices-you-can-already-open)
- [Smart cards, drawing tablets and software radios have not met hardware](../../kdos/06-reference/known-gaps.md#smart-cards-drawing-tablets-and-software-radios-have-not-met-hardware):
  their rules are written and not tried against the devices
- [A phone is mounted by hand](../../kdos/06-reference/known-gaps.md#a-phone-is-mounted-by-hand)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)

## See also

- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules): every rule file, the narrow ones, and the modules loaded by name
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups): what each group grants, and adding an account
- [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts): where `01_udev` and `02_modules` run
- [Databases stamped into the image](../../kdos/05-developer/build-system.md#databases-stamped-into-the-image): the hardware database
- [The udev hardware database](../15-image/060-udev-hwdb.md): the image step that compiles it
- [Users and groups](users-and-groups.md), [Modules and sysctl](modules-and-sysctl.md),
  [Live persistence](live-persistence.md), [Printing and scanning](printing-and-scanning.md),
  [Bluetooth](bluetooth.md), [Audio](audio.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 395.* Previous: [394. Avahi and mDNS](avahi-and-mdns.md) · [Contents](../README.md) · Next: [396. Bluetooth](bluetooth.md)
