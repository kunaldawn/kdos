# Power management

This page covers suspending, powering off and rebooting a KDOS machine from the desktop and from
a prompt, the power profiles, the power policy TLP applies, Intel thermal management, and the
per-application energy report. A laptop's [battery]{.idx}, lid and brightness are on
[Laptops](laptops.md), and the idle timers that dim, lock and blank the screen are on
[Screen lock](screen-lock.md). [elogind]{.idx} owns the machine's power state: it suspends, powers
off and reboots for whoever polkit allows, and every program that must act around a suspend hooks
into it. Inside a Plasma session, PowerDevil sets the policy.

## Installation

| Port | Provides | Service script |
|:--|:--|:--|
| [elogind](#port-elogind) | `org.freedesktop.login1`: suspend, power-off and reboot, the inhibitors, the lid and power key outside a session, and `loginctl` | `40_elogind` |
| [powerdevil](#port-powerdevil) | The session's power policy: idle actions, the lid, power and sleep keys, brightness, and the **Power Management** page of System Settings | none; the session starts it |
| [tlp](#port-tlp) | `tlp` and `tlp-stat`: power settings applied to `/sys`, with no daemon, and its elogind sleep hook | `55_tlp` |
| [tlp-pd](#port-tlp-pd) | The power profiles service PowerDevil and the battery applet switch between, and `tlpctl` | `56_tlppd` |
| [thermald](#port-thermald) | Intel's thermal daemon | `54_thermald` |
| [kdos-energyd](#port-kdos-energyd) | The per-application energy sampler and its client [`kdos-energy`]{.idx} | `56_energyd` |
| [powertop](#port-powertop) | An interactive report of wakeups and power use, with tunables | none |
| [upower](#port-upower) | The `org.freedesktop.UPower` service PowerDevil reads batteries from | none; the system bus starts it when asked |

All of them are on the image, and the service scripts in the table are enabled on the live medium
and on an installed system alike. `54_thermald` and `56_energyd` skip themselves on a machine with
nothing for them to do; see
[Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves).

## Configuration

### elogind

KDOS's settings for elogind, from `fs/etc/elogind/logind.conf.d/50-kdos.conf`:

<!-- kfs:include fs/etc/elogind/logind.conf.d/50-kdos.conf lines=37-48 pin=1b53338a5c9c -->

The lid and power key settings apply at the greeter and at a console login; inside a Plasma
session PowerDevil holds the key inhibitors and its own settings decide. `InhibitDelayMaxSec`
gives the screen locker time to lock before the machine sleeps. Only suspend to RAM is offered,
from `fs/etc/elogind/sleep.conf.d/50-kdos.conf`:

<!-- kfs:include fs/etc/elogind/sleep.conf.d/50-kdos.conf lines=14-22 pin=9a46f7ee6d90 -->

### What runs around a suspend

Before the machine sleeps, elogind sends `PrepareForSleep`, on which the screen locker locks and
KWin blanks, and runs every program in `/usr/lib/elogind/system-sleep/` with `pre suspend`; after
waking, with `post suspend`. TLP installs its hook there, `49-tlp-sleep`, and NetworkManager
follows `PrepareForSleep` itself, built with elogind as its suspend and resume tracker.

### Who may suspend

polkit answers: the `org.freedesktop.login1` actions for suspend, power-off and reboot are
`allow_active` upstream, so the person at the machine in the active session needs no password,
and anyone else is asked for an administrator's. KDOS adds no rule for them; see
[sudo, PAM and polkit](sudo-pam-polkit.md).

### TLP

`fs/etc/init.d/55_tlp.sh` applies TLP's boot settings and its shutdown settings; TLP is not a
daemon, so nothing is supervised:

<!-- kfs:include fs/etc/init.d/55_tlp.sh -->

The recipe installs TLP without its systemd parts and with its elogind sleep hook in
`/usr/lib/elogind/system-sleep`, with its drop-in directory at `/etc/tlp.d`:

<!-- kfs:include ports/core/hardware/tlp/build.sh lines=12-19 pin=2c19e0e4a4e1 -->

Battery charge thresholds are TLP's: `/etc/polkit-1/rules.d/50-kdos.rules` refuses PowerDevil's
helper the two writes, so the battery page shows the limits TLP sets rather than one TLP would
undo; see [Laptops](laptops.md).

KDOS ships no TLP settings of its own, so upstream's defaults apply. TLP reads them in this order,
each overriding the one before:

| File | From |
|:--|:--|
| `/usr/share/tlp/defaults.conf` | TLP's built-in defaults; do not edit |
| `/etc/tlp.d/*.conf` | Drop-in files, read in name order; `00-template.conf` is upstream's example |
| [`/etc/tlp.conf`]{.idx} | Upstream's full file, every setting commented out |

### thermald

`fs/etc/init.d/54_thermald.sh` starts thermald only on an Intel processor, and treats its exit
status 2, an Intel model it does not know, as final:

<!-- kfs:include fs/etc/init.d/54_thermald.sh -->

### kdos-energyd

`fs/etc/init.d/56_energyd.sh` starts the sampler only where the processor has a readable RAPL
energy counter:

<!-- kfs:include fs/etc/init.d/56_energyd.sh lines=18-27 pin=facc77a8a745 -->

The daemon has no configuration, and its sampling interval cannot be changed. See
[kdos-energyd](../../kdos/04-programs/daemons.md#kdos-energyd).

## Usage

### Suspending, powering off and rebooting

From the desktop, the Application Launcher's Leave buttons, and `Ctrl+Alt+Del`, which opens the
logout screen with Sleep, Restart, Shut Down and Log Out. The battery and brightness applet in the
tray switches the power profile. The sleep key suspends and the power button opens the logout
screen, as System Settings → **Power Management** sets them; see
[Laptops](laptops.md).

From a prompt:

```bash
loginctl suspend
loginctl poweroff
loginctl reboot
```

`kdos-power suspend`, `poweroff` and `reboot` are the same three, run through `loginctl`. Over SSH,
where polkit asks for an administrator, `sudo loginctl poweroff` or `sudo poweroff` work as well.

Hibernation is not offered: `sleep.conf.d` turns it off, and the initramfs sets up no resume
device.

### Switching the power profile

```bash
tlpctl list               # power-saver, balanced, performance, and which is in force
tlpctl set power-saver
```

or the profile slider in the battery and brightness applet. Each switch runs `tlp power-saver`,
`tlp balanced` or `tlp performance`, so TLP's own settings stay the tuning under every profile.

### Reading what TLP has applied

1. `sudo tlp-stat -s` prints TLP's state and the power source it detected. `service status tlp`
   prints the same summary. Its last block:

   ```text
   +++ TLP Status
   tlp            = enabled, last run: 19:08:32, 108 sec(s) ago
   tlp-rdw        = enabled
   tlp-pd         = not installed
   TLP profile    = performance/AC
   Power source   = unknown
   ```

   A virtual machine reports its power source as `unknown`, and TLP then applies its mains
   profile.
2. `sudo tlp-stat -c` prints the configuration TLP is using.
3. `sudo tlp-stat -b` prints the battery's state and any charge thresholds; `sudo tlp-stat -p`
   the processor's.

### Changing a TLP setting

1. Create a drop-in, for example [`/etc/tlp.d/50-local.conf`]{.idx}, and put the settings there, one
   `KEY=value` per line. The names are those in `/etc/tlp.conf`, such as
   `CPU_ENERGY_PERF_POLICY_ON_BAT=power`.
2. Apply it: `sudo tlp start`. It answers `TLP started using profile performance/AC (auto).`, or
   the battery profile on battery.
3. Check: `sudo tlp-stat -c`. Each setting is listed with the file and line it came from, so the
   drop-in's shows as `/etc/tlp.d/50-local.conf L0001: CPU_ENERGY_PERF_POLICY_ON_BAT="power"`
   among the `defaults.conf` lines.

`sudo tlp power-saver`, `sudo tlp balanced` and `sudo tlp performance` apply one of TLP's three
profiles by hand. TLP assigns `balanced` to battery and `performance` to mains (`TLP_PROFILE_BAT`
and `TLP_PROFILE_AC`), and its default smart switching (`TLP_AUTO_SWITCH=2`) changes to the new
source's profile, when the machine moves between battery and mains, only if the profile in force
is the one assigned to the source being left; a profile chosen by hand otherwise stays.
`sudo tlp bat` and `sudo tlp ac` apply the profile assigned to battery or to mains in manual mode,
which TLP reports as `balanced/BAT (manual)`: that profile stays whatever the power source, until
the next `tlp start` or boot. `sudo tlp-stat -s` shows which profile is in force.

### Checking thermald

1. `service status thermald`. A machine that is not Intel reports `[DOWN] thermald`, and its
   boot log, `/run/kdos-init.thermald.log`, says `[SKIP] thermald: not an Intel processor`.
2. On an Intel model thermald does not support, it exits with status 2 and the supervisor leaves
   it stopped, saying so once in the system log:

   ```bash
   sudo grep thermald /var/log/messages
   ```

### Seeing which application uses the processor's power

1. `kdos-energy` prints the report: each application's share of the energy that could be
   attributed, over the time the daemon has been sampling. `kdos-energy --json` prints it as JSON.
2. System Monitor's KDOS page shows the same report, from the `ksystemstats-kdos` sensors.

The answer is a share, never watt-hours, and it covers the processor package only. Root and
members of `wheel` may read it.

### Finding wakeups with powertop

1. `sudo powertop` opens its report. The *Overview* tab lists what wakes the processor most.
2. The *Tunables* tab lists settings that are marked *Bad*. Changing one there lasts until the
   next boot, and TLP applies its own values again at boot and whenever a change between battery
   and mains changes its profile.

## Tips

- `kdos doctor` checks that elogind and [`kdos-energyd`]{.idx} are running, and says when the
  machine has no RAPL counter for `kdos-energyd` to read.
- `loginctl` with no argument lists the sessions; `systemd-inhibit` is not on the image, but
  `qdbus6 --system org.freedesktop.login1 /org/freedesktop/login1 org.freedesktop.login1.Manager.ListInhibitors`
  lists who is holding off a suspend or the lid.
- `cat /sys/power/mem_sleep` shows which suspend state `mem` means on this machine, the one in
  brackets; `mem_sleep_default=deep` on the kernel command line asks for S3 where the firmware
  offers both. See [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md).
- Most virtual machines have no RAPL counter, so `56_energyd` skips itself there, logging
  `[SKIP] kdos-energyd: no readable RAPL energy domain` in `/run/kdos-init.energyd.log`. On an AMD
  host the guest's thermald is skipped as not Intel; on most Intel hosts the guest's thermald exits
  with its status 2.
- `upower -e` lists what the UPower service sees; with no battery that is only
  `/org/freedesktop/UPower/devices/DisplayDevice`.

## Troubleshooting

**`loginctl suspend` is refused, or asks for a password.** The caller is not in the active local
session: an SSH login, or a session switched away from. Use `sudo`, or suspend from the desktop.

**Suspend does nothing.** Something holds a sleep inhibitor; list them as in [Tips](#tips). Read
elogind's log: `sudo grep elogind /var/log/messages`.

**The machine suspends but the network is gone after waking.** NetworkManager follows elogind's
`PrepareForSleep`; check that both run (`service status networkmanager`, `service status
elogind`).

**The session wakes unlocked.** The screen locker had no time to lock: check that
`InhibitDelayMaxSec` in `/etc/elogind/logind.conf.d/50-kdos.conf` is not lowered, and see
[Screen lock](screen-lock.md#troubleshooting).

**`kdos-energy` says the daemon is not running.** The message is
``kdos-energy: no kdos-energyd — `service start energyd` ``. Check `service status energyd`
(`[DOWN] kdos-energyd` when it is not running), and start it with `sudo service start energyd`.
On a machine with no `/sys/class/powercap/*/energy_uj` it cannot run.

**The fan runs hard on an Intel laptop.** Check that thermald runs (`service status thermald`)
and what TLP set for the processor (`sudo tlp-stat -p`).

## Known gaps

- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)

## See also

- [kdos-energyd](../../kdos/04-programs/daemons.md#kdos-energyd): the energy sampler
- [Locking, idle and power](../../kdos/02-user-guide/desktop.md#locking-idle-and-power)
- [Services](../../kdos/02-user-guide/administration.md#services): `40_elogind`, `54_thermald`,
  `55_tlp`, `56_tlppd` and `56_energyd`
- [Services with no daemon](../../kdos/02-user-guide/administration.md#services-with-no-daemon):
  why `service status tlp` prints TLP's summary
- [polkit and the authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-the-authentication-agent)
- [Laptops](laptops.md): the lid, the backlight and the battery readout
- [Screen lock](screen-lock.md): what locks the session and how to unlock it
- [Logs](logs.md): where each daemon's output goes and how to read it
- [Services](services.md): turning a service on or off and reading its state

<!-- book-nav -->
---

*Part XVI. System administration, chapter 425.* Previous: [424. Graphics drivers](graphics-drivers.md) · [Contents](../README.md) · Next: [426. Laptops](laptops.md)
