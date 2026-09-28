# Administration

This chapter is for the person who looks after a KDOS machine once it is installed or booted from
the stick. The first half is the machine as a whole: services and periodic jobs, the login on
`tty1`, users and groups, storage, networking and the firewall, hardware and device access,
printing, media and time, updates, diagnosis and logs, copying and rebuilding the medium, and
tuning builds for this CPU. The second half is what each user sets up in their own home: backups,
mail, passwords and one-time codes, calendars and contacts, finding files by name, and input
methods. Each task is given as the command or the file that does it, with the reason the system is
arranged that way where the reason affects what you do.

If you are new to KDOS, read [Getting started](getting-started.md) first; this chapter assumes you
can open a terminal and use `sudo`. The mechanisms behind it are in
[Boot and init](../03-architecture/boot-and-init.md) and [The daemons](../04-programs/daemons.md).
Two facts shape everything below:

- **There is no systemd and no settings database.** Services are shell scripts in `/etc/init.d`,
  and every setting in this chapter is a plain file that takes effect when the program reads it.
  [Configuration](../06-reference/configuration.md) lists every one of those files and keys.
  [How KDOS differs](../01-philosophy/how-kdos-differs.md) compares this with how other
  distributions arrange the same things.
- **The first account is `kdos`, and on the live image its password is `kdos`.** The installer asks
  for a new one. On a machine that still has the shipped password, run `passwd` before anything
  else; `sudo kdos doctor` warns until you do (the check needs to read `/etc/shadow`, so it runs
  only as root).

