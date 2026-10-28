# Bluetooth

This page covers [Bluetooth]{.idx} on KDOS: the daemon and how it is started, pairing and
connecting devices from the desktop and from a terminal, Bluetooth [audio]{.idx}, and what to check when an
adapter or a device does not appear. The sound a headset makes once it is connected is set by
[PipeWire]{.idx}, on [Audio](audio.md).

## Installation

The [bluez](#port-bluez) port provides the daemon, [`bluetoothd`]{.idx}, installed in `/usr/lib/bluetooth`
with a link in `/usr/sbin`, and the tools [`bluetoothctl`]{.idx} and `btmon`:

<!-- kfs:include ports/core/hardware/bluez/build.sh lines=30-31 pin=b8a0ba625b0d -->

It also installs `obexd`, the object-exchange daemon for file transfer, beside `bluetoothd` in
`/usr/lib/bluetooth` and not on the `PATH`. The session bus starts it when a program asks for
`org.bluez.obex` (`/usr/share/dbus-1/services/org.bluez.obex.service`); no service script runs it.

The desktop's pairing window, [`kdos-bt`]{.idx}, and the Bluetooth pane of [`kdos-audio`]{.idx} are part of
[kdos-shell](#port-kdos-shell). The audio side, the Bluetooth plugin and its codecs, is
[pipewire](#port-pipewire)'s, with [wireplumber](#port-wireplumber) choosing the profile. Adapter
firmware comes from [linux-firmware](#port-linux-firmware).

The service script is part of the base filesystem, `fs/etc/init.d/60_bluetooth.sh`. The service is
named `bluetooth`, and its supervised process `bluetoothd`. It is on by default on both the live
medium and an installed system. The installer lists it among its service choices, on unless you
turn it off:

<!-- kfs:include src/system/kdos-installer/conf.c lines=23-29 pin=9a2600b3e3b2 -->

## Configuration

### The service script

`bluetoothd` is started with flags rather than a configuration file of KDOS's own. `-n` keeps it in
the foreground for the supervisor; `-E` and the `--kernel=` feature turn on the experimental
interfaces and the kernel's ISO sockets that LE Audio needs:

<!-- kfs:include fs/etc/init.d/60_bluetooth.sh -->

Why LE Audio needs both is in [Audio](../../kdos/03-architecture/session.md#audio) in the KDOS
book.

### /etc/bluetooth/

`main.conf`, `input.conf` and `network.conf` are upstream's files as the bluez package installs
them, with every setting commented out (only the section headers, `[General]`, `[Policy]` and the
rest, are live lines), so the daemon runs on its built-in defaults. Their keys are
described in the comments of each file. After an edit:

```bash
sudo service restart bluetooth
```

### Who may use Bluetooth

No group is needed. bluez's D-Bus policy, `/usr/share/dbus-1/system.d/bluetooth.conf`, lets any
account send to `org.bluez`; only root may own the name.

### /dev/uhid

A Bluetooth keyboard or mouse reaches the kernel's input layer through `/dev/uhid`, whose driver
loads only when the node is opened. `01_udev` creates that node, with the other on-demand
nodes, before `udevd` starts:

<!-- kfs:include fs/etc/init.d/01_udev.sh lines=16-32 pin=33f7d1ca34ae -->

### Audio codecs

PipeWire's Bluetooth plugin is built with these codecs:

<!-- kfs:include ports/core/audio-io/pipewire/build.sh lines=30-39 pin=c7adc7c9c594 -->

LC3plus, LDAC decoding and spandsp's packet-loss concealment are off because their libraries are
not ports; see
[Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port).

## Usage

### Pairing a device from the desktop

1. Open `kdos-bt` with `Super+F5`, or choose *Bluetooth…* on the Hardware page of Settings. The top
   row shows the adapter and whether it is powered.
2. If the adapter is off, press `o` to power it on.
3. Put the device into pairing mode, then press `s` to scan. Devices appear as they are found.
4. Select the device with the arrow keys and press `p` to pair. When the device asks for a passkey
   to be confirmed, the question takes the bottom row: press `y` (or `Enter`) to confirm or `n` to
   refuse. A keyboard, which has no screen to show a passkey on, gets a number instead, as
   `type 123456 on <device>` on the bottom row: type it on that keyboard and press its `Enter`.
5. Press `t` to trust the device, and `Enter` to connect.
6. Press `s` again to stop scanning.

`x` removes a paired device and `d` makes this machine visible to others, or hides it again. `Esc`
refuses a question on the bottom row, and with none there closes the window. `F1` opens the window's help
page, which is also `kdos-doc bt`.

`kdos-bt` cannot pair a device that wants a PIN typed on this machine, an old device without
secure simple pairing: it refuses the request and its bottom row reads
`this device needs a typed PIN — pair it with bluetoothctl`. Pair such a device
[from a terminal](#pairing-from-a-terminal).

### Connecting a headset

1. Pair it once, as above.
2. Open `kdos-audio` with `Super+F3` and press `Tab` for its Bluetooth pane. `Enter` connects or
   disconnects the selected device.
3. In the Outputs pane, select the headset's sink and press `Enter` to make it the default.

### Pairing from a terminal

`bluetoothctl` speaks to the same daemon and registers a pairing agent of its own:

```bash
bluetoothctl
```

Then, at its prompt:

```text
power on
agent on
default-agent
scan on
pair AA:BB:CC:DD:EE:FF
trust AA:BB:CC:DD:EE:FF
connect AA:BB:CC:DD:EE:FF
scan off
quit
```

`devices` lists what the adapter knows, and `info <address>` one device's state. Each command can
also be given on the command line, as `bluetoothctl connect AA:BB:CC:DD:EE:FF`.

### Turning Bluetooth off

For this boot only, block the radio or stop the daemon:

```bash
sudo rfkill block bluetooth
sudo service stop bluetooth
```

To keep the daemon from starting at boot, `sudo service disable bluetooth`. On an installed system
the installer has already done this if Bluetooth was turned off there.

## Tips

- While `kdos-bt` is open, an untrusted device that asks to use a service is put to you on the
  bottom row (`y` or `n`); a trusted one is allowed without asking. Trust the devices you use
  every day.
- Which profile a headset uses, A2DP for music or HFP for a call, is WirePlumber's choice and not
  the pairing window's. `wpctl status` lists the headset's nodes once it is connected.
- `sudo btmon` prints every packet between the daemon and the adapter, which shows where a pairing
  stops.
- TLP runs at every boot, from `55_tlp`. When `DEVICES_TO_DISABLE_ON_STARTUP` names `bluetooth`,
  in `/etc/tlp.conf` or a file under `/etc/tlp.d/`, the radio is blocked at boot, and so it is
  with `RESTORE_DEVICE_STATE_ON_STARTUP=1` when it was off at the last shutdown. As shipped neither
  is set. See [Power management](power-management.md#tlp).
- Pairing keys are kept under `/var/lib/bluetooth`. On the live medium they are lost at power-off
  unless a persistence store exists ([Live persistence](live-persistence.md)).

## Troubleshooting

**`kdos-bt` shows no adapter.** Its top row then reads `no bluetooth adapter on this machine`, as
does the Bluetooth pane of `kdos-audio`, and `bluetoothctl show` says
`No default controller available`. A pane that reads `not running — service start bluetooth`
instead could not reach the daemon at all. Work down the chain:

1. Is the daemon running?

   ```bash
   service status bluetooth
   ls /etc/service.disabled
   ```

   A running daemon reports `[ OK ] bluetoothd  (pid N)`. A file named `bluetooth` in
   `/etc/service.disabled` keeps it from starting at boot; `sudo service enable bluetooth` and
   `sudo service start bluetooth`.

2. Is the radio blocked?

   ```bash
   rfkill list
   sudo rfkill unblock bluetooth
   ```

   `rfkill list` prints nothing at all when the kernel has no radio of any kind, which is the
   case in a virtual machine.

3. Did the kernel find the adapter and load its firmware?

   ```bash
   sudo dmesg | grep -i bluetooth
   ```

   The kernel's log is root's to read, so `dmesg` without `sudo` prints only an error.

   The `Bluetooth: Core ver …`, `HCI` and `BNEP` lines are the protocol stack loading and appear
   with no adapter too; an adapter adds lines naming `hci0`. A line naming a firmware file that
   could not be loaded points at [linux-firmware](#port-linux-firmware).

**The daemon keeps restarting.** `service status bluetooth` then reports a
`[FAIL] bluetoothd` line with the daemon's last exit code. The daemon's own messages are in the
supervisor's log, which only root can read, and in syslog's `/var/log/messages`:

```bash
sudo tail -n 40 /run/kdos-svc.bluetoothd.log
```

A clean start logs `Bluetooth daemon <version>`, `Starting SDP server` and
`Bluetooth management interface <version> initialized`.

**A keyboard will not pair.** Pairing a keyboard needs an agent, a program that answers the
daemon's passkey questions, and the daemon refuses the pairing when none answers. `kdos-bt`
registers one while it is open, so keep its window open until the pairing finishes, and type the
number its bottom row shows on the keyboard itself. A keyboard that wants a PIN typed on this
machine is refused by `kdos-bt`; pair it from `bluetoothctl` after `agent on` and `default-agent`.

**A keyboard or mouse pairs and connects, and does nothing.** Check that the HID node exists:

```bash
ls -l /dev/uhid
```

If it is missing, `01_udev` did not create it at boot; read `/run/kdos-init.udev.log`.

**A headset is connected and plays nothing.** PipeWire runs only inside a desktop session, so a
headset is a sound device only there. In the session, check that its sink exists and is the
default (`wpctl status`), and see [Audio](audio.md).

**An LE Audio headset or hearing aid falls back to classic audio, or does not connect.** Check that
the daemon has both LE Audio flags:

```bash
tr '\0' ' ' < /proc/$(pgrep -x bluetoothd)/cmdline; echo
```

The line must carry `-E` and `--kernel=6fbaf188-05e0-496a-9885-d6ddfdb4e03e`.

## Known gaps

- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal):
  no range of adapters and devices has been tested. The procedures for pairing, connecting and
  playing to a headset need a radio, which the test rig's virtual machine does not have; there they
  reach only the daemon with no adapter
- [A USB modem that first appears as a storage device is not switched into a modem](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem):
  Bluetooth tethering is built and has not been connected on hardware

## See also

- [Bluetooth](../../kdos/02-user-guide/administration.md#bluetooth): the user guide's section
- [kdos-bt](../../kdos/04-programs/kdos-shell.md#kdos-bt) and [kdos-audio](../../kdos/04-programs/kdos-shell.md#kdos-audio): the two windows
- [Audio](../../kdos/03-architecture/session.md#audio): PipeWire, the codecs and LE Audio
- [Audio](audio.md), [udev and device groups](udev-and-device-groups.md),
  [Power management](power-management.md), [Microcode and firmware](microcode-and-firmware.md),
  [Logs](logs.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 396.* Previous: [395. udev and device groups](udev-and-device-groups.md) · [Contents](../README.md) · Next: [397. Printing and scanning](printing-and-scanning.md)
