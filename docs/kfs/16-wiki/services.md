# Services

A KDOS [service]{.idx} is a script in `/etc/init.d` named `NN_name.sh`. `rcS` runs every enabled
one at boot, in numeric order, and the long-running daemons among them are kept alive by `ksvc`,
the [supervisor]{.idx term="supervisor (ksvc)"}. This page covers managing them by hand: listing them and their state, starting,
stopping and restarting one, choosing which start at boot, and finding out why one did not. Adding
a service of your own is on [Writing a service](writing-a-service.md); scheduled jobs are on
[Timers](timers.md). Every command on this page runs on a running KDOS machine.

## Installation

Services are part of every image. Three things provide them:

- [kdos-tools](#port-kdos-tools) installs the supervisor, `/usr/sbin/ksvc`, and the command that
  manages services, `/usr/sbin/service`, which is a link to it:

<!-- kfs:include src/system/kdos-tools/build.sh lines=25-27 pin=3545e2d02ffd -->

- The base filesystem, `fs/etc/init.d/` in the tree, supplies the scripts every image has, and
  `/etc/init.d/service_helper`, the shell functions those scripts call.
- Ports add scripts of their own from their `build.sh`, so a script exists only while its package is
  installed. [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts)
  in the KDOS book lists both sets, and [Services](../../kdos/02-user-guide/administration.md#services) says what
  each one starts. On a running machine the complete list is:

  ```bash
  ls /etc/init.d
  ```

Which services start at boot differs between the live medium and an installed system:

| | Live medium | Installed system |
|:--|:--|:--|
| Markers in `/etc/service.disabled` | `sshd`, from the image | The [live session]{.idx term="live system"}'s markers, copied with the rest of the root, after which the installer writes or removes one for each of its five service choices. CUPS and the SSH server are off unless chosen |
| Where a change is kept | In memory, gone at power-off, unless a persistence store exists; see [Live persistence](live-persistence.md) | On the root filesystem |

The installer's five choices, and the `services` key of an answer file, are in
[Answer files](../../kdos/04-programs/kinstall.md#answer-files).

## Configuration

### /etc/service.disabled/

A service is disabled by an empty file in this directory named after the service, and enabled by
removing it. Nothing else records whether a service starts at boot. The image ships one,
`fs/etc/service.disabled/sshd`. The rules are in the configuration reference's
[entry for the marker](../../kdos/06-reference/configuration.md#etcservicedisabledname).

The installer writes or removes the markers for the services in its table:

<!-- kfs:include src/system/kdos-installer/conf.c lines=23-29 pin=9a2600b3e3b2 -->

The last field is whether the service is on by default.

### /etc/init.d/service_helper

Every supervised script sources this file. Three of its functions are thin wrappers over `ksvc`:

<!-- kfs:include fs/etc/init.d/service_helper lines=48-58 pin=e25ef977dc8e -->

The fourth, `wait_bus_name NAME`, returns once `NAME` has an owner on the system bus, or after ten
seconds with `[WARN] NAME is not on the system bus after 10 seconds` and status 1. `40_dbus.sh`
waits for `org.freedesktop.DBus`, `41_polkitd.sh` for `org.freedesktop.PolicyKit1` and
`42_modemmanager.sh` for `org.freedesktop.ModemManager1`, so each daemon answers on the bus before
the scripts after it start. A daemon `ksvc` supervises has no D-Bus activation file, so the bus
never starts a second copy of it, and a call to its name while it is stopped is refused with
`ServiceUnknown`; see
[How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).

### A service script

A typical script, `70_sshd.sh`. `start` checks that the daemon exists and starts it under the
supervisor in the foreground; `stop` and `status` ask the supervisor:

<!-- kfs:include fs/etc/init.d/70_sshd.sh -->

The service's name, the one every `service` command takes, comes from the file name: `sshd` for
`70_sshd.sh`. The name the script passes to `supervise` (its `NAME`) is the one the supervisor's
files under `/run` carry, and it can differ: `35_chrony.sh` is the service [`chrony`]{.idx} and supervises
`chronyd`, `22_syslog.sh` is `syslog` and supervises `syslogd`, and `30_network.sh` is `network`
and supervises `dhcpcd`.

## Usage

### Listing services

1. List every service, whether it starts at boot, and its current state:

   ```bash
   service list
   ```

   Each row is one line: the service name, `[ENABLED]` or `[DISABLED]`, and the first line the
   script's own `status` printed, cut to the terminal's width. A script that prints more —
   `utmps` a line for each of `utmpd` and `wtmpd`, `timers` one per job, `tlp` its whole status
   report — gets ` (+N more: service status <name>)` after that line, and `service status <name>`
   shows the rest. A script whose `status` prints nothing shows `—`: `modules` and `userdirs`
   always, and `lvm` on a machine with no logical volumes.

2. Ask about one service:

   ```bash
   service status sshd
   ```

   A supervised service answers `[ OK ] sshd  (pid N)` or `[DOWN] sshd`, or
   `[FAIL] sshd  (exited with code N; restarting)` while its daemon keeps exiting and the
   supervisor keeps starting it again (see Troubleshooting). A service with no daemon
   reports what it applied instead; the meaning of each is in
   [Services with no daemon](../../kdos/02-user-guide/administration.md#services-with-no-daemon).

### Starting, stopping and restarting

1. Start, stop or restart a service for this boot only:

   ```bash
   sudo service start cups
   sudo service stop cups
   sudo service restart cups
   ```

   `restart` runs the script's `stop`, waits a second and runs its `start`. A supervised daemon
   is stopped as soon as it exits after `SIGTERM`, and killed if it is still running when its stop
   grace runs out (five seconds unless the script named another). `start` on a service that is
   already running starts nothing: it prints `[SKIP] <name>: already supervised (pid N)`.

2. A name that matches no script exactly is matched as part of one, so `service status ssh` reaches
   `70_sshd.sh`. Rely on this only for `status`, `start`, `stop` and `restart`; see the warning
   under [Choosing what starts at boot](#choosing-what-starts-at-boot).

3. The script can also be run directly, which is what `service` does:

   ```bash
   sudo /etc/init.d/80_cups.sh start
   ```

### Choosing what starts at boot

1. Find the exact service name in the first column of `service list`.
2. Disable or enable it:

   ```bash
   sudo service disable bluetooth
   sudo service enable bluetooth
   ```

   `disable` creates [`/etc/service.disabled/bluetooth`]{.idx} and `enable` removes it. Neither starts or
   stops the service now; do that with `start` or `stop`.

3. Check the marker:

   ```bash
   ls /etc/service.disabled
   ```

`enable` and `disable` accept part of a name too, but write the marker under the name exactly as
typed, and the boot looks for the script's own name, so a shortened name succeeds and changes
nothing. Always give the full name.

### Turning the SSH server on and off

The SSH server is tied to its firewall rule. An administrator (a member of `wheel`) opens both at
once:

```bash
kdos-power firewall ssh on
```

This removes `/etc/service.disabled/sshd`, starts `70_sshd.sh` and opens the SSH port in the
firewall, a `tcp dport 22 accept` rule in `nft list ruleset`, and prints `ok ssh on`; the first
start also generates the host keys. `off` puts the marker back, stops the server, closes the port
and prints `ok ssh off`. See
[Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service).

### Turning NetworkManager off

1. Disable and stop it:

   ```bash
   sudo service disable networkmanager
   sudo service stop networkmanager
   ```

2. Start the fallback DHCP client for this boot; from the next boot on, `30_network` starts it by
   itself:

   ```bash
   sudo service start network
   ```

Nothing else starts it again. NetworkManager has no D-Bus activation file, so a call to its name
while it is stopped, such as the one `kdos-netagent` makes in every desktop session, fails with
`org.freedesktop.DBus.Error.ServiceUnknown` and starts nothing; the marker is the only switch. The
same holds for every daemon a service script supervises; see the configuration reference's
[entry for the marker](../../kdos/06-reference/configuration.md#etcservicedisabledname).

## Tips

- `ksvc check <name>` asks the supervisor directly about the name a script passed to `supervise`:
  `ksvc check chronyd`, not `chrony`. Given the service's name instead it finds no pid file and
  answers `[DOWN] chrony` while the daemon runs. It is also how one periodic job is checked, under
  the name `kdos-timer-<job>`.
- A supervised daemon's output is in `/run/kdos-svc.<name>.log`, under the supervisor's name, and
  in the system log tagged with that name. What its script printed at boot is in
  `/run/kdos-init.<service>.log`, under the service name. See [Logs](logs.md).
- A service that is enabled but skips itself at boot prints a `[SKIP]` line with the reason, for
  example `[SKIP] ngircd: no /etc/ngircd.conf (/usr/share/doc/ngircd/sample-ngircd.conf is the
  template)`. A script written this way changes nothing at install time: the service starts once
  its configuration exists.
- `kdos doctor` checks that `kdos-powerd`, `kdos-energyd` and `kdos-oomd` are running, by their
  sockets, and warns about each one that is not with the `service start` command that starts it.
  On a machine with no RAPL energy counters, most virtual machines among them, it reports
  `[skip] no RAPL energy domain on this machine — per-app energy cannot be measured here at all`
  instead of a stopped `kdos-energyd`, whose script skips it there.
- `kdos restarts` lists the processes still running files that an upgrade replaced. Restart a
  daemon among them with `sudo service restart <service>`, using the service's name, not the
  process's.

## Troubleshooting

**`Error: Service 'x' not found`.** No `/etc/init.d/NN_x.sh` exists, and no service name contains
`x`. Check `ls -l /etc/init.d`: a name that is not two digits, an underscore, a name and `.sh` is
not a service. A port's script is missing when the port is not installed.

**`service start x` prints nothing and the service is not in `service list`.** The script is not
executable. `service list`, `rcS` and `rcK` skip it, and `service` fails to run it without a
message. Run `sudo chmod 755 /etc/init.d/NN_x.sh`.

**A disabled service still starts at boot.** The marker has the wrong name. Compare
`ls /etc/service.disabled` with the first column of `service list`, remove the wrong marker, and
run `sudo service disable <full name>`.

**A service is `[DOWN]` after boot.** Read what its script printed:

```bash
cat /run/kdos-init.<service>.log
```

A `[SKIP]` line gives the reason the script did not start the daemon. The scripts that skip
themselves on unsuitable hardware are listed in
[Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves).

**The daemon keeps restarting.** The supervisor restarts it five seconds after every exit, and
`service status` answers `[FAIL] <name>  (exited with code N; restarting)` between the starts,
`[FAIL] <name>  (exited with code N, M failed starts in a row; restarting)` once it has failed
more than once in a row, and `[FAIL] <name>  (crash loop: M failed starts in a row, …)` while a
daemon that failed twice in a row is running again and has not yet lasted a minute. An exit with a
non-zero status counts as a failed start, and so does an exit with status 0 within five seconds of
the start. A failure after a minute or more of running starts the count again at one, and an exit
with status 0 after five seconds clears it. The supervisor keeps that state in
`/run/kdos-svc.<name>.state`. Its log shows each exit and the daemon's own last words:

```bash
sudo tail -n 40 /run/kdos-svc.<name>.log
```

Lines of the form `[KDOS] (<name>) Exited with code <N>. Restarting in 5s...` repeat. Correct what the daemon
complains about, then `sudo service restart <service>`.

**`[KDOS] (smartd) Exited with code 17, a final status: not restarting`.** The daemon decided it has
nothing to do on this machine, and its script told the supervisor that this exit is final. On a
virtual machine `smartd` ends this way, finding no disk that reports SMART data, and `service status
smartd` then answers `[DOWN] smartd`. `thermald` ends the same way on an Intel machine with nothing
for it to drive, and on any other processor its script skips it with
`[SKIP] thermald: not an Intel processor`. Nothing needs fixing; disable the service to skip it at
boot.

**`service stop` reports `Stopped`, and the daemon is back five seconds later.** The pid file the
supervisor wrote, `/run/<name>.pid`, holds another process's id: a daemon that writes a pid
file of its own at the same path overwrites the supervisor's, and `stop` then signals the daemon
alone, which the supervisor restarts. Compare the file with the process tree:

```bash
cat /run/<name>.pid
ps -o pid,pgid,args -p "$(cat /run/<name>.pid)"
```

The supervisor's command line starts `ksvc supervise`. The cure belongs in the script; see
[Writing a service](writing-a-service.md#tips).

**`[FAIL] <name>  (supervisor gone; daemon pid N still running)`.** The supervisor was killed and
its daemon was left running. `service start` will not start a second copy beside it: it waits
for that daemon for the stop grace and then refuses with `[FAIL]`. Stop it, then start the
service again:

```bash
sudo service stop <service>
sudo service start <service>
```

**`Error: Unknown command '<word>'`.** The words are in the wrong order. `service` takes the verb
first and the service name second, the name from the script's file name and not the daemon's:

```bash
sudo service start powerd
```

## Known gaps

- [LVM volume groups are activated only at boot](../../kdos/06-reference/known-gaps.md#lvm-volume-groups-are-activated-only-at-boot):
  `03_lvm` runs once
- The live image's accounts have known passwords; see
  [Security](../../kdos/06-reference/known-gaps.md#security)

## See also

- [Services](../../kdos/02-user-guide/administration.md#services): every shipped script and what it starts
- [ksvc and service](../../kdos/04-programs/kdos-command.md#ksvc-and-service): the command's reference
- [Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging): how the supervisor runs a daemon
- [The daemons](../../kdos/04-programs/daemons.md): KDOS's own root daemons and their scripts
- [Init and runlevels](init-and-runlevels.md), [Writing a service](writing-a-service.md),
  [Timers](timers.md), [Logs](logs.md), [Time and NTP](time-and-ntp.md),
  [Out of memory](out-of-memory.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 359.* Previous: [358. Init and runlevels](init-and-runlevels.md) · [Contents](../README.md) · Next: [360. Writing a service](writing-a-service.md)
