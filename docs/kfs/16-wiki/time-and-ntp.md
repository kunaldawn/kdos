# Time and NTP

KDOS keeps its clock with [chrony]{.idx}, which starts at boot and needs no setting, and keeps the
[time zone]{.idx} in two files that always name the same zone. This page shows how to check that the clock
is synchronised, how to set the time zone, how to use other time servers, and what to look at when
the clock is wrong. Why the zone is written twice, and what else the zone sets, is in the KDOS
book's [kdos-settingsd](../../kdos/04-programs/daemons.md#kdos-settingsd) section.

## Installation

| What | Port | Started by |
|---|---|---|
| [`chronyd`]{.idx} and [`chronyc`]{.idx} | [chrony](#port-chrony) | [`/etc/init.d/35_chrony.sh`]{.idx}, at every boot |
| The zone database, [`/usr/share/zoneinfo`]{.idx} | [tzdata](#port-tzdata) | read by the C library, nothing to start |
| `org.freedesktop.timedate1`, which sets the zone, the clock and network time | [kdos-settingsd](#port-kdos-settingsd) | [`/etc/init.d/43_settingsd.sh`]{.idx} |
| The **Date & Time** page of System Settings, and the clock in the panel | [plasma-workspace](#port-plasma-workspace) | System Settings |

The two service scripts come from the tree's `fs/` overlay, not from the ports, and the image
ships no marker for either in `/etc/service.disabled/`, so both start on the live medium and on an
installed system. `35_chrony.sh` runs after `30_network`. The installer asks for the zone on its
Time page and writes it; see [3. Time](../../kdos/02-user-guide/installation.md#3-time).

## Configuration

**[`/etc/chrony/chrony.conf`]{.idx}** is written by the chrony port, in
`ports/core/network/chrony/build.sh`:

<!-- kfs:include ports/core/network/chrony/build.sh lines=29-34 pin=76d243384eea -->

`pool` names the servers, and `iburst` sends a quick burst of requests at the start so the first
measurement arrives within seconds. `driftfile` is where chronyd keeps the rate error of this
machine's clock between boots. `makestep 1.0 3` lets it step the clock instead of slewing it when
it is more than one second out, during its first three updates only. `rtcsync` leaves the hardware
clock to the kernel, which copies the system time to it every eleven minutes once the clock is
synchronised (the kernel's `RTC_SYSTOHC` option, on by default and not turned off in the kernel
port's `ports/core/boot/linux/kdos.config`); no init script runs `hwclock`. No `allow` line is
present, so chronyd serves time to no other machine. Every directive is in `chrony.conf(5)`.

The file belongs to the package: an upgrade of chrony installs it again over an edited copy, and
`kpkg check chrony` lists the edit as `chrony: config ./etc/chrony/chrony.conf` (see
[Checking an installed root](../../kdos/03-architecture/packaging.md#checking-an-installed-root)).

**`/etc/init.d/35_chrony.sh`**, from `fs/etc/init.d/35_chrony.sh`:

<!-- kfs:include fs/etc/init.d/35_chrony.sh -->

**[`/etc/localtime`]{.idx}** is a symbolic link into `/usr/share/zoneinfo`. No package owns it. When
nothing is there at boot, as on every boot of the live medium, `rcS` links UTC, from
`fs/etc/init.d/rcS`:

<!-- kfs:include fs/etc/init.d/rcS lines=110-116 pin=c9969d56d2ea -->

**[`/etc/profile.d/20-timezone.sh`]{.idx}** sets `TZ` for every login shell. It is not in the tree: the
installer writes it, and kdos-settingsd writes it on every zone change, so the live medium has
none until the first one. The text, from `src/daemons/kdos-settingsd/store.c`:

<!-- kfs:include src/daemons/kdos-settingsd/store.c lines=986-992 pin=11bcddfa8bb9 -->

**`/etc/modprobe.d/kdos-regdom.conf`** is the wireless regulatory country, written from the zone
by the same call; see
[`/etc/modprobe.d/kdos-regdom.conf`](../../kdos/06-reference/configuration.md#etcmodprobedkdos-regdomconf).

## Usage

### Checking that the clock is synchronised

1. Ask chronyd how far off the clock is and from which server it takes the time. Any account can
   run it:

   ```bash
   chronyc tracking
   ```

   A `Leap status` of `Normal` means the clock is synchronised, and `Not synchronised` that
   chronyd has no usable source yet. `System time` is how far the clock is from the time chronyd
   has measured.

2. List the servers it is using and how each is doing:

   ```bash
   chronyc sources -v
   ```

   The source marked `*` is the one the clock follows. `-v` prints a legend for the columns.

3. Or open System Settings → **Date & Time**, which shows the zone, the clock and whether it is
   set automatically.

### Setting the time zone

polkit lets the person at the machine do it; anyone else is asked for an administrator's
password.

1. Find the zone's name, `Area/City`. The list of current zones is the third column of
   `zone1970.tab`:

   ```bash
   grep -v '^#' /usr/share/zoneinfo/zone1970.tab | cut -f3 | sort | less
   ```

2. Set it on System Settings → **Date & Time** → **Time Zone**, or from a prompt through
   timedate1:

   ```bash
   dbus-send --system --print-reply --dest=org.freedesktop.timedate1 \
       /org/freedesktop/timedate1 org.freedesktop.timedate1.SetTimezone \
       string:Europe/Berlin boolean:true
   ```

   `/etc/localtime`, `TZ` and the Wi-Fi country change together.

3. Open a new terminal, or log in again, and check with `date`. A shell started before the change
   keeps the `TZ` it read at login.

On the live medium, the zone lasts until power-off: `/etc` starts again as the image's at every
boot, and `rcS` links UTC. A persistence store keeps it
([kdos persist](../../kdos/04-programs/kdos-command.md#kdos-persist)).

### Correcting a clock that is far off

chronyd corrects a small offset by running the clock slightly fast or slow, which can take a long
time for a large one. To step it at once:

```bash
sudo chronyc makestep
```

It answers `200 OK`. `makestep` needs root, because chronyc sends it over chronyd's own socket,
`/run/chrony/chronyd.sock`, which only root can reach; run by any other account it goes over
`127.0.0.1` instead and is answered `501 Not authorised`. The monitoring commands above go over
`127.0.0.1` and need no privilege. chronyd opens the socket only in a run directory no other
account can enter, which is why `35_chrony.sh` sets `/run/chrony` to mode 0750 at every start.

### Using other time servers

1. Edit the configuration:

   ```bash
   sudoedit /etc/chrony/chrony.conf
   ```

   Replace the `pool` line with your own servers, one `server` line each:

   ```text
   server ntp1.example.net iburst
   server ntp2.example.net iburst
   ```

   chronyd is built with NTS, authenticated NTP, through [gnutls](#port-gnutls); `chronyd -v`
   lists `+NTS`. A server that offers it takes the `nts` option: `server <host> iburst nts`.

2. Restart the daemon:

   ```bash
   sudo service restart chrony
   ```

3. Check with `chronyc sources -v`.

Keep a copy of the edited file, because an upgrade of the chrony package puts the shipped one back
([Configuration](#configuration)).

## Tips

- `TZ=Asia/Tokyo date` shows another zone's time for one command without changing anything.
- The kernel reads the [hardware clock]{.idx} as UTC. A machine that also boots a system keeping the
  hardware clock in local time starts with the clock off by the zone's offset, and chronyd steps it
  at the first update.
- A zone with no country, such as `UTC`, removes the Wi-Fi country file, and the radio returns to
  the world domain at the next boot.
- `chronyc` with no arguments is an interactive prompt; `help` there lists every command.

## Troubleshooting

**`chronyc tracking` prints `506 Cannot talk to daemon`.** chronyd is not running.

```bash
service status chrony
cat /run/kdos-init.chrony.log
sudo cat /run/kdos-svc.chronyd.log
```

The first log is the boot script's output, the second chronyd's own messages, readable by root
only, which also go to `/var/log/messages` tagged `chronyd`; see
[Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging). Start it with
`sudo service start chrony`.

**`sudo chronyc makestep` prints `501 Not authorised`.** chronyd disabled its command socket. Its
log says why:

```bash
sudo grep 'command socket\|Wrong permissions' /run/kdos-svc.chronyd.log
```

`Wrong permissions on /run/chrony` and `Disabled command socket /run/chrony/chronyd.sock` mean the
directory is open to other accounts. Restart the service, which sets it back to 0750:
`sudo service restart chrony`.

**`Leap status` reads `Not synchronised`, and every source in `chronyc sources` shows `?`.** chronyd
reaches no server. Check that the machine is online, and that the names in `chrony.conf` resolve:
`getent hosts pool.ntp.org`. The firewall lets every outgoing packet through, and the answers back
with it.

**`date` shows UTC on an installed machine.** `rcS` links UTC at boot when nothing is at
`/etc/localtime`, and a link to a zone file that is not there reads as UTC too.
`ls -l /etc/localtime` shows which; set the zone again as in
[Setting the time zone](#setting-the-time-zone).

**The Date & Time page cannot save.** kdos-settingsd is not running: `service status settingsd`,
and its log `/run/kdos-svc.kdos-settingsd.log`. A refusal names the zone it could not find under
`/usr/share/zoneinfo`.

**`date` and the desktop's clock disagree.** One of them was started before a zone change. Log out
and in again.

## Known gaps

[Known gaps](../../kdos/06-reference/known-gaps.md) lists nothing for this topic.

## See also

- [The root daemons](../../kdos/04-programs/daemons.md): kdos-settingsd, which serves timedate1
- [Media, colour and time](../../kdos/02-user-guide/administration.md#media-colour-and-time): the zone data and how the zone is set
- [3. Time](../../kdos/02-user-guide/installation.md#3-time): the installer's Time page
- [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts): where the UTC link is made
- [Locale and console](locale-and-console.md): the other settings the profile carries into every login
- [Services](services.md): starting, stopping and disabling a service
- [Logs](logs.md): where chronyd's messages go
- [Live persistence](live-persistence.md): keeping the zone on the live medium

<!-- book-nav -->
---

*Part XVI. System administration, chapter 390.* Previous: [389. Out of memory](out-of-memory.md) · [Contents](../README.md) · Next: [391. Locale and console](locale-and-console.md)
