# Laptops

This page gathers what a laptop needs beyond a desktop machine: the [battery]{.idx} and what happens as it
runs down, the lid, the [panel]{.idx}'s brightness and the keyboard [backlight]{.idx}, the power button and the
sleep key, the touchpad, docks over [Thunderbolt]{.idx}, and the power settings that matter on battery.
Suspend itself, TLP and thermald are on [Power management](power-management.md), external
monitors on [Displays and scaling](displays-and-scaling.md), and locking on
[Screen lock](screen-lock.md). Most of what is here is done by the compositor and the panel inside
the desktop session; nothing below the session acts on the lid, the keys or the battery.

## Installation

| Port | Provides here |
|:--|:--|
| [kdos-comp](#port-kdos-comp) | The [lid]{.idx} policy, and the [key bindings]{.idx term="key binding"} for brightness, power and sleep |
| [kdos-shell](#port-kdos-shell) | The panel's battery reading and low-battery policy, and [`kdos-osd`]{.idx}, which changes the panel's brightness |
| [kdos-tools](#port-kdos-tools) | `kdos notify --battery` |
| [kdos-res](#port-kdos-res) | The Batteries page: charge, health, cycles and draw |
| [libinput](#port-libinput) | Touchpad handling inside the compositor |
| [tlp](#port-tlp), [thermald](#port-thermald) | Power settings and Intel thermal control; see [Power management](power-management.md) |
| [bolt](#port-bolt) | [`boltd`]{.idx} and [`boltctl`]{.idx}, which authorise Thunderbolt docks |
| [fprintd](#port-fprintd) | Fingerprint readers, for [`sudo`]{.idx} only |

All of them are on the image. The one service script among them that is specific to laptops is
`43_boltd`, which the `bolt` recipe writes. It wakes `boltd` through the system bus at boot when
the machine has a Thunderbolt controller, and a udev rule from the same recipe wakes it when a
Thunderbolt device appears later:

<!-- kfs:include ports/core/hardware/bolt/build.sh lines=21-63 pin=6ca3544e5101 -->

## Configuration

### The lid

What closing the lid does is a compositor setting in `~/.config/kdos/comp.conf`
(`fs/etc/skel/.config/kdos/comp.conf`):

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=226-232 pin=dcac7bde5ef9 -->

`suspend` hands the machine to `kdos-power suspend`, which locks first; `lock` locks and powers the
screens off; `off` only powers them off, for a closed laptop driving an external monitor. Opening
the lid powers the screens back on and never unlocks. See
[The lid](../../kdos/04-programs/kdos-comp.md#the-lid).

### The brightness keys

The shipped `~/.config/kdos-comp/rc.xml` (`fs/etc/skel/.config/kdos-comp/rc.xml`) binds them to
`kdos-osd`, which writes the panel's brightness and draws the gauge:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=475-476 pin=79ac4a394ad6 -->

The brightness file under `/sys/class/backlight` is root's until udev hands it to `video` at boot,
with `fs/etc/udev/rules.d/70-kdos-backlight.rules`. The same file covers the keyboard backlight:

<!-- kfs:include fs/etc/udev/rules.d/70-kdos-backlight.rules lines=27-31 pin=a30da97566e9 -->

### The power and sleep key bindings

The kernel reports the power button and the sleep key as keys, and only the compositor listens for
them; what each press does is under
[The power button and the sleep key](#the-power-button-and-the-sleep-key):

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=477-489 pin=6e4bd601eb1e -->

### The battery

The panel reads every supply under `/sys/class/power_supply`, sums the laptop's own batteries and
leaves out a peripheral's. As the battery runs down it warns once and then suspends the machine
(`src/desktop/kdos-shell/panel.c`):

<!-- kfs:include src/desktop/kdos-shell/panel.c lines=391-418 pin=ad1acd38df86 -->

There is no setting for either threshold. They act only while the panel runs, which is inside a
graphical session.

### The touchpad

The compositor uses libinput's defaults, with tap-to-click on. The shipped [`rc.xml`]{.idx} carries the
settings as a commented template:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=492-513 pin=8fd2d5fcc198 -->

### Charge thresholds

On laptops whose firmware supports it, TLP can stop charging short of 100 per cent.
`START_CHARGE_THRESH_BAT0` and `STOP_CHARGE_THRESH_BAT0` in a drop-in under [`/etc/tlp.d/`]{.idx term="/etc/tlp.d"} set the
thresholds; no file ships with them set. Which vendors TLP supports is upstream's list, in
`/usr/share/tlp/bat.d/`.

## Usage

### Reading the battery

1. The panel's battery cell shows the charge; click it for the calendar popup.
2. `Super+Ctrl+Alt+B`, or `kdos notify --battery`, shows the charge, the state and, where the kernel
   reports it, the wear, as a notification.
3. `kdos-res` and its Batteries page show the charge in watt-hours, the health (full-charge energy
   as a share of the design energy), the cycle count and the present draw.
4. From a prompt: `cat /sys/class/power_supply/BAT0/capacity /sys/class/power_supply/BAT0/status`.
   The battery's name may differ; `ls /sys/class/power_supply` lists them. On a machine whose
   kernel reports no battery, such as a virtual machine, `kdos notify --battery` says *On mains*
   when it sees a mains supply and *No battery* when it sees neither.

### Choosing what the lid does

1. Edit `~/.config/kdos/comp.conf` and set `lid_close = suspend`, `lock` or `off`.
2. Reload the compositor: `kdos-comp -r`. Settings' Session page writes the same key.
3. Check: close the lid. The compositor logs each close with the policy it applied, such as
   `lid closed — locking (lid_close = lock)`, in `$XDG_RUNTIME_DIR/kdos-comp.log`. The
   `lid_close = …` line near the top of that log is the policy the session started with, and says
   when it is the virtual-machine default; a reload does not write it again.

### Brightness

1. Press the brightness keys. Each press moves the panel by ten per cent; the lowest step is one
   per cent, never off.
2. From a prompt: `kdos-osd brightness +10` or `kdos-osd brightness -10`. `kdos-osd brightness`
   with no step shows the level.
3. Without a key binding, write the file yourself as a member of `video`:

   ```bash
   cat /sys/class/backlight/*/max_brightness
   echo 300 > /sys/class/backlight/intel_backlight/brightness
   ```

   The directory name depends on the driver: `intel_backlight`, `amdgpu_bl0`, `acpi_video0` and
   others.

The keyboard backlight has no key binding; write its file the same way, under
`/sys/class/leds/*::kbd_backlight/brightness`.

### The power button and the sleep key

1. A press of the power button opens a prompt: *Shut down the machine? Unsaved work will be lost.*
   Confirm to power off.
2. The sleep key, and a key reporting itself as [suspend]{.idx}, suspends at once.

Neither has been pressed on real hardware, and at a text login neither does anything; see
[Known gaps](#known-gaps).

### Changing the touchpad

1. Copy the `<libinput>` block from the comment in `~/.config/kdos-comp/rc.xml` out of the comment,
   keeping only the lines you want, for example `<naturalScroll>yes</naturalScroll>`.
2. Reload: `kdos-comp -r`.

The block's keys are labwc's; see
[`~/.config/kdos-comp/rc.xml`](../../kdos/06-reference/configuration.md#configkdos-comprcxml).

### Authorising a Thunderbolt dock

On a laptop whose firmware sets Thunderbolt security to `user` or `secure`, a new dock is detected
and its ports stay dead until it is enrolled:

1. Plug in the dock and list devices: `boltctl list`. The new one shows as not authorised, with
   its UUID.
2. Enrol it: `sudo boltctl enroll <uuid>`. From then on `boltd` authorises it each time it
   appears.
3. Check `service status boltd`.

`sudo` is needed because bolt's polkit rule requires an active session, which nothing on KDOS
provides; see [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates).

### Enrolling a fingerprint

1. `sudo fprintd-enroll <user>`, and follow the reader's prompts.
2. `sudo` asks for the finger first and the password after it. Nothing else does.

### Live medium and installed system

On the live medium [`comp.conf`]{.idx} and `rc.xml` changes are in the in-memory home directory, gone at
power-off unless a persistence store exists
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)); the `/etc/tlp.d` drop-ins
and Thunderbolt enrolments are in memory too. On an installed system all of them are on disk. In a
virtual machine `lid_close` defaults to `off`; see
[In a virtual machine](../../kdos/04-programs/kdos-comp.md#in-a-virtual-machine).

## Tips

- `crt = 0` in `comp.conf` turns off the phosphor pass, which costs a second swapchain per output
  and a shader run over every change on screen; on battery it is worth trying, and
  `kdos-comp -r` applies it without a new login.
- A battery held at its charge threshold reports *Not charging* on mains, and the panel does not
  count that as running down.
- `kdos toggle stay-awake` (`Super+Ctrl+I`) stops the idle timers for a presentation; it does not
  change what the lid does.
- The Wi-Fi country comes from the time zone; see
  [Wireless regulatory domain](wireless-regulatory-domain.md).

## Troubleshooting

**The brightness keys do nothing.** Run `kdos-osd brightness +10` in a terminal; it says which of
two causes it is:

- `kdos-osd: no panel in /sys/class/backlight`, followed by
  ``an external monitor is `ddcutil setvcp 10 <0-100>` `` and exit status 1, means the kernel made
  no backlight device: on a laptop, check `dmesg | grep -i backlight` for the graphics driver's
  message. On an external monitor the brightness is a DDC/CI setting; see
  [Displays and scaling](displays-and-scaling.md#the-brightness-of-an-external-monitor).
- `kdos-osd: brightness refused the write` means the file is not writable by you. `id` must list
  `video`, and `ls -l /sys/class/backlight/*/brightness` must show group `video` and mode
  `-rw-rw-r--`. If it does not, the rule did not run at boot; `sudo udevadm trigger
  --subsystem-match=backlight --action=add` runs it again.

If `kdos-osd` works from a terminal and the keys do not, the keys send other keysyms. `wev` shows
what a key sends.

**Closing the lid does nothing.** Read the compositor's log:

```bash
grep -i lid "$XDG_RUNTIME_DIR/kdos-comp.log"
```

`lid: watching switch device` names the switch it found; with no such line the compositor found
none. `lid_close = off` is also the default in a virtual machine, and the log says so:
`lid_close = off (a VM, and no lid_close in comp.conf)`.

**The machine did not suspend at 3 per cent.** The policy runs in the panel. At a text console,
with `panel = off` in `comp.conf`, or with no session at all, nothing acts on the battery.

**A dock's ports are dead.** `boltctl list`; enrol the dock as in
[Authorising a Thunderbolt dock](#authorising-a-thunderbolt-dock). If `boltctl` cannot reach the
daemon, `service start boltd` wakes it. On a machine with no Thunderbolt controller,
`service status boltd` says `boltd is not running`, the boot log `/run/kdos-init.boltd.log` says
`[SKIP] boltd: no thunderbolt controller`, and `boltctl list` prints nothing.

**`fprintd-enroll` finds no reader.** `fprintd-list <user>` says `No devices available` when
libfprint drives no reader on this machine; `lsusb` shows whether one is plugged in at all.

**The touchpad change was ignored.** The `<libinput>` block must be outside the comment and inside
`<labwc_config>`; a line the compositor could not read is logged in
`$XDG_RUNTIME_DIR/kdos-comp.log`.

## Known gaps

- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)
- [A touchpad scroll is not one-to-one with the finger](../../kdos/06-reference/known-gaps.md#a-touchpad-scroll-is-not-one-to-one-with-the-finger)
- [The control centre has no pointer page](../../kdos/06-reference/known-gaps.md#the-control-centre-has-no-pointer-page)
- [A fingerprint unlocks `sudo` and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [32-bit UEFI is built and has never been booted](../../kdos/06-reference/known-gaps.md#32-bit-uefi-is-built-and-has-never-been-booted),
  which is what the early Atom tablets need

## See also

- [Idle, dim, lock and lid](../../kdos/04-programs/kdos-comp.md#idle-dim-lock-and-lid) and
  [The lid](../../kdos/04-programs/kdos-comp.md#the-lid)
- [kdos-osd](../../kdos/04-programs/kdos-shell.md#kdos-osd)
- [Batteries](../../kdos/04-programs/kdos-res.md#batteries) and
  [kdos notify](../../kdos/04-programs/kdos-command.md#kdos-notify)
- [The device groups and the udev rules](../../kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules):
  the backlight rule
- [Thunderbolt, phones, smart cards and firmware updates](../../kdos/02-user-guide/administration.md#thunderbolt-phones-smart-cards-and-firmware-updates)
- [Power management](power-management.md): suspend, TLP and thermald
- [Screen lock](screen-lock.md): what locks the session and how to unlock it
- [Displays and scaling](displays-and-scaling.md): external monitors and their brightness
- [Microcode and firmware](microcode-and-firmware.md): CPU microcode, device firmware and `fwupd`

<!-- book-nav -->
---

*Part XVI. System administration, chapter 402.* Previous: [401. Power management](power-management.md) · [Contents](../README.md) · Next: [403. Firmware updates](firmware-updates.md)
