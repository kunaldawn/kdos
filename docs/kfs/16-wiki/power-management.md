# Power management

This page covers suspending, powering off and rebooting a KDOS machine from the desktop and from
a prompt, the power policy TLP applies, Intel thermal management, and the per-application energy
report. A laptop's [battery]{.idx}, lid and brightness are on [Laptops](laptops.md), and the idle timers
that dim, lock and blank the screen are on [Screen lock](screen-lock.md). KDOS has no logind and
no systemd, so one root daemon, [kdos-powerd]{.idx}, stands between the desktop and
`/sys/power/state`; why, and who may ask it for what, is in
[kdos-powerd](../../kdos/04-programs/daemons.md#kdos-powerd).

## Installation

| Port | Provides | Service script |
|:--|:--|:--|
| [kdos-powerd](#port-kdos-powerd) | The daemon `/usr/sbin/kdos-powerd` and its client `/usr/bin/kdos-power` | `55_powerd` |
| [tlp](#port-tlp) | `tlp` and `tlp-stat`: power settings applied to `/sys`, with no daemon | `55_tlp` |
| [thermald](#port-thermald) | Intel's thermal daemon | `54_thermald` |
| [kdos-energyd](#port-kdos-energyd) | The per-application energy sampler and its client [`kdos-energy`]{.idx} | `56_energyd` |
| [powertop](#port-powertop) | An interactive report of wakeups and power use, with tunables | none |
| [upower](#port-upower) | The `org.freedesktop.UPower` service, for applications that ask the system bus about batteries | none; the system bus starts it when asked |

All of them are on the image, and the service scripts in the table are enabled on the live medium
and on an installed system alike. `54_thermald` and `56_energyd` skip themselves on a machine with
nothing for them to do; see
[Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves).

KDOS's own panel and battery notices read `/sys/class/power_supply` directly and do not ask
`upower`.

## Configuration

### /etc/init.d/55_powerd.sh

`fs/etc/init.d/55_powerd.sh` starts the daemon under the supervisor:

<!-- kfs:include fs/etc/init.d/55_powerd.sh -->

The daemon has no configuration file. It admits `root`, `seat` and `wheel` to `suspend`, `poweroff`
and `reboot`; see [Who may ask for what](../../kdos/04-programs/daemons.md#who-may-ask-for-what).

### What runs around a suspend

`kdos-powerd` tells NetworkManager and TLP about a [suspend]{.idx} itself
(`src/daemons/kdos-powerd/main.c`):

<!-- kfs:include src/daemons/kdos-powerd/main.c lines=189-214 pin=141a8599bfdb -->

The order and its reasons are in
[What happens around a suspend](../../kdos/04-programs/daemons.md#what-happens-around-a-suspend).
The client locks the screen before it asks; see
[Suspend locks the screen first](../../kdos/04-programs/daemons.md#suspend-locks-the-screen-first).

### TLP

`fs/etc/init.d/55_tlp.sh` applies TLP's boot settings and its shutdown settings; TLP is not a
daemon, so nothing is supervised:

<!-- kfs:include fs/etc/init.d/55_tlp.sh -->

The recipe installs TLP without its systemd and elogind parts, with its drop-in directory at
`/etc/tlp.d`:

<!-- kfs:include ports/core/hardware/tlp/build.sh lines=12-19 pin=9a34546e1601 -->

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

From the desktop, the Start menu's footer and the System menu (a right click on the Start button)
hold Lock Screen, Suspend, Restart, Shut Down and the item that ends the session, *Log Off* in the
footer and *Log Out* in the System menu. Restart and Shut Down ask for confirmation first. The sleep key suspends and the power button
asks before it shuts down; see [Laptops](laptops.md#the-power-button-and-the-sleep-key).

From a prompt, as a member of `seat` or `wheel`:

```bash
kdos-power suspend      # lock the session, then suspend to RAM
kdos-power poweroff
kdos-power reboot
kdos-power ping         # exit status 0 when the daemon answers and would let you suspend
```

`kdos-power --no-lock suspend` suspends without locking, and so does setting
`KDOS_NO_LOCK_ON_SUSPEND=1` in its environment. Over SSH, or at a text console, the ordinary
`sudo poweroff` and `sudo reboot` work as well.

Hibernation is not offered: the initramfs sets up no resume device, although the kernel lists
`disk` in `/sys/power/state`.

A suspend itself has not been carried out on the test rig, whose virtual machine is killed rather
than woken; the refusal and `kdos-power ping` have.

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
profiles by hand. `sudo tlp bat` and `sudo tlp ac` apply the `balanced` and `performance` profiles
in manual mode, which TLP reports as `balanced/BAT (manual)`: the profile then stays when the
machine moves between battery and mains, until the next `tlp start` or boot. `sudo tlp-stat -s`
shows which profile is in force. Otherwise TLP's udev rule applies the profile for the power source
again whenever the machine moves between battery and mains.

### Checking thermald

1. `service status thermald`. A machine that is not Intel reports `[DOWN] thermald`, and its
   boot log, `/run/kdos-init.thermald.log`, says `[SKIP] thermald: not an Intel processor`.
2. On an Intel model thermald does not support, it exits with status 2 and the supervisor leaves
   it stopped, saying so once in the system log:

   ```bash
   grep thermald /var/log/messages
   ```

### Seeing which application uses the processor's power

1. `kdos-energy` prints the report: each application's share of the energy that could be
   attributed, over the time the daemon has been sampling. `kdos-energy --json` prints it as JSON.
2. The Energy page of `kdos-res`, and a right click on the panel's meters strip, show the same
   report.

The answer is a share, never watt-hours, and it covers the processor package only. Root and
members of `wheel` may read it.

### Finding wakeups with powertop

1. `sudo powertop` opens its report. The *Overview* tab lists what wakes the processor most.
2. The *Tunables* tab lists settings that are marked *Bad*. Changing one there lasts until the
   next boot, and TLP applies its own values again at boot and at each change between battery and
   mains.

## Tips

- `kdos doctor` checks that `kdos-powerd` and [`kdos-energyd`]{.idx} are running, and says when the
  machine has no RAPL counter for `kdos-energyd` to read.
- `cat /sys/power/mem_sleep` shows which suspend state `mem` means on this machine, the one in
  brackets; `mem_sleep_default=deep` on the kernel command line asks for S3 where the firmware
  offers both. See [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md).
- Every refused `kdos-powerd` request is logged with the caller's uid, so a power key that does
  nothing can be traced: `grep kdos-powerd /var/log/messages` shows lines such as
  `kdos-powerd: refused uid 1001 (not root and in neither seat nor wheel)`.
- Most virtual machines have no RAPL counter, so `56_energyd` skips itself there, logging
  `[SKIP] kdos-energyd: no readable RAPL energy domain` in `/run/kdos-init.energyd.log`, and
  `kdos doctor` warns `no RAPL energy domain on this machine — per-app energy cannot be measured
  here at all`. On an AMD host the guest's thermald is skipped as not Intel; on most Intel hosts
  the guest's thermald exits with its status 2.
- `upower -e` lists what the UPower service sees; with no battery that is only
  `/org/freedesktop/UPower/devices/DisplayDevice`. The first call starts the service.

## Troubleshooting

**`kdos-power: no kdos-powerd — service start powerd`.** The daemon is not running. Start it
with the command the message names:

```bash
sudo service start powerd
```

Then read why it stopped: `/run/kdos-svc.kdos-powerd.log`, and `grep kdos-powerd
/var/log/messages`.

**`kdos-power suspend` prints `kdos-power: err not permitted`.** The account is in neither
`seat` nor `wheel`; `id` shows the groups. `kdos-power ping` asks the same question without
suspending: it prints nothing and exits 0 when allowed, and prints the same refusal and exits 1
when not.

**The machine suspends but the network is gone after waking.** NetworkManager is told to sleep and
wake around the suspend only through `dbus-send`; check that NetworkManager is running
(`service status networkmanager`) and that the system bus is up (`service status dbus`).

**`kdos-power: no lock confirmation within 2s — suspending anyway`.** The lock screen did not
confirm in time, and the client suspends regardless, so the session may wake unlocked. See
[Screen lock](screen-lock.md#troubleshooting).

**`kdos-energy` says the daemon is not running.** The message is
``kdos-energy: no kdos-energyd — `service start energyd` ``. Check `service status energyd`
(`[DOWN] kdos-energyd` when it is not running), and start it with `sudo service start energyd`. On a machine with no `/sys/class/powercap/*/energy_uj` it cannot run.

**The fan runs hard on an Intel laptop.** Check that thermald runs (`service status thermald`)
and what TLP set for the processor (`sudo tlp-stat -p`).

## Known gaps

- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)
- [Broad hardware support is not a goal](../../kdos/06-reference/known-gaps.md#broad-hardware-support-is-not-a-goal)
- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)

## See also

- [kdos-powerd](../../kdos/04-programs/daemons.md#kdos-powerd) and
  [kdos-energyd](../../kdos/04-programs/daemons.md#kdos-energyd): the two daemons
- [Locking, idle and power](../../kdos/02-user-guide/desktop.md#locking-idle-and-power)
- [Services](../../kdos/02-user-guide/administration.md#services): `54_thermald`, `55_powerd`,
  `55_tlp` and `56_energyd`
- [Services with no daemon](../../kdos/02-user-guide/administration.md#services-with-no-daemon):
  why `service status tlp` prints TLP's summary
- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent)
- `laptops.md`: [Laptops](laptops.md)
- `screen-lock.md`: [Screen lock](screen-lock.md)
- `services.md`: [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 401.* Previous: [400. Graphics drivers](graphics-drivers.md) · [Contents](../README.md) · Next: [402. Laptops](laptops.md)
