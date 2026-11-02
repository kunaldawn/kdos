# Laptops

This page gathers what a laptop needs beyond a desktop machine: the [battery]{.idx} and what happens as it
runs down, the lid, the screen's brightness and the keyboard [backlight]{.idx}, the power button and the
sleep key, the touchpad, screen rotation, docks over [Thunderbolt]{.idx}, and the power settings
that matter on battery. Suspend itself, the power profiles, TLP and thermald are on
[Power management](power-management.md), external monitors on
[Displays and scaling](displays-and-scaling.md), and locking on [Screen lock](screen-lock.md).
Inside a Plasma session PowerDevil acts on the lid, the keys and the battery; at the greeter and on
a console login elogind acts on the lid and the power key alone.

## Installation

| Port | Provides here |
|:--|:--|
| [powerdevil](#port-powerdevil) | The [lid]{.idx} and key policy, the low-battery actions, the screen and keyboard brightness, and the **Power Management** page |
| [plasma-workspace](#port-plasma-workspace) | The battery and brightness applet in the system tray, and the on-screen display for the brightness and volume keys |
| [elogind](#port-elogind) | The lid and power key outside a session, and the brightness call PowerDevil's helper uses |
| [upower](#port-upower) | The battery readings PowerDevil and the applet show |
| [libinput](#port-libinput), [plasma-desktop](#port-plasma-desktop) | Touchpad handling inside KWin, and the **Touchpad** page |
| [iio-sensor-proxy](#port-iio-sensor-proxy) | The accelerometer and light sensor, for automatic screen rotation and brightness |
| [kinfocenter](#port-kinfocenter) | The **Energy** page: charge, health, cycles and draw over time |
| [tlp](#port-tlp), [tlp-pd](#port-tlp-pd), [thermald](#port-thermald) | Power settings, the power profiles and Intel thermal control; see [Power management](power-management.md) |
| [bolt](#port-bolt), [plasma-thunderbolt](#port-plasma-thunderbolt) | [`boltd`]{.idx} and [`boltctl`]{.idx}, which authorise Thunderbolt docks, and the **Thunderbolt** page |
| [fprintd](#port-fprintd) | Fingerprint readers, for the lock screen and `sudo` |

All of them are on the image. The service scripts specific to laptops are `43_boltd`, which the
`bolt` recipe writes, and `48_iio-sensor-proxy`. `43_boltd` wakes `boltd` through the system bus
at boot when the machine has a Thunderbolt controller, and a udev rule from the same recipe wakes
it when a Thunderbolt device appears later:

<!-- kfs:include ports/core/hardware/bolt/build.sh lines=21-63 pin=6ca3544e5101 -->

## Configuration

### The lid and the keys

Inside a session, System Settings → **Power Management** sets, for each of AC power, battery and
low battery, what closing the lid does (suspend, lock, turn off the screen, nothing, and whether
an external monitor changes that), what the power button and the sleep key do, and the idle
timers. PowerDevil holds elogind's lid and key inhibitors while the session runs. Outside a
session, elogind acts on them itself, from `fs/etc/elogind/logind.conf.d/50-kdos.conf`:

<!-- kfs:include fs/etc/elogind/logind.conf.d/50-kdos.conf lines=37-43 pin=021330c2ec8d -->

So a laptop at the greeter suspends when its lid closes, unless it is docked, and the power button
powers it off.

### Brightness

The brightness keys, the slider in the battery and brightness applet and the Power Management
timers all go through PowerDevil, which changes `/sys/class/backlight/*/brightness` through
elogind's `SetBrightness` call for the active session, and the keyboard backlight under
`/sys/class/leds/*kbd_backlight*` the same way. No group or udev rule is involved.

### The battery

PowerDevil reads every supply UPower reports, leaves out a peripheral's, and acts at two levels
the Power Management page sets: a notification at the low level, and the critical action
(suspend, power off or nothing) at the critical level. The applet shows the charge and, while
charging or discharging, the time left.

### Charge thresholds

On laptops whose firmware supports it, TLP can stop charging short of 100 per cent.
`START_CHARGE_THRESH_BAT0` and `STOP_CHARGE_THRESH_BAT0` in a drop-in under [`/etc/tlp.d/`]{.idx term="/etc/tlp.d"} set the
thresholds; no file ships with them set. Which vendors TLP supports is upstream's list, in
`/usr/share/tlp/bat.d/`. The battery page's own limit setting is refused by polkit, because TLP
would set its own over it at the next profile change.

## Usage

### Reading the battery

1. The battery and brightness applet in the tray shows the charge and the time left.
2. System Settings → **Energy** (Info Center) shows the charge, the health (full-charge energy
   as a share of the design energy), the cycle count and graphs of the charge and the draw.
3. From a prompt: `upower -i "$(upower -e | grep BAT)"`, or
   `cat /sys/class/power_supply/BAT0/capacity /sys/class/power_supply/BAT0/status`. The battery's
   name may differ; `ls /sys/class/power_supply` lists them.

### Choosing what the lid does

System Settings → **Power Management**, then each power profile's **When laptop lid closed**. The
change applies at once. For the greeter and console logins, change
`HandleLidSwitch` in a drop-in of your own beside `50-kdos.conf`, for example
`/etc/elogind/logind.conf.d/60-local.conf`, and restart elogind (`sudo service restart elogind`),
which ends every session.

### Brightness

The brightness keys and the applet's slider. From a prompt in the session:

```bash
qdbus6 org.kde.Solid.PowerManagement /org/kde/Solid/PowerManagement/Actions/BrightnessControl \
    org.kde.Solid.PowerManagement.Actions.BrightnessControl.brightness
```

prints the screen's brightness; `setBrightness <value>` on the same object sets it.

### The power button and the sleep key

Power Management's **When power button pressed** offers the logout screen, shut down, suspend,
lock or nothing; the sleep key suspends. Outside a session, the power button powers the machine
off.

### Changing the touchpad

System Settings → **Touchpad**: tap to click, two-finger and edge scrolling, natural scrolling,
pointer speed and acceleration, and disable while typing, per device. KWin applies them at once.

### Screen rotation

On a convertible with an accelerometer, System Settings → **Display Configuration** offers
**Automatic** orientation for the built-in screen, which follows iio-sensor-proxy. The panel turns
with the screen.

### Authorising a Thunderbolt dock

On a laptop whose firmware sets Thunderbolt security to `user` or `secure`, a new dock is detected
and its ports stay dead until it is enrolled:

1. Plug in the dock. A notification asks whether to authorise it; or open System Settings →
   **Thunderbolt** and **Authorize** it there.
2. From a prompt: `boltctl list` shows it as not authorised, with its UUID, and
   `boltctl enroll <uuid>` enrols it; polkit asks for an administrator's password. From then on
   `boltd` authorises it each time it appears.

### Enrolling a fingerprint

1. System Settings → **Users**, select your account, **Configure Fingerprint Authentication**,
   and follow the reader's prompts; or `fprintd-enroll` from a prompt.
2. The lock screen and `sudo` then accept the finger. See
   [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md).

### Live medium and installed system

On the live medium the Power Management and Touchpad settings are in the in-memory home
directory, gone at power-off unless a persistence store exists
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)); the `/etc/tlp.d` drop-ins
and Thunderbolt enrolments are in memory too. On an installed system all of them are on disk. In a
virtual machine the screen is never dimmed or blanked on a timer.

## Tips

- Phosphor mode adds a shader pass over every change on screen; on battery, `kdos phosphor off`
  saves that.
- A battery held at its charge threshold reports *Not charging* on mains.
- The applet's **Manually block sleep and screen locking** holds off the idle actions for a
  presentation; it does not change what the lid does.
- The Wi-Fi country comes from the time zone; see
  [Wireless regulatory domain](wireless-regulatory-domain.md).

## Troubleshooting

**The brightness keys do nothing.** Check that the kernel made a backlight device:
`ls /sys/class/backlight`. An empty directory on a laptop means the graphics driver made none;
`dmesg | grep -i backlight` shows its message. On an external monitor the brightness is a DDC/CI
setting; see [Displays and scaling](displays-and-scaling.md#the-brightness-of-an-external-monitor).
If the device exists, `wev` shows what the keys send: a laptop whose keys send other keysyms is
fixed with a key binding on the Shortcuts page.

**Closing the lid does nothing.** Something holds the lid inhibitor, or the setting is **Do
nothing**. With a second screen attached, PowerDevil by default keeps running; see the Power
Management page's external-monitor option.

**The machine did not suspend at the critical level.** The critical action runs in PowerDevil,
inside a session. At a text console or at the greeter, nothing acts on the battery.

**A dock's ports are dead.** `boltctl list`; enrol the dock as in
[Authorising a Thunderbolt dock](#authorising-a-thunderbolt-dock). If `boltctl` cannot reach the
daemon, `service start boltd` wakes it. On a machine with no Thunderbolt controller,
`service status boltd` says `boltd is not running`, the boot log `/run/kdos-init.boltd.log` says
`[SKIP] boltd: no thunderbolt controller`, and `boltctl list` prints nothing.

**`fprintd-enroll` finds no reader.** `fprintd-list <user>` says `No devices available` when
libfprint drives no reader on this machine; `lsusb` shows whether one is plugged in at all.

## Known gaps

- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [32-bit UEFI is built and has never been booted](../../kdos/06-reference/known-gaps.md#32-bit-uefi-is-built-and-has-never-been-booted),
  which is what the early Atom tablets need

## See also

- [Power management](power-management.md): suspend, the profiles, TLP and thermald
- [Locking, idle and power](../../kdos/02-user-guide/desktop.md): the desktop's side
- [Displays and scaling](displays-and-scaling.md), [Screen lock](screen-lock.md),
  [Fingerprints, FIDO keys and smart cards](fingerprint-fido-smartcards.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 426.* Previous: [425. Power management](power-management.md) · [Contents](../README.md) · Next: [427. Firmware updates](firmware-updates.md)