When something is wrong, start with [Diagnosing](#diagnosing).

## Services

A *service* is a script in `/etc/init.d` named `NN_name.sh`, where `NN` is two digits that fix its
place in the boot order. At boot, `init` runs `/etc/init.d/rcS` as its `sysinit` entry
([Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts)). `rcS` runs the
enabled scripts in numeric order with the argument `start`, and `ksvc`, the KDOS service supervisor,
keeps each long-running daemon under a process of its own and restarts it five seconds after it
exits.
You manage services with `service`:

```sh
service list                  # every service, whether it starts at boot, and its state
service status <name>
service start   <name>
service stop    <name>
service restart <name>        # stop, wait one second, start
service enable  <name>        # start it at boot
service disable <name>        # do not start it at boot
```

`service` is a link to `ksvc`, so `ksvc status sshd` and `service status sshd` do the same thing.
`ksvc check <name>` asks the supervisor directly whether the process it started under that name is
alive, which is how a single periodic job is checked (see [Periodic jobs](#periodic-jobs)).

`<name>` is the part of the file name between the number and `.sh`: `sshd` for `70_sshd.sh`. For
`status`, `start`, `stop` and `restart`, an exact name is tried first and then any service whose
name contains what you typed, so `service status ssh` finds `sshd`. `enable` and `disable` check
the same way that the service exists, but record the name exactly as you typed it, so type the full
name from `service list`: `service disable ssh` writes a marker that `rcS` never looks for.

Enabling and disabling is a marker file, not an edit: `service disable bluetooth` creates the empty
file `/etc/service.disabled/bluetooth`, and `service enable bluetooth` deletes it. Nothing in
`/etc/init.d` is rewritten. The live image ships no markers, so every installed service starts; the
installer writes markers for the services you turn off, and turns off CUPS and sshd by default (see
[Installation](installation.md)).

During boot, each script's output goes to `/run/kdos-init.<name>.log` and is then copied to the
console, so the first thing to read about a service that failed at boot is that file. It lives in
`/run`, a tmpfs, and is gone after the next reboot.

The scripts that ship, in boot order. Scripts marked *(package)* come with the package named and
are absent if that package is removed.

| Script | What it starts |
|---|---|
| `01_udev` | Device management, and the coldplug that loads drivers for the hardware already present. It also creates device nodes such as `/dev/uinput`, `/dev/uhid`, `/dev/snd/seq` and `/dev/vhost-net`, whose driver loads only when a program first opens the node, so they exist before anything asks for them |
| `02_modules` | Modules listed in `/etc/modules-load.d` |
| `03_lvm` | LVM volume groups, activated once at boot, then any `fstab` entry on a logical volume checked and mounted. On a disk boot the initramfs has already activated the groups it saw; this pass catches the rest — every group on a live boot, and any on a disk that appeared after the initramfs ran. A group plugged in later is not activated automatically; run `sudo vgchange -aay` |
| `05_hostname` | The hostname |
| `10_sysctl` | Kernel parameters from `/etc/sysctl.conf`: unprivileged `ping`; the name of the fatal signal in the kernel log when a program crashes; the `fs.protected_*` link protections, which stop a user redirecting a root job's write in `/tmp` onto another file; and per-task delay accounting, which `iotop`'s IO% and `htop`'s delay columns read, at a small cost on every wait the kernel accounts |
| `12_zram` | Compressed swap in RAM — see [Storage](#storage) |
| `15_userdirs` | `/run/user/<uid>` for every account with a UID from 1000 to 65533, and a delegated cgroup subtree for each with the `cpu`, `memory` and `pids` controllers, which is what lets rootless podman apply a *box*'s memory and CPU limits (a box is the rootless container one application runs in; see [the glossary](../06-reference/glossary.md)) |
| `18_timers` | Periodic jobs: one supervised `snooze` per line of `/etc/kdos/timers.d` — see [Periodic jobs](#periodic-jobs) |
| `20_dmesg`, `22_syslog` | Kernel and system logging. `20_dmesg` lowers the console log level to 3, so only kernel errors reach the screen. `22_syslog` runs `syslogd`, whose `/etc/syslog.conf` sets `secure_mode 2`, so it opens no network socket: it neither accepts messages from other hosts nor forwards to one. `secure_mode 1` allows forwarding to an `@host` action without listening. See [Logs](#logs) |
| `25_nftables` | The firewall, loaded before the network comes up — see [The firewall](#the-firewall) |
| `30_network` | `dhcpcd`, the fallback DHCP client. It stays down when NetworkManager is installed and not disabled, because NetworkManager's own DHCP client does not defer to it: two clients on one link means two leases, two default routes and two writers of `/etc/resolv.conf` |
| `31_babeld` *(babeld)* | Babel mesh routing, skipped until a non-empty `/etc/babeld.conf` exists. Open the `babel` firewall name for the neighbours |
| `35_chrony` | Time synchronisation |
| `40_dbus` | The system message bus. On its first start it generates `/var/lib/dbus/machine-id` and links `/etc/machine-id` to it |
| `41_polkitd` | polkit, the privilege broker. It starts before NetworkManager, which asks it on its first privileged call |
| `42_modemmanager` | ModemManager: mobile broadband modems, for NetworkManager and `mmcli`. Started at boot rather than on demand for the same reason as polkit |
| `42_networkmanager` | NetworkManager |
| `43_boltd` *(bolt)* | Thunderbolt. Only when `/sys/bus/thunderbolt/devices` is non-empty, it calls `org.freedesktop.bolt` so the bus starts `boltd`. A device plugged in later is handled by the `bolt` package's udev rule |
| `45_avahi` | mDNS: finding printers and other machines on the local network by name |
| `45_seatd` | Seat management, which the desktop needs |
| `46_hostapd` | A wireless access point, only when `/etc/hostapd/hostapd.conf` exists — see [An access point with hostapd](#an-access-point-with-hostapd) |
| `47_pcscd` | Smart cards and security keys: the PC/SC daemon, with readers arriving through udev |
| `50_alsa` | Saves and restores the sound card's mixer levels |
| `51_mdmonitor` | `mdadm --monitor --scan --syslog`: a failed member, a degraded array or a missing spare is written to the system log. Skipped when `/proc/mdstat` lists no array; a `MAILADDR` or `PROGRAM` line in `/etc/mdadm.conf` adds a route of your own |
| `51_lsmd` *(libstoragemgmt)* | The libstoragemgmt plugin daemon, which udisks2's LSM module and `lsmcli` reach over `/run/lsm/ipc`; plugins that do not need root run as the `libstoragemgmt` account |
| `52_smartd` | Disk health: `smartd` polls every disk that answers SMART every half hour and logs a failing health check, bad sectors, drive errors and failed self-tests. Where no disk answers SMART — any virtual machine — it logs why and stops |
| `53_xfs_healer` | One `xfs_healer` per XFS filesystem mounted at boot. It logs the metadata damage the kernel reports and, where the filesystem's `autofsck` property asks for it, has it repaired online. Skipped on a kernel without the XFS health monitor |
| `54_thermald` | Intel thermal management |
| `55_powerd` | `kdos-powerd`: suspend, power-off and reboot for the desktop, and a few system settings (time zone, accent, autologin, firewall names) |
| `55_tlp` | Laptop power management: `tlp init start` at boot and `tlp init stop` at shutdown, which apply the startup and shutdown radio settings in `/etc/tlp.d`. `kdos-powerd` runs `tlp suspend` and `tlp resume` around a suspend |
| `56_energyd` | Per-application energy use |
| `56_nut` *(nut)* | The UPS tools: the drivers, `upsd` and `upsmon`, or `upsmon` alone as a network client. Skipped unless `MODE` in `/etc/nut/nut.conf` is `standalone`, `netserver` or `netclient`. Open the `nut` firewall name to serve UPS status to other machines |
| `57_oomd` | Memory-pressure protection: ends the heaviest application before the machine stalls, sparing the desktop itself |
| `58_mountd` | Removable media for the desktop |
| `59_packd` | `kdos-packd`, which mounts application *packs*: signed EROFS images holding an application, runtime, base or dataset, which boxes are built over. See [Packs and boxes](../03-architecture/packs-and-boxes.md) |
| `60_bluetooth` | Bluetooth |
| `62_virtlogd` *(libvirt)* | libvirt's log daemon, which keeps each guest's console log |
| `63_gssd` *(nfs-utils)* | `rpc.gssd`, the Kerberos half of an NFS client, which a `sec=krb5` mount needs. It mounts `rpc_pipefs` at `/var/lib/nfs/rpc_pipefs` first. Skipped until `/etc/krb5.keytab` exists |
| `63_libvirtd` *(libvirt)* | libvirt's management daemon for `qemu:///system`. Its polkit rule lets members of the `libvirt` group manage guests |
| `64_lircd` *(lirc)* | The infrared remote daemon, skipped until a receiver is present (a `/dev/lirc*` node or an rc-core device). Its socket `/run/lirc/lircd` is what Kodi, VLC and other LIRC clients read |
| `65_brltty` *(brltty)* | The console screen reader, skipped until `/etc/brltty.conf` exists. Its speech goes straight to the sound card, so it cannot speak while a desktop session's PipeWire holds the card. See [Accessibility](accessibility.md) |
| `70_sshd` | The SSH server. Before the first start it generates every missing host key type with `ssh-keygen -A`. The shipped firewall blocks port 22 until you open `ssh` |
| `71_snmpd` *(net-snmp)* | The SNMP agent, as root, skipped until a non-empty `/etc/snmp/snmpd.conf` exists; it logs to syslog. Open the `snmp` firewall name for other machines |
| `72_nfsd` *(nfs-utils)* | The NFSv4 server, skipped until `/etc/exports` names a share. It loads `nfsd`, supervises `nfsdcld` (the client records a restarted server needs) and `nfsv4.exportd`, runs `exportfs -r`, then starts the kernel threads with `rpc.nfsd -N 3 -V 4`. NFSv3 is off, so no portmapper, `rpc.mountd` or lock daemon runs. After editing `/etc/exports`, run `sudo exportfs -r`. Open the `nfs` firewall name for other machines |
| `73_mosquitto` *(mosquitto)* | The MQTT broker, as the `mosquitto` account, skipped until `/etc/mosquitto/mosquitto.conf` exists. With no configuration the broker listens on loopback only; a `listener 1883` line there, plus the `mqtt` firewall name, lets devices on the LAN reach it |
| `74_prosody` *(prosody)* | The XMPP chat server, as the `prosody` account, skipped until it has an account: `sudo prosodyctl adduser <user>@<host>` makes the first. Clients are refused until the host has a certificate; `sudo prosodyctl cert generate <host>` makes a self-signed one. Open the `xmpp` firewall name for other machines |
| `75_mumble-server` *(mumble)* | The Mumble voice server, as the `mumble-server` account, skipped until its database exists: running `mumble-server -supw <password>` as that account makes it. Open the `mumble` firewall name for other machines |
| `76_postgresql` *(postgresql)* | One shared PostgreSQL server, as the `postgres` account, skipped until a cluster exists: `sudo -u postgres initdb -D /var/lib/postgres/data` makes it. It listens on loopback and on a socket in `/tmp` |
| `77_radicale` *(radicale)* | Radicale, the CalDAV and CardDAV server, as the `radicale` account, skipped until `/etc/radicale/users` has an account line (`name:` followed by a hash from `openssl passwd -6`). Open the `caldav` firewall name for other machines |
| `78_maddy` *(maddy)* | The maddy mail server, as the `maddy` account, skipped until `/var/lib/maddy/credentials.db` holds an account (running `maddy creds create <address>` and `maddy imap-acct create <address>` as the `maddy` account, with `sudo -u maddy`, makes one). Open the `mail` firewall name for other machines |
| `79_ngircd` *(ngircd)* | The ngIRCd chat server, as the `ngircd` account, skipped until a non-empty `/etc/ngircd.conf` exists; `/usr/share/doc/ngircd/sample-ngircd.conf` is the template. Open the `irc` firewall name for other machines |
| `80_cups` | Printing |
| `81_cups-browsed` | Printers shared on the network, added to CUPS as they appear |
| `82_ipp-usb` | Driverless printing and scanning over USB: each IPP-over-USB device is served on localhost as it is plugged in |
| `83_samba` *(samba)* | `smbd`, skipped until a non-empty `/etc/samba/smb.conf` exists. Open the `smb` firewall name for other machines |
| `84_minidlna` *(minidlna)* | MiniDLNA, the media server for televisions and players on the LAN, skipped until a non-empty `/etc/minidlna.conf` exists; the template is in `/usr/share/doc/minidlna`. Open the `dlna` firewall name for other machines |
| `85_gnuhealth` *(gnuhealth)* | The GNU Health server (`trytond`), as the `gnuhealth` account, skipped until `/etc/gnuhealth/trytond.conf` names a database `uri`. Open the `tryton` firewall name for other machines |
| `86_kiwix-serve` | The offline library on `127.0.0.1:8080`, skipped until `/var/lib/kiwix/library.xml` lists an archive — see [The local servers](../03-architecture/boot-and-init.md#the-local-servers) |
| `87_kolibri` | Kolibri, the offline curriculum, on `127.0.0.1:8081`, skipped until a channel is installed |
| `88_llama-server` | The local language-model server on `127.0.0.1:8082`, skipped until a `.gguf` model is under `/usr/share/llama.cpp/models` |
| `89_step-ca` *(step-ca)* | The step-ca certificate authority, as root, from `/etc/step-ca`. Skipped until `/etc/step-ca/config/ca.json` (from `sudo STEPPATH=/etc/step-ca step ca init`) and `/etc/step-ca/password.txt` (the CA password, mode 600) both exist. Open the `https` firewall name for other machines when the CA listens on 443 |

### Shutdown

Shutdown runs the same set backwards. `/etc/init.d/rcK` is the first `::shutdown` entry in
`/etc/inittab`; it runs each enabled script with `stop`, in reverse order, before swap is turned
off and filesystems are unmounted, so a service can still save its state. A service that fails to
stop does not hold up the rest. The firewall is not stopped: it stays loaded until power-off, so
the machine is never on the network unfiltered. See
[Boot and init](../03-architecture/boot-and-init.md#shutdown).

### Services that skip themselves

A daemon that cannot do its job on this machine is skipped at boot, with a `[SKIP]` line saying
why, instead of being started and left to fail:

| Script | Skipped when |
|---|---|
| `54_thermald` | The processor is not Intel |
| `56_energyd` | No readable CPU energy counter (`/sys/class/powercap/*/energy_uj`) |
| `57_oomd` | `/proc/pressure/memory` is not writable (pressure stall information is off) |
| `59_packd` | The kernel cannot mount EROFS, the pack filesystem |

The check runs before the daemon starts, because a daemon that refuses to run, restarted every
five seconds by its supervisor, keeps the boot from ever settling.

Some daemons can only tell for themselves that they have nothing to do: thermald knows which Intel
models it supports, and smartd finds out only by asking each disk. When one of these — thermald,
smartd, `mdadm --monitor` or `xfs_healer` — exits with a status that restarting cannot change, the
supervisor logs one line such as `[KDOS] (thermald) Exited with code 2, a final status: not
restarting` and leaves it stopped, and `service status` reports it as not running. Each script
names those final statuses when it starts its daemon (`supervise --final-exit 2 …`); how the
supervisor tells that exit from a crash is in
[The daemons](../04-programs/daemons.md#the-shape-they-share).

### Services with no daemon

Twelve scripts leave nothing for the supervisor to watch: `01_udev`, `02_modules`, `03_lvm`,
`05_hostname`, `10_sysctl`, `12_zram`, `15_userdirs`, `20_dmesg`, `25_nftables`, `43_boltd`,
`50_alsa` and `55_tlp`. Most apply a setting and exit; `25_nftables` and `12_zram` hand their
result to the kernel, which keeps it. For these, `service status` reports what the script applied
rather than a running process: `sysctl: applied`, the zram device and its nominal size, whether a
firewall ruleset is loaded, the logical volumes (`lvs`), or TLP's own summary (`tlp-stat -s`).
`02_modules` and `15_userdirs` print nothing. Two report a process after all:
`service status boltd` says whether the bus has `boltd` running, and `service status udev` says
whether `udevd` is running, because `udevd` runs on its own once the script has started it.

## Periodic jobs

There is no cron daemon. A periodic job is one line in a file ending `.timer` under
`/etc/kdos/timers.d/`:

```text
NAME  TIMESPEC...  --  COMMAND...
```

Blank lines and lines starting with `#` are ignored. For example, the shipped update check:

```text
update-check  -H 4 -M 17 -s 8h  --  kdos update check --json --out /var/lib/kdos/update.json
```

`snooze` is a small program that sleeps until the clock matches its options, runs a command once
and exits. At boot `18_timers` starts one `snooze` per line under the supervisor, as the service
`kdos-timer-NAME`; when `snooze` exits after a run, the supervisor starts it again and it sleeps
until the next slot. That gives one process per job and no shared schedule file to corrupt.
`service status timers` shows every job, and `ksvc check kdos-timer-update-check` shows one.

**The timespec** is `snooze`'s own options, unchanged: `-H 4 -M 17` is 04:17, `-d /2` is every
second day. `snooze(1)` is the full reference.

**Missed runs.** `-s` sets the slack. A machine that was *asleep* at the slot runs the job once
when it wakes, if it wakes within the slack — once, not once per missed slot, so a laptop closed
for a fortnight does not run fourteen catch-up jobs.

`-s` does not cover a machine that was *off*. A `snooze` that has just started looks forward from
now, so a slot that passed before boot is missed until the next one. `-t FILE` changes that:
`snooze` then counts from that file's modification time, and the slack covers the gap. A job that
must run even when the machine was off across its slot needs both `-s` and `-t`.

**The command is a list of words, not a shell line.** There is no pipe, no redirection and no `&&`;
a program that writes a file takes a flag naming it, which is why `kdos update check` has `--out`.
Quotes are not interpreted either: the line is split on spaces, so `-- kdos notify "Good morning"`
passes `"Good` and `morning"` as two arguments. A program that needs a sentence reads it from a file
of its own, which is what `kdos remind` does. This is what lets the table be parsed rather than run
by a shell, so a line cannot run anything its author did not write.

**Errors.** A line with no `--`, no command, or a name containing anything but letters, digits,
`_` and `-` is reported at boot and skipped. A line whose command is not installed is skipped with
a `[SKIP]` line. Nothing is guessed at: a job that silently never started looks exactly like one
that has not fired yet.

The jobs that ship in the system table:

| File | Runs | When | Why then |
|---|---|---|---|
| `10-update-check.timer` | `kdos update check --json --out /var/lib/kdos/update.json` | 04:17, slack 8 h | Not midnight, when every other machine runs its jobs |
| `20-fstrim.timer` | `fstrim -a` | 12:47, slack 20 h | See below |
| `30-fwupd-refresh.timer` (added by the `fwupd` package) | `fwupdmgr refresh` | 13:43, slack 20 h | The firmware metadata, so `fwupdmgr get-updates` has something to compare against |

`fstrim` tells an SSD which blocks are free. The installer mounts ext4 and XFS without the
`discard` option, so without this job the drive's write speed and wear degrade as it fills; btrfs
and f2fs discard on their own, and a device that cannot discard is skipped. The slot is at midday
because `fstrim` has no timefile to pass to `-t`, so a slot the machine was off for is missed — and
a desktop switched off every night would never reach an overnight one.

### Your own jobs

Put your own jobs in `~/.config/kdos/timers.d/*.timer`, in the same format. Your desktop session
starts them when you log in and stops them when you log out: a job that writes into your home
directory should not outlive the login that started it. While you are logged in they repeat like
the system ones — each line runs in a loop that restarts `snooze` after every run.

Differences from the system table:

- A line that does not parse, whose command is not installed, or whose timespec `snooze -n` rejects
  is skipped **silently**. If a job never fires, check the line with `snooze -n <timespec>`, which
  prints the next five times it would run.
- The job's output is discarded.
- A job that must run every night whether or not anyone is logged in belongs in the system table.

Give each job a minute of its own. A laptop that wakes after its slots have passed runs every job
whose slack covers the gap at the same moment, and jobs that start together compete for one disk.
That is why the backup job below sits at 03:40 and not beside the 04:17 update check.

Two jobs ship in the skeleton copied into each new home:

| File | Runs | State |
|---|---|---|
| `20-updatedb.timer` | `kdos-updatedb` at 03:05 | Enabled — see [Finding a file by name](#finding-a-file-by-name) |
| `10-backup.timer` | `kdos-backup --once` at 03:40 | Commented out: a first backup of a home directory can take hours, and should be something you chose — see [Backups](#backups) |

## The session on tty1

Logging in on the first console, `tty1`, starts the desktop. `/etc/inittab` gives `tty1` to
`kdos-getty`, which loads the console font and colours and then runs `kdos-login`. `kdos-login`
reads `/etc/kdos/login.conf` and either logs in the account it names without asking or shows the
normal password prompt. `tty2` is a plain login prompt: the recovery console to use when the
desktop does not come up.

`/etc/kdos/login.conf` has one key:

| Key | Does |
|---|---|
| `autologin` | The account `tty1` logs in without a password. Shipped as `autologin = kdos`. Commented out, `tty1` asks for a password |

Change it with `kdos-users`, or `kdos-power autologin <user>` or `kdos-power autologin off`
(members of `wheel`). Those refuse an account that cannot log in, which would leave a machine that
boots to a login nobody can complete. This is the only place the desktop account is named, and the
installer writes it when you choose a user name. If `kdos-login` is missing, `kdos-getty` starts a
plain autologin prompt itself, using the same `autologin` key, so a broken login program still
leaves you a console.

## Users and groups

One account ships: `kdos` (UID 1000), a member of `wheel`, `seat`, `users` and the hardware groups
below. The installer asks for its password; on the live image it is `kdos`.

- **`wheel`** is what `sudo` and polkit grant on, and what the root daemons check (through the
  socket's peer credentials) before accepting a configuration change. The `sudo` grant is the one
  line `%wheel ALL=(ALL) ALL` in `/etc/sudoers.d/00-sudo`, which asks for your own password.
- **`seat`** alone reaches suspend, power-off, reboot and removable-media mounting. An account in
  `seat` but not `wheel` can run a desktop and power it down, but cannot administer the machine.

The hardware groups of the `kdos` account, and what removing each would silently stop:

| Group | Makes usable without root |
|---|---|
| `dialout` | Serial adapters, debug probes, SDRs, instruments, security keys, cameras, scanners, phones — see [the udev rules](#the-device-groups-and-the-udev-rules) |
| `audio` | Sound devices |
| `video` | The GPU's display nodes, screen and keyboard backlight, and a monitor's DDC/CI line |
| `render` | GPU rendering and compute |
| `input` | Input devices and game controllers' raw HID |
| `kvm` | Hardware virtualisation for QEMU |
| `cdrom` | Optical drives |
| `lpadmin` | Adding and managing printers in CUPS |
| `tty` | Opening a virtual terminal from a backgrounded session: `/dev/tty0` is `0620 root:tty`, and a backgrounded session has no controlling terminal of its own, so without this group the desktop is refused a terminal |

**Adding a user.** `kdos-users` lists accounts and their groups and sets autologin; it does not
create accounts. Use the `shadow` tools:

```sh
sudo useradd -m -G wheel,seat,audio,video,render,input -s /bin/bash alice
sudo passwd alice
```

Add the other groups from the table as needed. Two things to check afterwards:

- **Boxed applications** run rootless podman, which needs a range in `/etc/subuid` and
  `/etc/subgid` for the account; `grep alice /etc/subuid` shows whether `useradd` added one. The
  shipped files give `kdos` the range starting at 100000 and `root` the one starting at 200000,
  65536 IDs each, so a range added by hand for another account must not overlap those.
- **`/run/user/<uid>`** is created by `15_userdirs` at boot for the accounts that exist then. For an
  account added since, run `sudo service start userdirs` or reboot before its first desktop login.

## Storage

**Filesystem checks at boot.** Before anything is mounted, `rcS` runs `fsck -A -R -T -a`: every
`/etc/fstab` filesystem with a non-zero pass number is checked and repaired where that is safe,
except the root, which the initramfs checked before mounting it. The report goes to
`/run/kdos-fsck.log`. A filesystem left with uncorrected errors is still mounted, and the boot
splash says so, rather than the machine stopping behind the splash; read the log and repair it by
hand from `tty2`. A filesystem on an LVM volume group that the initramfs did not activate is
checked and mounted later by `03_lvm`, which appends to the same log.

**Swap.** `rcS` runs `swapon -a` at boot, after `mount -a`, so any swap partition or swapfile in
`/etc/fstab` is used. The installer can create a swapfile and writes its `fstab` line.

**zram** is compressed swap held in RAM, set up by `12_zram` from `/etc/kdos/zram.conf`:

```ini
size = 50
algorithm = zstd
```

| Key | Means | Default |
|---|---|---|
| `size` | How much swap the device may claim to hold, as a percentage of RAM (1–90). It is not the memory the device occupies, since compressed pages live in that same RAM. A value outside 1–90 is reported and replaced with 50 | `50` |
| `algorithm` | The compression algorithm. One the running kernel lacks is reported, and the kernel's default is used | `zstd` |

The device is used ahead of disk swap (priority 100). Once it is up, `12_zram` changes two kernel
settings, and `service stop zram` puts both back:

- **zswap is turned off.** zswap is a compressed cache in front of whatever swap exists; in front of
  zram every page would be compressed twice. A machine whose zram did not come up keeps zswap in
  front of its disk swap.
- **`vm.page-cluster` is set to 0**, turning off swap read-ahead, which suits a disk's seek time
  but on zram only decompresses pages nobody asked for. `service stop zram` restores the default of
  3. The setting covers every swap device, which is why it is not in `/etc/sysctl.conf`.

**Removable media** are mounted by the `kdos-mountd` root daemon, which you reach through
`kdos-devices` (`Super+F6`) or the `kdos-mount` command. A mounted device appears under
`/media/<user>/<label>` (the device name when it has no label, with any character other than
letters, digits, `.`, `_` and `-` removed), private to you and removed again on unmount. Every
removable mount is `nosuid,nodev`, and `noexec` by default: a setuid program on someone else's USB
stick would otherwise be a way to become root. Create `/etc/kdos/mountd.conf` (not shipped) to
change that:

| Key | Effect |
|---|---|
| `exec = yes` | Mount removable media without `noexec`, so programs on them can run |
| `format = yes` | Allow formatting a removable device |
| `write = yes` | Allow writing a disk image over a whole removable disk |

The daemon never offers an internal disk, a filesystem the kernel cannot mount, anything already
mounted, anything named in `/etc/fstab`, or the medium the system booted from. Members of `seat` or
`wheel` may use it.

The client never names a device or a mount point: it asks for a row number from the list the
daemon published, and the daemon decides the rest.

```sh
kdos-mount list              # the removable devices, one numbered row each
kdos-mount mount 0           # mount row 0; prints the mount point
kdos-mount unmount 0
kdos-mount smart 0           # the disk's SMART health verdict (smartctl -H)
kdos-mount write 0 kdos.iso sdb   # kdos.iso over all of sdb, read back and compared
kdos-mount shares            # the network shares connected now
kdos-mount browse            # file servers that answered an mDNS or NetBIOS broadcast
```

The same daemon unlocks a LUKS-encrypted stick, formats a device when `format = yes` allows it,
writes an image over a whole stick when `write = yes` allows it (the image is opened as you and
handed to the daemon open, and the disk's name is typed back to confirm), and connects SMB shares,
including with a Kerberos ticket from `kinit`
(`kdos-mount krb5 <server> <share> <user|-> <domain|->`). The passphrase or share password is
typed into `kdos-disks` or `kdos-connect`, never on a command line. Every verb and refusal is in
[The daemons](../04-programs/daemons.md#kdos-mountd).

**Disks in the native applications.** The applications ported natively (Dolphin's device list,
GNOME Disks, K3b, the Impression image writer, the file choosers of GTK and Qt) do not use
`kdos-mountd`. They reach disks through udisks2, with gvfs on top for GTK. `udisksd` has no
service script: the system bus starts it on the first request. It mounts under
`/run/media/<user>`, and every request is decided by polkit, whose rule in
`/etc/polkit-1/rules.d/50-kdos.rules` grants mounting, unlocking, ejecting, formatting and raw
device access to members of `wheel` without a password. With no authentication agent to ask for a
password, an account outside `wheel` is refused. `kdos-mountd` remains the authority for the panel
and `kdos-devices`.

## Networking

NetworkManager manages the network, with `wpa_supplicant` for wireless, ModemManager for mobile
broadband and a local `dnsmasq` for DNS. The polkit rule in `/etc/polkit-1/rules.d/50-kdos.rules`
lets members of `wheel` scan, connect, forget networks, switch the radio and share a connection
without a password. It deliberately leaves out setting the hostname, setting machine-wide DNS and
reloading NetworkManager's configuration; those need `sudo`.

| Tool | For |
|---|---|
| `kdos-net` (`Super+F4`) | The desktop network manager: networks, connecting, forgetting, the hotspot |
| `kdos-netagent` | The passphrase box NetworkManager raises; started with the session |
| `nmtui` | The full text interface |
| `nmcli` | Scripting |

**Connecting to wireless.** Open `kdos-net`, choose the network and type the passphrase; it is
saved in the connection profile.

Any later question comes from `kdos-netagent`. NetworkManager never prompts by itself: when a saved
secret is missing or refused, it asks the password agents registered with it, and fails the
connection silently if none answers. So a changed wireless key, 802.1X enterprise wireless and a
VPN one-time code all arrive as a box from the agent, not from the window you started in. The agent
stores nothing; only a request that allows interaction raises a box.

**DNS.** NetworkManager starts a caching `dnsmasq` on `127.0.0.1` and `::1` and feeds it the
servers of each connection, as set by `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf`. DNS is
split: a VPN's servers answer for the VPN's own domains rather than replacing every server the
machine has. A file of the same name in `/etc/NetworkManager/conf.d` overrides the shipped one.
`/etc/resolv.conf` has one writer, `resolvconf` (openresolv), which NetworkManager, the fallback
`dhcpcd` and `wg-quick` all go through, so none of them overwrites another's entries.

**VPNs.** OpenVPN works through NetworkManager with certificate and password authentication;
PKCS#11 hardware tokens are not supported by the OpenVPN build. WireGuard works two ways:

- `nmcli connection import type wireguard file wg0.conf` makes a NetworkManager profile, and its
  `DNS=` servers go to the local `dnsmasq` for the tunnel's domains.
- `wg-quick up wg0` hands a `DNS=` line to `resolvconf`, which makes the tunnel's servers the only
  ones in `/etc/resolv.conf` until `wg-quick down wg0`.

**Mobile broadband** is a NetworkManager connection like any other. ModemManager finds a built-in
WWAN card or a USB modem (MBIM, QMI, QRTR or AT). Connect it with
`nmcli connection add type gsm ifname '*' apn <apn>`, or with `nmtui`'s mobile broadband entry,
which offers carriers from `mobile-broadband-provider-info`. `kdos-netagent` asks for a SIM PIN.
`mmcli -m any` shows the modem as ModemManager sees it; members of `wheel` can unlock and enable a
modem and read its text messages there without `sudo`.

**Other connection types:**

| Connection | Command |
|---|---|
| PPPoE (DSL), carried by `pppd` | `nmcli connection add type pppoe ifname ppp0 pppoe.parent <ethernet> username <user> password <password>` |
| A phone's Bluetooth tethering | `nmcli connection add type bluetooth bt-type panu bluetooth.bdaddr <phone>`, after pairing the phone in `kdos-bt` |
| A phone that offers only dial-up over Bluetooth | The same with `bt-type dun`, plus an APN |

### An access point with hostapd

`kdos-net`'s hotspot is the quick way to share a connection, and needs none of this. `hostapd` is
for a machine that *is* the network — an access point that stays up across reboots, with or without
an uplink:

1. Write `/etc/hostapd/hostapd.conf`, starting from `hostapd.conf.example` beside it. `46_hostapd`
   starts `hostapd` at boot only when that file exists.
2. Tell NetworkManager to leave the wireless card alone, or both will try to drive it: create a file
   in `/etc/NetworkManager/conf.d` containing

   ```ini
   [keyfile]
   unmanaged-devices=interface-name:<interface>
   ```

3. Give the interface an address, and run a `dnsmasq` of your own on it for DHCP and DNS
   (`interface=`, `bind-interfaces`, `dhcp-range=`), from an init script of your own in
   `/etc/init.d`. `bind-interfaces` is required: NetworkManager's `dnsmasq` already answers on
   `127.0.0.1:53` and `::1:53`, and a second one without it tries the wildcard address, fails with
   the port in use, and serves no clients.
4. Let the clients through the firewall. The shipped rules open DHCP and DNS only for
   NetworkManager's hotspot range `10.42.0.0/16`, so add a file in `/etc/nftables.d` that accepts
   `udp dport { 53, 67 }` and `tcp dport 53` with `iifname "<interface>"` in the `input` chain —
   and, if clients should reach anything beyond this machine, accepts forwarding from that
   interface. See [Adding your own rules](#adding-your-own-rules).

### Syncing and serving on the network

Three programs answer other machines. Each has a name in [the firewall](#the-firewall) that must be
turned on first; the shipped policy blocks them all.

| Program | Start it with | Firewall name |
|---|---|---|
| Syncthing | **File Sync** in the menu, which runs `kdos-syncthing` in a terminal and opens the web interface on `127.0.0.1:8384`. Closing the terminal stops syncing | `syncthing`, on both machines |
| mosh | `mosh <user>@<host>` from the other machine; `mosh-server` is started over SSH | `ssh` and `mosh` |
| Caddy | `caddy run --config /etc/caddy/Caddyfile`, which serves `kiwix-serve` (expected on port 8080) over HTTPS on port 8443, with a certificate from Caddy's own local authority | `caddy` |

`kdos-syncthing` gives a user who has no Syncthing configuration one with local discovery on, and
global discovery, relays, NAT traversal and usage reporting off: two machines on one network find
each other, and nothing reaches a server outside it. An existing configuration is left as it is.

## The firewall

`/etc/nftables.conf` holds the firewall policy, and `25_nftables` loads it before the network
starts, so the machine is never on the network unfiltered.

What the shipped policy does:

| Traffic | Rule |
|---|---|
| Incoming | Dropped unless a rule below accepts it |
| Forwarded | Dropped unless a rule below accepts it |
| Outgoing | Accepted |
| Replies to connections this machine opened | Accepted, incoming and forwarded |
| Invalid packets | Dropped |
| Loopback | Accepted |
| ICMPv6 | Accepted: destination-unreachable, packet-too-big, time-exceeded, parameter-problem, echo-request, router advertisements, neighbour solicitation and advertisement |
| ICMP | Accepted: destination-unreachable, time-exceeded, parameter-problem, echo-request |
| mDNS (UDP 5353), DHCPv6 client (UDP 546) | Accepted |
| NetBIOS name replies (UDP 137 to 137) | Accepted |
| DNS (UDP and TCP 53) and DHCP (UDP 67) from `10.42.0.0/16` | Accepted |
| Anything to or from `10.42.0.0/16` | Forwarded |
| DNS (UDP and TCP 53) from a `podman*` bridge | Accepted |
| From a `podman*` bridge, or to one through a published port | Forwarded |

Why the less obvious rows are there:

- **ICMPv6** is answered because dropping it does not make IPv6 safer — it breaks neighbour
  discovery and path MTU discovery.
- **`10.42.0.0/16`** is `kdos-net`'s hotspot. NetworkManager gives each shared connection a `/24`
  from that range and runs `dnsmasq` on it. A packet this firewall drops cannot be let through by
  NetworkManager's own rules, so without these rows a client joins the hotspot and never gets an
  address. Outside that range the ports stay closed.
- **`podman*`** covers rootful podman's bridge networks — `sudo podman run`, rootful distrobox and
  any network `podman network create` makes. These rows live in `/etc/nftables.d/40-podman.nft`.
  Without them a container gets an address, reaches nothing, and cannot resolve names, because
  `aardvark-dns` answers on the bridge's own address. Rootless boxes forward nothing and need
  neither.
- **NetBIOS** is a reply, not a service. Name lookups on a Windows network are broadcasts that
  every machine answers from its own address, so the firewall cannot match them as replies to a
  connection. Without this rule `kdos-mount browse` finds no file servers. Nothing on the image
  listens on port 137.

### Opening a service

Anything that should be reachable from another machine needs a rule — sshd, a shared printer and a
served offline library are all blocked by default. There are two ways, and they do not interfere
with each other.

**By name, with `kdos-firewall`** (the Firewall tool in Settings, under System) or the command
`kdos-power firewall`:

```sh
kdos-power firewall list          # every name, and whether it is on
kdos-power firewall ssh on
kdos-power firewall ssh off
```

| Name | Opens |
|---|---|
| `ssh` | TCP 22 |
| `http` | TCP 80 |
| `https` | TCP 443 |
| `ipp` | TCP 631 (sharing a printer) |
| `smb` | TCP 445 |
| `kiwix` | TCP 8080 (`kiwix-serve`) |
| `mdns` | UDP 5353 |
| `mqtt` | TCP 1883 and 8883 |
| `xmpp` | TCP 5222 |
| `nfs` | TCP 2049 |
| `babel` | UDP 6696 (Babel mesh routing, `babeld`) |
| `nut` | TCP 3493 (UPS status for other machines) |
| `snmp` | UDP 161 (the SNMP agent, `snmpd`) |
| `mumble` | TCP and UDP 64738 (a Mumble voice server) |
| `caldav` | TCP 5232 (Radicale's calendars and contacts) |
| `mail` | TCP 25, 143, 465, 587 and 993 (Maddy) |
| `irc` | TCP 6667 and 6697 (ngIRCd) |
| `dlna` | TCP 8200, and UDP 1900 for discovery (MiniDLNA) |
| `tryton` | TCP 8000 (the GNU Health server) |
| `caddy` | TCP 8443 (the shipped `Caddyfile`'s site) |
| `mosh` | UDP 60000–61000; also needs `ssh` |
| `syncthing` | TCP and UDP 22000, and UDP 21027 for discovery |
| `kdeconnect` | TCP and UDP 1714–1764 (KDE Connect with a phone) |
| `vnc` | TCP 5900 (`wayvnc`; it binds 127.0.0.1 unless given an address) |
| `xonotic` | UDP 26000 (hosting a Xonotic game) |

Turning a name on or off needs membership of `wheel`. The meaning of each name is fixed inside
`kdos-powerd`, not in the tool you click, so no client can open a port that is not on this list.
`kdos-powerd` rewrites `/etc/nftables.d/50-kdos-services.nft` completely on every change and
reloads the firewall, so do not put your own rules in that file: they disappear at the next
toggle.

### Adding your own rules

Anything else is an edit. `/etc/nftables.conf` carries commented examples in its `input` chain,
and its last line, `include "/etc/nftables.d/*.nft"`, reads every `.nft` file in that directory. A
file of your own there is never touched by `kdos-firewall`. For example,
`/etc/nftables.d/60-local.nft`, opening TCP 3000 for a development server, a port no firewall
name covers:

```text
table inet filter {
    chain input {
        tcp dport 3000 accept
    }
}
```

Add to `table inet filter` and do not declare a table of your own. Reloading deletes and rebuilds
only `inet filter`, so that netavark's `inet netavark` table (rootful containers' address
translation and published ports) and NetworkManager's `nm-shared-*` tables (the hotspot) survive; a
table of your own would survive too, keeping its old rules beside the new ones. Name the chain
without a `type … hook` line — that re-opens the existing chain, while repeating the hook declares a
second chain and the load fails.

Check, then apply:

```sh
sudo nft -c -f /etc/nftables.conf     # check only
sudo service nftables start           # apply
```

`25_nftables` runs the same check before loading, so a file with a mistake is refused as a whole
and the rules already loaded stay in force. `sudo service nftables stop` removes the `inet filter`
table, which leaves the machine unfiltered until the next start.

## Hardware

### Firmware

`linux-firmware` ships whole, installed with upstream's own script so that the alias links drivers
ask for exist. A trimmed subset would be a guess about your hardware, and a wrong guess fails
silently.

`sof-firmware` is separate and is needed for audio on Intel Tiger Lake and newer. It carries both
the DSP firmware and the topology files; firmware without its topology loads and produces no sound.

`wireless-regdb`, the table of which radio channels and power levels each country allows, is built
from upstream's `db.txt` and ships with upstream's signature. The kernel checks that signature, and
rejects an unsigned or altered database silently, leaving the radio on the restrictive world
settings: no 5 GHz DFS channels and reduced transmit power. The build verifies the signature
against the database it produced and fails on a mismatch, so an edited `db.txt` never ships.

The country comes from your time zone. The installer and `kdos-power timezone <Area/City>` both
write the zone's country code to `/etc/modprobe.d/kdos-regdom.conf` as cfg80211's
`ieee80211_regdom`. `kdos doctor` warns when the radio is still on the world domain `00`, and
`sudo iw reg set <CC>` changes it until the next boot.

### Microcode

CPU microcode is applied by the kernel's early loader, which runs before any filesystem exists, so
it is carried as an uncompressed archive in front of the initramfs: Intel's from the `intel-ucode`
package and AMD's from `linux-firmware`. Late loading is off in the kernel, so this is the only way
microcode is applied.

```sh
kdos doctor        # whether the initramfs carries this CPU's microcode, and the running revision
```

The check exists because an initramfs rebuilt without microcode has no symptom: the processor
keeps whatever revision the firmware loaded.

### The device groups and the udev rules

Opening a device as a normal user takes two things: membership of a group, and a udev rule that
gives that group the device. Neither works alone. The KDOS rules live in `/etc/udev/rules.d`:

| Rules file | Devices | Group |
|---|---|---|
| `70-kdos-serial.rules` | USB serial adapters — FTDI, CP210x, CH341, CDC-ACM | `dialout` |
| `70-kdos-debug.rules` | In-circuit debuggers and programmers, raw USB and HID: CMSIS-DAP from any vendor, ST-Link, J-Link, Atmel-ICE, PICkit, USBasp, USB-Blaster, XDS110, Nu-Link, KitProg and the rest of openocd's and openFPGALoader's cables | `dialout` |
| `70-kdos-sdr.rules` | Software-defined radio front ends: every RTL2832U stick librtlsdr knows, HackRF, Airspy, bladeRF, SDRplay and Mirics, LimeSDR, the USRP B-series, ADALM-Pluto, the FUNcube Dongles (their HID node too), Perseus, RFNM and Fobos | `dialout` |
| `70-kdos-usbtmc.rules` | USB Test & Measurement: scopes, meters, function generators — `/dev/usbtmc*` and the raw USB device pyvisa-py opens | `dialout` |
| `70-kdos-sigrok.rules` | Every logic analyser, scope and meter libsigrok's `60-libsigrok.rules` marks | `dialout` |
| `70-kdos-gpio.rules` | `/dev/gpiochip*`, for libgpiod's tools and the GPIO cables of openocd and openFPGALoader | `dialout` |
| `70-kdos-fido.rules` | FIDO2/U2F security keys, for `ssh-keygen -t ed25519-sk` and the `fido2-*` tools | `dialout` |
| `70-kdos-camera.rules` | PTP/MTP cameras, for gphoto2 | `dialout` |
| `70-kdos-scanner.rules` | Flatbed and sheet-fed scanners, for SANE | `dialout` |
| `70-kdos-ptouch.rules` | Brother P-touch label printers, which `ptouch-print` drives over raw USB | `lpadmin` |
| `70-kdos-gamepad.rules` | Game controllers' raw HID, for SDL's HIDAPI drivers | `input` |
| `70-kdos-i2c.rules` | The DDC/CI line of a display controller, for `ddcutil` | `video` |
| `70-kdos-backlight.rules` | The panel's brightness for `kdos-osd`, and the keyboard backlight where there is one | `video` |

One more file there grants no group: `99-qemu-tablet.rules` marks QEMU's emulated USB tablet as a
plain pointer, so in a virtual machine the pointer is handled as a mouse rather than as a graphics
tablet or a joystick.

Packages add two more: `libmtp`'s rules give phones to `dialout`, and `gpsd`'s
`/usr/lib/udev/rules.d/70-kdos-gpsd.rules` handles GPS receivers (below).

Every rule that names a device node sets `MODE="0660"`, readable only by the owner and group, so
other accounts on a shared machine cannot read these devices.

Some rules are deliberately narrow:

- **i2c** matches only adapters whose PCI parent is a display controller
  (`ATTRS{class}=="0x03*"`). `/dev/i2c-*` also covers the chipset's SMBus, where every memory
  module's configuration chip sits, and a stray write there can leave a machine that will not boot.
  `ddcutil` ships a broader rule of its own, which its package removes.
- **gpio** is not narrowed and does not need to be: the kernel refuses a line a driver already
  holds, and a GPIO keeps no state across a reset.
- **backlight** carries no `GROUP=` or `MODE=`, because a backlight has no node in `/dev` and udev
  discards such a rule whole. It runs `chgrp video` and `chmod 0664` on the `brightness` file
  instead, on the `add` event that `01_udev`'s coldplug replays at boot.

Upstream rules that grant access through a `plugdev` group or `uaccess` are not installed: there is
no `plugdev` group, udev turns an unknown group into root, and nothing here uses `uaccess`. That is
why libfido2's `70-u2f.rules` and the rules of rtl-sdr, hackrf, airspy, bladeRF, openocd and
openFPGALoader are absent, and the KDOS files above cover their devices. stlink's are removed
because they make every ST-Link writable by every account (`MODE:="0666"`). Where upstream
separates identifying a device from granting it, the identification is kept and the grant is
KDOS's: libsigrok's `60-libsigrok.rules` sets `ID_SIGROK`, sane-backends' `20-sane.hwdb` and
`65-libsane.rules` set `libsane_matched`, and eudev's `fido_id` sets `ID_SECURITY_TOKEN`; each
`70-kdos-*` file turns its mark into the group. So the FIDO rule covers every key whose HID
descriptor declares FIDO, and the scanner rule every scanner a SANE backend supports.

**Modules loaded by name.** Some drivers declare nothing udev can match, so files in
`/etc/modules-load.d` load them at boot through `02_modules`:

| File | Loads | Without it |
|---|---|---|
| `kdos-i2c.conf` | `i2c-dev` | No `/dev/i2c-*`, and the i2c rule has nothing to grant |
| `kdos-hwmon.conf` | `nct6775` (Nuvoton), `it87` (ITE), `drivetemp` (SATA disk temperature) | `sensors` and `kdos-res` show CPU and GPU temperatures only — no fans, voltages or board temperatures |
| `kdos-containers.conf` | `tun`, `overlay` | Container networking and overlay storage |
| `kdos-drm.conf` | `virtio_gpu`, `qxl`, `bochs`, `vmwgfx` | Virtual machines' display drivers |

Each hwmon driver binds only a chip it finds; on other hardware, or where the firmware has claimed
the chip, the load fails and `02_modules` logs a skip. `sensors-detect` is not shipped, because it
probes the SMBus by writing to it. `fancontrol` is shipped, and nothing starts it.

**One blacklist.** `/etc/modprobe.d/kdos-sdr.conf` blacklists `dvb_usb_rtl28xxu`. Otherwise the
kernel claims an RTL2832U dongle as a TV tuner when it is plugged in, and SDR programs cannot open
it even though `lsusb` lists it.

**Software-defined radio.** Each radio the SDR rule covers has its own library and tools — `rtl-sdr`,
`hackrf`, `airspy` and `bladerf` — and a SoapySDR module: `soapyrtlsdr`, `soapyhackrf`,
`soapyairspy` and `soapybladerf`, in `/usr/lib/SoapySDR/modules0.8`. `SoapySDRUtil --find` lists
what is plugged in, and a SoapySDR program reaches it, for example `rtl_433 -d driver=hackrf`. A
bladeRF streams nothing until its FPGA image is loaded; that image is Nuand's and is not shipped,
so put it in `~/.config/Nuand/bladeRF/` or `/etc/Nuand/bladeRF/`, or let a bladeRF 2.0 load it from
its own flash. SDRplay receivers are granted but have no driver, because SDRplay's API is a closed
library.

**Drawing tablets** need no group. The compositor opens them through libinput, which uses
`libwacom`'s database to pair the stylus with its pad, group the pad's buttons and rings, and tell
a screen tablet from a desk one. `libwacom-list-local-devices` shows what it recognised. A model
missing from the database works as a plain absolute pointer. To add it, describe it in a `.tablet`
file under `/etc/libwacom`, then run `libwacom-update-db --skip-systemd-hwdb-update` followed by
`sudo udevadm hwdb --update` (the tool's own last step would call `systemd-hwdb`, which this system
does not have).

**GPS receivers.** Plugging one in starts `gpsd`; no init script runs it at boot. `gpsd`'s
`70-kdos-gpsd.rules` runs `gpsd.hotplug` for receivers whose USB ID identifies them — u-blox,
Garmin, DeLorme, the Holux MediaTek, the Telit module — and for the kernel's `gnss` devices, and
`gpsdctl` adds the device to a running `gpsd` or starts one. Upstream's rule also matches the
generic CP2102 and ATEN serial chips, which would make `gpsd` probe every ESP32 board and USB serial
adapter, so it is not installed. Hand over a receiver behind a generic chip with
`sudo gpsdctl add /dev/ttyUSB0`; until something is added, `localhost:2947` refuses connections.

Without the group and the rule, a serial programmer, development board, instrument or GPS receiver
looks like a broken cable rather than a permission problem. `kdos doctor` checks the serial
(`/dev/ttyUSB*`, `/dev/ttyACM*`), video capture (`/dev/video*`) and USBTMC (`/dev/usbtmc*`) nodes
that are present, lists the ones you cannot open, and names the group that owns each.

### Bluetooth

`bluetoothd` is started by `60_bluetooth`. `kdos-bt` (`Super+F5`, or Bluetooth in Settings, under
Hardware) scans, pairs, trusts and connects devices, and registers the pairing agent: without an
agent to confirm the passkey, the service refuses to pair a keyboard. `kdos-audio` (`Super+F3`) has
a Bluetooth pane as well, for connecting a headset. `bluetoothctl` is the same control from a
terminal, for scripting. Which profile and codec a headset uses is PipeWire's decision, not the
pairing tool's; see [kdos-shell](../04-programs/kdos-shell.md#kdos-bt).

### Thunderbolt, phones, smart cards and firmware updates

None of these runs under the supervisor. `boltd` and `fwupd` are started by the system bus on
first use and run as root through `dbus-daemon-launch-helper`; `libmtp` and `opensc` are
libraries; a phone's storage is mounted by a FUSE process of your own.

**Thunderbolt.** Where the firmware's Thunderbolt security level is `user` or `secure` — the
default on most business laptops — a TB3 or TB4 dock is detected and then never authorised, so its
ports stay dead until something approves it. `boltd` approves a dock already enrolled: `43_boltd`
starts it at boot on a machine with a Thunderbolt controller, and a udev rule starts it when a
Thunderbolt device appears later. Enrol a new dock with `sudo boltctl enroll <uuid>`. `boltd` is
built to trust `wheel` and ships a polkit rule for it, but that rule also requires an active
session, which nothing on KDOS provides, so the command needs `sudo` — see
[Security model](../03-architecture/security-model.md).

**Firmware updates.** `fwupd` updates the firmware of SSDs, docks and peripherals from the LVFS, the
Linux Vendor Firmware Service. Run `sudo fwupdmgr get-updates` to see what is available and
`sudo fwupdmgr update` to apply it; the metadata is refreshed daily by `/etc/kdos/timers.d/30-fwupd-refresh.timer`.

System firmware — the BIOS, the embedded controller, and anything the vendor ships as a UEFI
capsule — is applied at the next boot. fwupd places the capsule on the EFI system partition, which
it expects at `/boot/efi` (`EspLocation` in `/etc/fwupd/fwupd.conf`, matching the installer's
`fstab`). Then either the firmware picks it up from disk, or the machine boots once into
`fwupdx64.efi` (from `fwupd-efi`, placed in `EFI/kdos` beside the kernel), which hands it over.
fwupd's usual way of finding that partition is to ask udisks; the `fwupd` package is patched to
describe the FAT filesystem mounted at `EspLocation` from sysfs and the udev database instead. The
live image mounts no EFI partition, so there only devices updated over USB are offered. The loader is unsigned, which is
no extra restriction, since KDOS already requires Secure Boot to be off (see
[Known gaps](../06-reference/known-gaps.md#hardware-and-platform)).

**Phones.** `libmtp` recognises a phone and gives its USB device to `dialout`. To browse its
storage, unlock the phone, set it to file transfer, and open Android File Transfer from the Start
menu, which copies to and from the phone with no mount. For a directory instead, run
`aft-mtp-mount ~/Phone` (from `android-file-transfer`); any file manager can then read `~/Phone`,
and `fusermount3 -u ~/Phone` unmounts it. `kdos-mountd` does not offer MTP devices, so this is
always by hand. libmtp's own `mtp-*` tools reach files by numeric ID only. `gphoto2` and `libgphoto2` cover still cameras in PTP
mode.

**Smart cards.** `pcscd` and `ccid` reach the reader, and `opensc-pkcs11.so` speaks PIV, CAC,
OpenPGP and national ID cards. Its `opensc.module` registers it with p11-kit, so openconnect accepts
a `pkcs11:` URI and GnuTLS programs see the card's certificates; `ssh -I /usr/lib/opensc-pkcs11.so`
uses it directly. `pkcs11-tool --list-slots` tells you whether the card is seen at all. Members of
`wheel` reach smart cards without `sudo`.

## Printing

CUPS is the print system, started by `80_cups`. The installer turns it off by default, so on an
installed machine enable it before the first print job:

```sh
sudo service enable cups
sudo service start cups
```

`kdos-print` (Printers in Settings, under System) lists the queues and their jobs, and adds a
printer CUPS has discovered. It runs as you, not through a root daemon: CUPS is built with
`lpadmin` as its system group, so membership of `lpadmin` is the authority to manage printers, and
the `kdos` account has it. It sets a printer up only as IPP Everywhere, the driverless protocol a
printer advertises and CUPS configures without a PPD file; a printer that needs a vendor driver is
refused, and the window says so. The same from a terminal:

```sh
lpinfo -v                                        # the device URIs CUPS can reach
lpadmin -p <name> -v <uri> -m everywhere -E      # add and enable a driverless queue
lpstat -p -d                                     # the queues, and the default
```

`81_cups-browsed` adds printers shared on the network to CUPS as they appear, and `82_ipp-usb`
serves each IPP-over-USB printer on localhost as it is plugged in, so a USB printer is discovered
the same way. Sharing a printer with other machines needs the `ipp` name in
[the firewall](#opening-a-service). How `kdos-print` is built is in
[kdos-shell](../04-programs/kdos-shell.md#kdos-print).

## Media, colour and time

**ffmpeg** is built with the full codec set: H.264, HEVC, VP8/VP9, AV1 encode and decode, JPEG XL,
MP3, Opus, Vorbis, FLAC, LC3, subtitle burn-in, HDR tone mapping (`zscale`, `libplacebo`),
high-quality resampling (`soxr`), time-stretch (`rubberband`), SVG input (`librsvg`), text
recognition (the `ocr` filter, through `tesseract`), QR codes (`qrencode` and `qrencodesrc`), the
OpenCL filters (`tonemap_opencl`, `nlmeans_opencl`, `unsharp_opencl`, `overlay_opencl` and the
rest) and hardware acceleration through VA-API and Vulkan. There is one encoder per format. There
is no `ffplay`, because it needs SDL, and SDL depends back on ffmpeg through PipeWire.

The OpenCL filters run on Mesa's rusticl, which offers an Intel or AMD GPU because
`/etc/profile.d/50-opencl.sh` sets `RUSTICL_ENABLE`; a machine with neither has no OpenCL device.

The build passes `--enable-gpl` (which x264, x265 and rubberband require) together with
`--enable-version3`, and ffmpeg's `configure` licenses that combination as GPL version 3 or later.
Everything that links it inherits that licence. `ports/core/ffmpeg/LICENSE.notice` is the notice a
redistributor should read.

**Hardware video decoding** goes through VA-API, and the driver depends on the GPU:

| GPU | Driver |
|---|---|
| AMD (r600, radeonsi), NVIDIA through nouveau, virgl | Mesa's `gallium-va` |
| Intel, Broadwell and newer | `intel-media-driver` (`iHD`), over `intel-gmmlib` |
| Intel Sandy Bridge, Ivy Bridge, Haswell | `libva-intel-driver` (`i965`) |

libva tries `iHD` first and falls back to `i965`, so each machine loads the one that serves it.
With no driver, ffmpeg and mpv silently fall back to software decoding. `vainfo`, from
`libva-utils`, lists the profiles the loaded driver offers on this machine.

**mpv** is the native player. Its `--vo=gpu-next` renderer uses Vulkan where a device answers and
OpenGL through EGL otherwise. An external subtitle in a legacy encoding — CP1251, GBK, Shift-JIS —
is detected by `uchardet` and converted, and `libass` breaks long subtitle lines where Unicode
allows (through `libunibreak`), not only at spaces. The `mpv-mpris` plugin, loaded from
`/etc/mpv/scripts` for every user, puts mpv on the session bus, so the media keys and the panel's
now-playing display control it.

**Optical discs.** `mpv` opens `dvd://`, `bd://` and `cdda://`, and plays a DVD's titles and
chapters without its menus. `mpd` and `cmus` play audio CDs; `cd-paranoia` rips one with verified
reads, and `ffmpeg -f dvdvideo -i /dev/sr0` rips a DVD title with its chapters. GStreamer has
`cdiocddasrc`, `dvdreadsrc` and `rsndvdbin`, and `rsndvdbin` plays DVD menus. CSS-encrypted DVDs
and AACS-encrypted Blu-rays do not open, and Blu-ray Java menus do not run — see
[Known gaps](../06-reference/known-gaps.md#hardware-and-platform).

**Other media abilities:**

- `cmus` plays `.m4a` (AAC and ALAC) through its ffmpeg input, alongside its own decoders for MP3,
  FLAC, Vorbis, Opus and the rest.
- GStreamer's `webrtcbin` is built, with ICE from `libnice` and SRTP from `libsrtp`, for
  peer-to-peer media pipelines.
- `mutool barcode` reads and draws QR codes and barcodes in a page or image, through `zxing-cpp`.
- Every pango text layout on the host — the compositor's window titles, GStreamer's
  `textoverlay` — breaks Thai text at word boundaries found by `libthai`, since Thai puts no spaces
  between words.

**Colour.** `lcms2` is the colour management engine, and the only thing on the host that can apply
an ICC profile.

**Time zones.** The zone data is a compiled zoneinfo tree that musl reads directly, built in the
"fat" format rather than "slim". Applications in containers read the same tree, and a format they
misread would make the host and its containers disagree about local time.

The zone is set with `kdos-time` (Date and time in Settings, under System) or
`kdos-power timezone <Area/City>`, both of which need membership of `wheel`. `kdos-powerd` then
rewrites two files together: `/etc/localtime`, the link programs follow into the zoneinfo tree, and
`/etc/profile.d/20-timezone.sh`, which sets `TZ` for every login shell. A shell opened before the
change keeps the old `TZ` until it is restarted. The same change writes the wireless regulatory
country (see [Firmware](#firmware)). On a machine where nothing has chosen a zone, `rcS` links
`/etc/localtime` to UTC at boot. The clock itself is kept by `chrony` (`35_chrony`), and `kdos-time`
shows whether it is synchronised.

## Keeping it current

```sh
kdos cve                  # which installed versions have known vulnerabilities, offline
kdos cve --json           # the same, as JSON
kdos update check         # what the ports tree pins that is not installed
kdos update apply         # install it: prebuilt package first, else compile (see A/B root slots)
kdos app install <id>     # install a boxed application (to update one, see Applications below)
```

**Vulnerabilities.** `kdos cve` compares your installed versions against a copy of Alpine's security
database shipped in `/usr/share/kdos/secdb.txt`, with no network. It exits 1 when any package is
behind a recorded fix and 0 otherwise. A package that database does not carry is counted as
*unknown*, not clean, and the summary says how many there are. Every run prints the database's
date, and warns when it is more than 180 days old.

**Host packages** are built from *ports* — the recipes in the KDOS source tree (see the
[glossary](../06-reference/glossary.md)). What counts as "an update" is a newer ports tree, so
`kdos update` needs one on the machine. `PORT_REPO` in `/etc/kpkg.conf` names it: a
space-separated list of recipe directories, `/ports/core` by default. The KDOS source tree keeps its
recipes in three directories — `ports/core` for upstream software, `src/packages` and `src/desktop`
for KDOS's own programs — so name all three. For a complete checkout of the KDOS repository at
`DIR`:

```sh
PORT_REPO="DIR/ports/core DIR/src/packages DIR/src/desktop"
```

A stick built with `KDOS_ISO_SOURCES=1` is not such a tree: its copy under `/mnt/iso/sources`
has an empty `ports/` (see [kdos rebuild](#kdos-rebuild)). Without a ports tree `kdos update` says
so and exits 2. The
[ports catalogue](../06-reference/ports-catalogue.md) lists every port by phase and group, and
[How KDOS is built](../05-developer/how-kdos-is-built.md) follows a port from recipe to installed
package.

| Command | Does |
|---|---|
| `kdos update check` | Lists installed packages that differ from the ports tree. `--json` prints JSON; `--out PATH` writes that JSON to a file (the nightly timer uses this for the panel's update badge) |
| `kdos update apply` | Installs each out-of-date package, from the binary host where it has a matching package, otherwise by compiling it. `--dry-run` shows what would happen; `--binhost-only` never compiles; `--root DIR` installs into another root |
| `kdos update theme` | Re-runs the theme generators for your home directory after an artwork upgrade |

A *binary host* (binhost) is a signed directory of prebuilt packages — on a USB stick, an NFS mount
or any local path; `kdos update` never downloads. Name it with `binhost = /path/to/repo` in
`/etc/kdos/update.conf`, or with the `KDOS_BINHOST` environment variable. There is no public binary
host; see [Packaging](../03-architecture/packaging.md#the-binary-host) for making one.

Compiling needs the source archives the new recipes name, and `kpkg` never downloads them: they
must already be in the port directory or in `/var/cache/kpkg/sources`. In a checkout, `make fetch`
places them — it is the only step that uses the network. See
[Developing](../05-developer/developing.md#where-sources-come-from).

**A/B root slots.** On a machine installed with two root slots, `kdos update apply` installs into
the inactive slot, which must be mounted, then installs that slot's kernel with
`kdos-bootctl deploy` and marks the slot to be tried on the next boot. The running system is not
touched, so a bad update can be rolled back by booting the other slot. If the inactive slot is not
mounted it refuses; `--in-place` updates the running root instead and gives up that rollback. See
[Boot and init](../03-architecture/boot-and-init.md) for how a tried slot is confirmed, and
[Known gaps](../06-reference/known-gaps.md) for what the updater does not do.

**Applications** have no separate update command. An application is a stack of images this machine
built, so rebuilding it is the update: `kdos app remove <id>` and then `kdos app install <id>`
rebuild the application's own image. Under the shipped `snapshot = auto` the rebuild reads the same
Debian snapshot as before, because the pulled `debian:trixie-slim` is kept; its packages move to a
newer snapshot only when the catalogue names a later date or this machine pulls a newer
`debian:trixie-slim`. Installing an application
that is already installed builds nothing and is reported as a failure: every image is already
there, `kdos-box create` refuses because the box exists, and the command ends with
`1 of 1 did not install`. See [Applications](applications.md#updating).

## Diagnosing

```sh
kdos doctor        # checks kernel, hardware, session, container and desktop set-up
kdos doctor --json # the same, as JSON
kdos status        # what this machine is and what it is running
kdos restarts      # which running processes still use code an upgrade replaced
kdos why <thing>   # what provides something, and why it is that way
kdos stutter       # why the desktop hiccuped, naming the application
kdos cve           # known vulnerabilities (kdos doctor --cve runs the same report)
```

Run `kdos doctor` first when something is wrong. Each line is `ok`, `warn`, or `skip` with a
reason: much of what it checks cannot be answered in a virtual machine, and reporting those as `ok`
would claim something never tested. It exits 0 when nothing warned and non-zero otherwise, in both
the text and the JSON form, so a script can act on the status alone. `--cve` runs the
vulnerability report instead (see [Keeping it current](#keeping-it-current)).

The checks are grouped into sections:

| Section | Checks |
|---|---|
| Kernel | Landlock, the kernel's unprivileged file and network sandbox that [`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox) uses, is present and enabled; the initramfs carries this CPU's microcode, and the running revision |
| Hardware | The wireless regulatory database is signed and loaded, and the domain is not the world domain `00`; Intel audio firmware (`sof-firmware`); GPU firmware; device nodes you cannot open, naming the group that owns each (see [the udev rules](#the-device-groups-and-the-udev-rules)); the boot medium |
| Boxes | The application packs |
| Session | `WAYLAND_DISPLAY` names a socket that exists; `XDG_RUNTIME_DIR` is set; `kdos-comp`, `kdos-shell` and the wlr desktop portal are running |
| Containers | The mount namespace root is `/`, and `/etc/subuid` and `/etc/subgid` carry your range |
| Desktop | The accent, the foot theme and the KDE palette (`kdeglobals`) are applied; Xwayland is reachable; the setuid bits on `kdos-checkpass`, `kdos-resctl`, `newuidmap` and `newgidmap`, and on `dbus-daemon-launch-helper` with group `messagebus`; `/etc/subuid` and `/etc/subgid` exist; `kdos-powerd`, `kdos-energyd` (or the absence of any RAPL energy counter) and `kdos-oomd` are running, and `fcitx5` when it is installed; the compositor's frame-timing and command sockets exist; `kdos-keys` and the screen-capture portal with `kdos-portals.conf` are installed; `~/.local/bin` is on `PATH` |
| Security | Run as root only: whether the `kdos` account still has the shipped password `kdos` |

Without its setuid bit, `kdos-checkpass` cannot read `/etc/shadow`, so the lock screen refuses every
password and locks you out of your own session, recoverable only from `tty2`; the fix it prints is
`chown root` and `chmod 4755`. The shipped-password check needs to read `/etc/shadow`, so an
ordinary user's run has no Security section at all rather than a check that pretends to have
looked; run `sudo kdos doctor` to get it, and expect the session and container checks of that run
to warn, since root has no desktop session.

`service list` and `service status <name>` show each service's state. After an upgrade,
`kdos restarts` lists the running processes that still use code the upgrade replaced or removed;
each keeps the old version until it restarts. See
[kdos restarts](../04-programs/kdos-command.md#kdos-restarts).

### Logs

`syslogd` (from sysklogd) writes the system log. It reads the kernel's messages itself, so there is
no separate `klogd`. `/etc/syslog.conf` routes them:

| File | Holds |
|---|---|
| `/var/log/messages` | Everything except authentication |
| `/var/log/auth.log` | Logins, `sudo` and other authentication |
| `/var/log/kern.log` | Kernel messages |
| `/var/log/cron` | The `cron` facility; nothing on the image logs there by default |

`syslogd` rotates each file itself at 1 MB and keeps five old copies, so no `logrotate` job exists.
The limit matters on the live medium, where `/var/log` is in RAM and an unbounded log would take
memory rather than disk. Emergency messages are also written to every logged-in terminal.

Three more places hold what the system log does not:

| Where | Holds |
|---|---|
| `dmesg` | The kernel's ring buffer, including messages from before `syslogd` started |
| `/run/kdos-init.<name>.log` | The output of each service script at boot (see [Services](#services)) |
| `/run/kdos-fsck.log` | The boot-time filesystem check (see [Storage](#storage)) |

A supervised daemon's restarts and final exits (`[KDOS] (name) Exited with code …`) appear in its
service's boot log.

## Copying and rebuilding the medium

```sh
sudo kdos clone /dev/sdb        # write this medium to another stick
kdos rebuild /mnt/disk/work     # run the build from a source tree on this machine
```

### kdos clone

`kdos clone` copies the medium this system booted from onto another device, byte for byte, then
reads it back to verify. The boot arrangement is whatever the medium already carries, so the copy
boots exactly what the original boots. With no device named, it lists the devices it would accept.

| Option | Does |
|---|---|
| `--source <path>` | Clone an image file or another device instead of the boot medium. Required on an installed system, which has no boot medium to copy |
| `--extent` | Print the image's exact length and stop |
| `-y`, `--yes` | Do not ask before overwriting. Required when not run from a terminal |
| `--no-probe` | Skip the counterfeit-capacity test (`f3probe`) |
| `--no-verify` | Skip the read-back, and say what that costs |

The length copied comes from the image's own description, not from the device, so a 3 GB image on
a 64 GB stick copies 3 GB. To confirm, you type the device's *name*, not `y`. Before writing
anything it refuses the medium the system booted from, any disk with a filesystem mounted, anything
named in `/etc/fstab`, and any device smaller than the image.

Before writing, `f3probe` tests whether the stick really has the capacity it reports — a counterfeit
stick silently wraps around and loses data. The verify drops the page cache before reading back:
otherwise the kernel would return the bytes it just wrote from memory, not what the flash stored,
and a counterfeit stick would pass.

### kdos rebuild

`kdos rebuild <work-directory>` runs the KDOS build on a booted system, with no network. It looks
for a source tree wherever `KDOS_SOURCES` points, then at `/mnt/iso/sources`, `/kdos` and the
current directory, and accepts one that has `script/kdosbuild.sh`, `ports/core` and
`src/build/kdosbuild`.

A stick built with `make build KDOS_ISO_SOURCES=1` carries a copy under `/mnt/iso/sources`, but
not a complete one: `src/` and `script/` arrive, `ports/` arrives empty, and the `fs/` overlay of
system files, the `Makefile` and the `Dockerfile` are not copied. From that stick alone
`kdos rebuild` finds no ports and stops before copying anything. Even with a complete tree named by
`KDOS_SOURCES`, a full rebuild stops in phase 1, which reads the `fs/` overlay from
`/workspace/fs`. A full rebuild needs a complete checkout on a build machine; see
[Why KDOS](../01-philosophy/why-kdos.md#kdos-can-build-kdos) and
[The kdos command](../04-programs/kdos-command.md#kdos-rebuild).

| Option | Does |
|---|---|
| `--dry-run` | Run the checks and stop |
| `--iso-only` | Rebuild only the packaging phase, which produces the ISO |

The checks are what make it safe to start. The work directory needs about 25 GB, and is refused if
it is on a temporary or overlay filesystem: a live stick's root is RAM, so a rebuild started there
reports gigabytes free, fills memory and dies hours in. It also stops if a compiler, `make`, `bash`,
`tar` or `xz` is missing, and warns that no ISO will be produced if `mksquashfs`, `xorriso` or
`mkfs.fat` is.

## Tuning for this machine

```sh
kdos march probe          # which instruction set levels this CPU has
kdos march run lz4        # build lz4 twice and measure
kdos march report         # the ledger: kept, reverted, unmeasurable
```

`kdos march probe` reports which of the x86-64 instruction set levels beyond the baseline
(`x86-64-v2`, `-v3` and `-v4`) this CPU supports. `kdos march run <port>` builds a port with and
without `-march=` set to the highest of them, runs the benchmark named by the `bench =` line in
that port's recipe against both builds (the median of five runs by default), and keeps the faster
flags only where the gain exceeds 3 per cent plus this machine's own measured noise. A difference
inside the noise is recorded as reverted, and a port with no `bench =` line is reported as
unmeasurable. The decisions are kept in `/var/lib/kdos/march.ledger`, and
`kdos march report` lists reverted ports as prominently as kept ones. On a CPU with only the
baseline level there is nothing to measure, and it says so.

## Backups

`kdos-backup` backs up with [restic](https://restic.net/). Opened from the menu it is a window
listing the snapshots in your repository, newest first: `b` starts a backup (it does not wait for
it to finish) and `r` re-reads the repository. `kdos-backup --once` backs up without a window, waits
for restic, and exits 0 on success, 1 on any failure and 2 on a bad argument; that is what the
timer runs.

Set it up once:

1. Create the repository: `restic -r <repo> init`. It asks for a password twice and does not store
   it anywhere.
2. Put that password, and nothing else, in `~/.config/kdos/backup.pass`, and make it private with
   `chmod 600 ~/.config/kdos/backup.pass`. `kdos-backup` refuses to run while the file is readable
   by anyone else.
3. Edit `~/.config/kdos/backup.conf` (one `key = value` per line, `#` starts a comment):

   | Key | Means | Repeats |
   |---|---|---|
   | `repo` | Where the repository is | No; the last line wins |
   | `include` | A path to back up | Up to eight; more are ignored |
   | `exclude` | A path to leave out | Up to eight; more are ignored |
   | `password-file` | The password file, when it is not `~/.config/kdos/backup.pass` | No; the last line wins |

4. Run `kdos-backup --once` and check that it succeeds.
5. To run it nightly, uncomment the `backup` line in `~/.config/kdos/timers.d/10-backup.timer`.

`--once` stops with a message, before touching restic, if `backup.conf` names no `repo` or no
`include`, or if the password file is missing or readable by others. An enabled timer with no
configuration therefore does nothing harmful.

Restoring is a restic command, run by hand so its options are in front of you:
`restic -r <repo> restore <snapshot> --target <dir>`. `restic help restore` lists the options.

## Mail

Mail is four programs sharing one directory. `~/Mail` is a Maildir (one file per message), and
each program is pointed at it:

| Program | Does |
|---|---|
| `mbsync` | Fetches: makes an IMAP mailbox and `~/Mail` the same, in both directions |
| `notmuch` | Indexes and searches what is there. It never fetches and never sends |
| `aerc` | Reads and writes |
| `msmtp` | Sends. `/usr/sbin/sendmail` and `/usr/bin/sendmail` both link to it |

`notmuch new` is the one command you type. Its `pre-new` hook runs `mbsync -a` before the scan and
its `post-new` hook removes the `new` tag from what arrived, so one command fetches and indexes.

The `pre-new` hook steps aside (exit 0) if `mbsync` is not installed, `~/.mbsyncrc` is unreadable,
or that file names no `Channel` — so an unconfigured machine still indexes. Otherwise a failed
fetch fails `notmuch new` too, because indexing after a fetch that did not happen would report
"0 new messages" and look like an empty inbox.

### Setting up an account

Three files to fill in, all shipped with commented examples:

| File | Holds | Mode |
|---|---|---|
| `~/.mbsyncrc` | The incoming (IMAP) server | 600 |
| `~/.msmtprc` | The outgoing (SMTP) server | 600 |
| `~/.config/notmuch/default/config` | Your own address | 644 |

The two mode-600 files hold, or fetch, a password: `msmtp` refuses to send through an `.msmtprc`
that carries a `password` line and is readable by others. The image build sets both modes in the
skeleton copied into each new home.

`~/.config/aerc/accounts.conf` is not shipped. aerc refuses to start with one that anyone else can
read, and its own setup wizard writes it at mode 600 the first time you run `aerc`, so let the
wizard do it.

Keep passwords out of all of them. `pass` is installed, and the templates show
`PassCmd "pass show …"` (mbsync) and `passwordeval "pass show …"` (msmtp).

### Rendering attachments

Five kinds of message part are shown as text. `~/.config/aerc/aerc.conf` sends each to one
script, `kdos-part`; plain text, delivery reports and attached messages go through aerc's own
`colorize`. A type not listed gets aerc's *No filter configured* card, with its `:open`, `:save`
and `:pipe` hints. The `[filters]` section replaces aerc's whole default set rather than adding to
it, which is why the plain-text rows are written out.

| Part | Shown as |
|---|---|
| HTML | `w3m -dump`, which lays tables out on the grid, so marketing mail is readable |
| An invitation (`text/calendar`) | aerc's calendar filter, run through `gawk`. It only reads: a filter runs every time a message scrolls past, and one that imported invitations would accept every meeting you looked at |
| PDF | `mutool draw -F txt` |
| An image | `chafa`, as coloured symbols, the same in every terminal. With no filter for images, aerc would instead draw a JPEG or PNG through the terminal's graphics protocol, which `kdos-term` supports |
| A `.docx` | `docx2txt` |

`kdos-part` saves the part to a temporary file first, because `mutool` opens a document by path and
cannot read a pipe.

HTML is rendered where it cannot reach the network, so a tracking image has nowhere to report to.
`kdos-part` first tries `unshare --map-root-user --net`, which runs `w3m` with no network at all;
where the system refuses a new user namespace, it runs `w3m` behind an unreachable proxy instead.
It tests whether the namespace works rather than whether the `unshare` program exists — aerc's own
HTML filter checks only the program, and where namespaces are refused it shows
`unshare: Operation not permitted` in place of the message.

### OAuth

XOAUTH2 sign-in works in the three programs that talk to a server: `aerc` speaks it itself, `msmtp`
has it built in, and `mbsync` reaches it through `cyrus-sasl` and its `libxoauth2.so` plugin, which
the image carries. `pizauth` obtains and refreshes the token. The plugin takes whatever SASL returns as the password
and sends it as the token, so `PassCmd` is the only configuration `mbsync` needs:

```text
IMAPAccount work
AuthMechs XOAUTH2
PassCmd   "pizauth show work"
```

OAUTHBEARER is not available to `mbsync`: the plugin implements XOAUTH2 only. A provider that
offers only OAUTHBEARER can be read in `aerc` and sent to through `msmtp`, but has no local Maildir
— and with it no `notmuch` index and no offline search.

## Passwords and one-time codes

`pass` is the password store: one file per entry in a git repository, each encrypted with `gpg`.
There is no database and no format to migrate; even without `pass`, `gpg -d` reads every entry.

For a site that asks for a six-digit code, save the `otpauth://` link its QR code contains, then
ask for a code when you need one:

```sh
pass otp insert site/example        # paste the otpauth:// link
pass otp site/example               # the code for right now
pass otp -c site/example            # and copy it to the clipboard
```

Nothing needs enabling: `pass` loads its installed extensions automatically. The code comes from
`oathtool`, which also works on its own: `oathtool --totp -b <secret>`.

There is no one-time code for logging in to this machine. `oath-toolkit`'s PAM module is not built:
a wrong line in the login stack locks everyone out, including whoever is trying to fix it. These
codes are for other people's websites.

## Calendar and contacts

Calendars and contacts are plain files, like the mail. `~/.local/share/calendars` holds calendars
and `~/.local/share/contacts` holds address books, each as a *vdir*: one directory per collection,
one `.ics` or `.vcf` file per item. That can be searched with `grep`, compared with `diff` and backed
up by copying. A file holding two events is not a valid vdir entry; use `khal import` to bring one
in.

| Program | Does |
|---|---|
| `khal` | Prints what is on: `khal list today 7d`, `khal import invite.ics` |
| `ikhal` | The same calendar, full screen, to move around in. This is the *Calendar* menu entry |
| `khard` | The address book: `khard list`, `khard show`, `khard new` |
| `vdirsyncer` | Keeps a server's collection and the local vdir the same, as `mbsync` does for mail. Unconfigured, `vdirsyncer sync` does nothing and exits 0 |

`khal` and `khard` refuse to start with no store configured, so the shipped configuration names
empty ones, and the image creates both directories in each new home: `khal` reads every directory
under `~/.local/share/calendars/`, and `khard` reads the one address book
`~/.local/share/contacts/personal`. To add a calendar, make a directory under
`~/.local/share/calendars` and put `.ics` files in it; `khal` discovers it with no registration.

The panel's calendar reads the same store: days with something on are marked, and today's events
are listed under the month. It asks `khal` when it opens and when you change month, and runs
nothing between those times.

**Syncing with a server** is `vdirsyncer`, configured in `~/.config/vdirsyncer/config`. A *pair* is
two storages plus the rule for settling conflicts. There is no safe default: `a wins` discards the
server's edit and `b wins` discards yours, so the shipped example makes you choose. Run
`vdirsyncer discover` once, then `vdirsyncer sync` whenever you like. Keep the password out of the
file with `password.fetch = ["command", "pass", "show", "…"]`.

`khard list` exits 1 on an empty address book and prints `Found no contacts`. That is its answer
for "nothing matched", not a failure, so a script using it must treat that exit as an empty result.

## Finding a file by name

`plocate` searches an index instead of the disk, so a search by name across your whole home reads
one index file instead of walking the tree, as `fd` does.

The index is per user. A timer in `~/.config/kdos/timers.d/` rebuilds it nightly at 03:05, covering
only `$HOME`, and writes it to `~/.cache/kdos/plocate.db`, so it can only contain paths you could
already list. `LOCATE_PATH`, set in `/etc/profile.d/40-plocate.sh`, points `plocate` at it, and
`locate` is the same program under its usual name. Run `kdos-updatedb` to rebuild it now.

`updatedb` is plocate's, in `/usr/sbin`. findutils' own `locate` and `updatedb` are not installed,
because they read and write a different index format.

Upstream plocate is a setgid program reading one shared index for the whole machine; KDOS ships
neither the setgid bit nor the shared index. The reasoning is in
[the security model](../03-architecture/security-model.md#no-setgid-binaries-and-a-per-user-locate-index).

## Input methods

`fcitx5` is the input method engine, for typing languages that need more than a keyboard layout:
Chinese (pinyin, shuangpin and the table methods), Japanese and Korean. The session starts it by
name if it is installed. It talks to `kdos-comp` through `input-method-v2`, and is configured
through text files under `~/.config/fcitx5/` — its graphical configuration tool is Qt and is not
shipped. `Ctrl+Space` switches method; that is fcitx5's own binding, and the compositor does not
claim it.

The candidate window is KDOS's. `kdos-ime` starts just before fcitx5 and owns `org.kde.impanel`,
so the text being composed and the list of candidates are drawn like every other KDOS surface.
fcitx5 prefers that panel over its own as soon as it appears; without `kdos-ime`, fcitx5 draws its
own window.

Cloud pinyin is compiled out: it would send what you type to a remote service.

## See also

- [Configuration](../06-reference/configuration.md) — every file and key named here
- [The daemons](../04-programs/daemons.md) — what each root service owns
- [Boot and init](../03-architecture/boot-and-init.md) — the boot path and the service convention
- [The kdos command](../04-programs/kdos-command.md) — every diagnostic on this page
- [Packaging](../03-architecture/packaging.md) — ports, the binary host and updates
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port `kdos update` can install, by group
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — why services, packages and settings are arranged this way
- [Security model](../03-architecture/security-model.md) — what `wheel` means and what is not protected
- [Installation](installation.md) — the services, accounts and swap the installer sets up
- [Known gaps](../06-reference/known-gaps.md) — what does not exist

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 10.* Previous: [9. Theming](theming.md) · [Contents](../README.md) · Next: [11. Accessibility](accessibility.md)
