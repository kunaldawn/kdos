# Administration

This chapter is for the person who looks after a KDOS machine once it is installed or booted from
the stick. The first half is the machine as a whole: services and periodic jobs, the login screen,
users and groups, storage, networking and the firewall, hardware and device access, printing,
media and time, updates, diagnosis and logs, copying and rebuilding the medium, and tuning builds
for this CPU. The second half is what each user sets up in their own home: backups, mail,
passwords and one-time codes, calendars and contacts, finding files by name, the language, and
input methods. Most tasks have a page in System Settings, and each is also given as the command or
the file that does it, with the reason the system is arranged that way where the reason affects
what you do.

If you are new to KDOS, read [Getting started](getting-started.md) first; this chapter assumes you
can open a terminal and use `sudo`. The mechanisms behind it are in
[Boot and init](../03-architecture/boot-and-init.md), [The session](../03-architecture/session.md)
and [The daemons](../04-programs/daemons.md). Two facts shape everything below:

- **There is no systemd init and no service manager of that kind.** PID 1 is sysvinit; services are
  shell scripts in `/etc/init.d`, supervised by `ksvc`; each user's own services (the session bus,
  PipeWire, the user's timers) are started at their first login by `turnstile` and supervised by the
  same `ksvc`. elogind tracks who is logged in, so polkit and Plasma know who is at the machine, and
  KDOS answers the few systemd interfaces KDE calls (the clock, the hostname, the locale, system
  updates) with daemons of its own. Every setting is a plain file, whether System Settings writes it
  or you do. [Configuration](../06-reference/configuration.md) lists every one of those files and
  keys, and [How KDOS differs](../01-philosophy/how-kdos-differs.md) compares this with how other
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

A supervised service is `[ OK ]` only while its daemon runs. A daemon that keeps exiting is not:
the supervisor stays up and restarts it every five seconds, and `service status` reports
`[FAIL] <name>  (exited with code N; restarting)` between the starts, or `(crash loop: …)` while a
daemon that has failed twice in a row is running again and has not yet lasted a minute. A daemon
whose supervisor was killed outright is `[FAIL] <name>  (supervisor gone; daemon pid N still
running)`; `service start` will not start a second beside it, and `service stop` ends it. The
daemon's own last words are in `/run/kdos-svc.<name>.log`. The exact lines are in
[ksvc and service](../04-programs/kdos-command.md#ksvc-and-service).

`<name>` is the part of the file name between the number and `.sh`: `sshd` for `70_sshd.sh`. For
`status`, `start`, `stop` and `restart`, an exact name is tried first and then any service whose
name contains what you typed, so `service status ssh` finds `sshd`. `enable` and `disable` check
the same way that the service exists, but record the name exactly as you typed it, so type the full
name from `service list`: `service disable ssh` writes a marker that `rcS` never looks for.

Enabling and disabling is a marker file, not an edit: `service disable bluetooth` creates the empty
file `/etc/service.disabled/bluetooth`, and `service enable bluetooth` deletes it. Nothing in
`/etc/init.d` is rewritten. The live image ships one marker, `sshd`, so the SSH server is the one
installed service that does not start until you enable it; opening `ssh` in the firewall (below)
enables it. The installer writes markers for the services you turn off, and turns off CUPS and sshd
by default (see [Installation](installation.md)).

**Your own services.** `service --user` manages the services turnstile starts for you at your first
login and stops at your last: `dbus` (your session bus, which Plasma and every box use), `pipewire`,
`wireplumber`, `pipewire-pulse`, `mpd`, `timers` (your periodic jobs) and `boxes`. They live in
`/etc/kdos/user-services`, and their output is in `$XDG_RUNTIME_DIR/ksvc/kdos-svc.<name>.log`.

```sh
service --user list
service --user restart pipewire
```

A user who lingers (`loginctl enable-linger <user>`) keeps these services, and the boxes they run,
up with no login at all; without that, they stop when the user's last login ends.

During boot, each script's output goes to `/run/kdos-init.<name>.log` and is then copied to the
console, so the first thing to read about a service that failed at boot is that file. It lives in
`/run`, a tmpfs, and is gone after the next reboot.

The scripts that ship, in boot order. Scripts marked *(package)* come with the package named and
are absent if that package is removed. The login screen is not among them: `/etc/inittab` starts it
on `tty1` once `rcS` has run every script (see [The login screen](#the-login-screen)).

| Script | What it starts |
|---|---|
| `01_udev` | Device management, and the coldplug that loads drivers for the hardware already present. It also creates device nodes such as `/dev/uinput`, `/dev/uhid`, `/dev/snd/seq` and `/dev/vhost-net`, whose driver loads only when a program first opens the node, so they exist before anything asks for them |
| `02_modules` | Modules listed in `/etc/modules-load.d` |
| `03_lvm` | LVM volume groups, activated once at boot, then any `fstab` entry on a logical volume checked and mounted, and any swap entry on one turned on. On a disk boot the initramfs has already activated the groups it saw; this pass catches the rest — every group on a live boot, and any on a disk that appeared after the initramfs ran. A group plugged in later is not activated automatically; run `sudo vgchange -aay`. See [Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups) |
| `04_live-root` | On the live medium only, records `x-gvfs-hide` for the overlay root in `/run/mount/utab`, so Dolphin, the file dialogs and the Device Notifier do not list the RAM root as a device. See [The live medium and persistence](../03-architecture/boot-and-init.md#the-live-medium-and-persistence) |
| `05_hostname` | The hostname |
| `06_utmps` | Login records: `utmpd` and `wtmpd`, the two daemons through which `login`, `who`, `last` and init read and write the records of who is logged in and since when, because musl's own utmp calls do nothing. It waits up to two seconds for their sockets, so init's boot record is not lost — see [Login records](../03-architecture/boot-and-init.md#login-records) |
| `10_sysctl` | Kernel parameters from `/etc/sysctl.conf`: unprivileged `ping`; the name of the fatal signal in the kernel log when a program crashes; the `fs.protected_*` link protections, which stop a user redirecting a root job's write in `/tmp` onto another file; per-task delay accounting, which `iotop`'s IO% and `htop`'s delay columns read, at a small cost on every wait the kernel accounts; no ICMP redirect accepted or sent; the loose reverse-path filter; and kernel addresses hidden from every account but root |
| `12_zram` | Compressed swap in RAM — see [Storage](#storage) |
| `15_cgroups` | Enables the `cpu`, `memory` and `pids` controllers at the root of the cgroup tree, so each login's delegated cgroup can offer them, which is what lets rootless podman apply a *box*'s memory, CPU and process limits (a box is the rootless container one application runs in; see [the glossary](../06-reference/glossary.md)) |
| `18_timers` | Periodic jobs: one supervised `snooze` per line of `/etc/kdos/timers.d` — see [Periodic jobs](#periodic-jobs) |
| `20_dmesg`, `22_syslog` | Kernel and system logging. On a boot with `quiet`, `20_dmesg` lowers the console log level to 3, so only critical, alert and emergency kernel messages reach the screen; once `syslogd` is running it turns the kernel's console logging down further, to emergencies only, and restores it when it exits. On a boot without `quiet` (the verbose and single-user entries) the level stays the one `loglevel=` set: `20_dmesg` leaves it, and `22_syslog` runs `syslogd -l`, which keeps the kernel's console logging on. `22_syslog` runs `syslogd`, whose `/etc/syslog.conf` sets `secure_mode 2`, so it opens no network socket: it neither accepts messages from other hosts nor forwards to one. `secure_mode 1` allows forwarding to an `@host` action without listening. See [Logs](#logs) |
| `25_nftables` | The firewall, loaded before the network comes up — see [The firewall](#the-firewall) |
| `30_network` | `dhcpcd`, the fallback DHCP client. It stays down when NetworkManager is installed and not disabled, because NetworkManager's own DHCP client does not defer to it: two clients on one link means two leases, two default routes and two writers of `/etc/resolv.conf` |
| `31_babeld` *(babeld)* | Babel mesh routing, skipped until a non-empty `/etc/babeld.conf` exists. Open the `babel` firewall name for the neighbours |
| `35_chrony` | Time synchronisation |
| `40_dbus` | The system message bus. On its first start it generates `/var/lib/dbus/machine-id` and links `/etc/machine-id` to it |
| `40_elogind` | elogind, the login and seat manager: every login registers with it, KWin takes the screen and the input devices through it, polkit asks it who is at the machine, and Plasma's Shut Down, Restart and Sleep are its calls. It supervises nothing; see [Boot and init](../03-architecture/boot-and-init.md) |
| `41_polkitd` | polkit, the privilege broker. It starts before NetworkManager, which asks it on its first privileged call |
| `42_modemmanager` | ModemManager: mobile broadband modems, for NetworkManager and `mmcli`. Started at boot rather than on demand for the same reason as polkit |
| `42_networkmanager` | NetworkManager |
| `43_settingsd` | `kdos-settingsd`: the machine's name, clock, time zone, locale and keyboard layouts, for System Settings' Date & Time and Region pages, the first-start wizard and the login screen, which takes its keyboard layout from it |
| `43_boltd` *(bolt)* | Thunderbolt. Only when `/sys/bus/thunderbolt/devices` is non-empty, it calls `org.freedesktop.bolt` so the bus starts `boltd`. A device plugged in later is handled by the `bolt` package's udev rule |
| `44_accounts` | AccountsService: the accounts as System Settings' Users page, the first-start wizard and the login screen see them, with each account's picture |
| `44_switcheroo-control` *(switcheroo-control)* | `net.hadess.SwitcherooControl` on the system bus, which tells a launcher which GPU is discrete and what environment starts a program on it; KIO's "run on the discrete GPU" and `switcherooctl launch` read it. Skipped when the machine has fewer than two GPUs |
| `45_avahi` | mDNS: finding printers and other machines on the local network by name |
| `46_hostapd` | A wireless access point, only when `/etc/hostapd/hostapd.conf` exists — see [An access point with hostapd](#an-access-point-with-hostapd) |
| `46_sysupdated` | `kdos-sysupdated`: the system update Discover shows, the slots System Versions lists, and the confirmation of a booted update once a session has run; see [Keeping it current](#keeping-it-current) |
| `47_pcscd` | Smart cards and security keys: the PC/SC daemon, with readers arriving through udev |
| `48_iio-sensor-proxy` | The accelerometer and light sensor, for screen rotation and automatic brightness. Skipped on a machine with no sensor its rules recognise |
| `50_alsa` | Saves and restores the sound card's mixer levels |
| `51_mdmonitor` | `mdadm --monitor --scan --syslog`: a failed member, a degraded array or a missing spare is written to the system log. Skipped when `/proc/mdstat` lists no array; a `MAILADDR` or `PROGRAM` line in `/etc/mdadm.conf` adds a route of your own |
| `51_lsmd` *(libstoragemgmt)* | The libstoragemgmt plugin daemon, which udisks2's LSM module and `lsmcli` reach over `/run/lsm/ipc`; plugins that do not need root run as the `libstoragemgmt` account |
| `52_smartd` | Disk health: `smartd` polls every disk that answers SMART every half hour and logs a failing health check, bad sectors, drive errors and failed self-tests. Where no disk answers SMART — any virtual machine — it logs why and stops |
| `53_xfs_healer` | One `xfs_healer` per XFS filesystem mounted at boot. It logs the metadata damage the kernel reports and, where the filesystem's `autofsck` property asks for it, has it repaired online. A filesystem is skipped when the kernel has no XFS health monitor or its `autofsck` property is `none`, and an XFS stick mounted after boot gets no healer |
| `54_thermald` | Intel thermal management |
| `55_powerd` | `kdos-powerd`: the firewall's named services, and the accent of the boot menu, the splash, the consoles and the login screen, for the `kdos` command and System Settings |
| `55_tlp` | Laptop power management: `tlp init start` at boot and `tlp init stop` at shutdown, which apply the startup and shutdown radio settings in `/etc/tlp.d`. elogind runs TLP's sleep hook around a suspend |
| `56_energyd` | Per-application energy use |
| `56_tlppd` | `tlp-pd`: the power profiles (power saver, balanced, performance) Plasma's battery applet switches, each one running TLP's tuning for it |
| `56_nut` *(nut)* | The UPS tools: the drivers, `upsd` and `upsmon`, or `upsmon` alone as a network client. Skipped unless `MODE` in `/etc/nut/nut.conf` is `standalone`, `netserver` or `netclient`. Open the `nut` firewall name to serve UPS status to other machines |
| `57_oomd` | Memory-pressure protection: ends the heaviest application before the machine stalls, sparing the desktop itself |
| `59_packd` | `kdos-packd`, which mounts application *packs*: signed EROFS images holding an application, runtime, base or dataset, which boxes are built over. See [Packs and boxes](../03-architecture/packs-and-boxes.md) |
| `60_bluetooth` | Bluetooth |
| `61_spacenavd` *(spacenavd)* | The 3D mouse daemon. It finds 3Dconnexion devices as they are plugged in and serves them on `/run/spnav.sock`, the socket every libspnav client (FreeCAD, OpenSCAD, KiCad, Blender) connects to. `/etc/spnavrc`, absent by default, sets sensitivity, axis mapping and dead zones; `/usr/share/doc/spacenavd/example-spnavrc` is the template. It serves no X11 client, so a program that speaks only the proprietary 3Dconnexion X protocol sees no device |
| `62_plasma-setup` | On a machine installed for someone else, arms Plasma's first-start wizard, which makes the first account; does nothing once `/etc/plasma-setup-done` exists. See [Preparing a machine for someone else](installation.md#preparing-a-machine-for-someone-else) |
| `62_virtlogd` *(libvirt)* | libvirt's log daemon, which keeps each guest's console log |
| `63_gssd` *(nfs-utils)* | `rpc.gssd`, the Kerberos half of an NFS client, which a `sec=krb5` mount needs. It mounts `rpc_pipefs` at `/var/lib/nfs/rpc_pipefs` first. Skipped until `/etc/krb5.keytab` exists |
| `63_libvirtd` *(libvirt)* | libvirt's management daemon for `qemu:///system`. Its polkit rule lets members of the `libvirt` group manage guests. Its `default` NAT network is defined and not started: `virbr0` and its `dnsmasq` come up when virt-manager or `virsh net-start default` starts it |
| `63_live-installer` | On the live medium only, puts the **Install KDOS** entry and icon in place, and the polkit rule that lets the live session start the installer without a password |
| `64_lircd` *(lirc)* | The infrared remote daemon, skipped until a receiver is present (a `/dev/lirc*` node or an rc-core device). Its socket `/run/lirc/lircd` is what Kodi, VLC and other LIRC clients read |
| `65_turnstiled` | turnstile: each user's runtime directory, `/run/user/<uid>`, and their own services, from their first login to their last. The login screen cannot start without it |
| `65_brltty` *(brltty)* | The console screen reader, skipped until `/etc/brltty.conf` exists. Its speech goes straight to the sound card, so it cannot speak while a desktop session's PipeWire holds the card. See [Accessibility](accessibility.md) |
| `70_sshd` | The SSH server, disabled until you enable it (the image ships `/etc/service.disabled/sshd`). Opening `ssh` in the firewall enables and starts it, and closing `ssh` disables and stops it. Before the first start it generates every missing host key type with `ssh-keygen -A` |
| `71_snmpd` *(net-snmp)* | The SNMP agent, as root, skipped until a non-empty `/etc/snmp/snmpd.conf` exists; it logs to syslog. Open the `snmp` firewall name for other machines |
| `72_nfsd` *(nfs-utils)* | The NFSv4 server, skipped until `/etc/exports` names a share. It loads `nfsd`, supervises `nfsdcld` (the client records a restarted server needs) and `nfsv4.exportd`, runs `exportfs -r`, then starts the kernel threads with `rpc.nfsd -N 3 -V 4`. NFSv3 is off, so no portmapper, `rpc.mountd` or lock daemon runs. After editing `/etc/exports`, run `sudo exportfs -r`. Open the `nfs` firewall name for other machines |
| `73_mosquitto` *(mosquitto)* | The MQTT broker, as the `mosquitto` account, skipped until a non-empty `/etc/mosquitto/mosquitto.conf` exists; the package ships only `/etc/mosquitto/mosquitto.conf.example`, the template. A configuration with no `listener` line makes the broker listen on loopback only; a `listener 1883` line, plus the `mqtt` firewall name, lets devices on the LAN reach it |
| `74_prosody` *(prosody)* | The XMPP chat server, as the `prosody` account, skipped until it has an account: `sudo prosodyctl adduser <user>@<host>` makes the first. Clients are refused until the host has a certificate; `sudo prosodyctl cert generate <host>` makes a self-signed one. Open the `xmpp` firewall name for other machines |
| `75_mumble-server` *(mumble)* | The Mumble voice server, as the `mumble-server` account, skipped until its database exists: running `mumble-server -supw <password>` as that account makes it. Open the `mumble` firewall name for other machines |
| `76_postgresql` *(postgresql)* | One shared PostgreSQL server, as the `postgres` account, skipped until a cluster exists: `sudo -u postgres initdb -D /var/lib/postgres/data` makes it. It listens on loopback and on a socket in `/tmp` |
| `77_radicale` *(radicale)* | Radicale, the CalDAV and CardDAV server, as the `radicale` account, skipped until `/etc/radicale/users` has an account line (`name:` followed by a hash from `openssl passwd -6`). Open the `caldav` firewall name for other machines |
| `78_maddy` *(maddy)* | The maddy mail server, as the `maddy` account, skipped until `/var/lib/maddy/credentials.db` holds an account (running `maddy creds create <address>` and `maddy imap-acct create <address>` as the `maddy` account, with `sudo -u maddy`, makes one). Open the `mail` firewall name for other machines |
| `79_ngircd` *(ngircd)* | The ngIRCd chat server, as the `ngircd` account, skipped until a non-empty `/etc/ngircd.conf` exists; `/usr/share/doc/ngircd/sample-ngircd.conf` is the template. Open the `irc` firewall name for other machines |
| `80_cups` | Printing |
| `81_cups-browsed` | Printers shared on the network, added to CUPS as they appear |
| `82_ipp-usb` | Driverless printing and scanning over USB: each IPP-over-USB device is served on localhost as it is plugged in |
| `83_samba` *(samba)* | `smbd`, skipped until some account can share: the `sambashare` group has a member, `/var/lib/samba/usershares` (which it makes at every boot) holds a usershare, or `/etc/samba/smb.conf` defines a share. An account just added to `sambashare` starts it at its next login. Open the `smb` firewall name for other machines |
| `84_minidlna` *(minidlna)* | MiniDLNA, the media server for televisions and players on the LAN, skipped until a non-empty `/etc/minidlna.conf` exists; the template is in `/usr/share/doc/minidlna`. Open the `dlna` firewall name for other machines |
| `85_crond` | cronie, for each user's own crontab (`crontab -e`, or System Settings' Task Scheduler). The jobs the system ships are not cron's; see [Periodic jobs](#periodic-jobs) |
| `85_gnuhealth` *(gnuhealth)* | The GNU Health server (`trytond`), as the `gnuhealth` account, skipped until `/etc/gnuhealth/trytond.conf` names a database `uri`. Open the `tryton` firewall name for other machines |
| `86_kiwix-serve` | The offline library on `127.0.0.1:8080`, skipped until `/var/lib/kiwix/library.xml` lists an archive — see [The local servers](../03-architecture/boot-and-init.md#the-local-servers) |
| `87_kolibri` | Kolibri, the offline curriculum, on `127.0.0.1:8081`, skipped until a channel is installed |
| `88_llama-server` | The local language-model server on `127.0.0.1:8082`, skipped until a `.gguf` model is under `/usr/share/llama.cpp/models` |
| `89_step-ca` *(step-ca)* | The step-ca certificate authority, as root, from `/etc/step-ca`. Skipped until `/etc/step-ca/config/ca.json` (from `sudo STEPPATH=/etc/step-ca step-cli ca init`) and `/etc/step-ca/password.txt` (the CA password, mode 600) both exist. Open the `https` firewall name for other machines when the CA listens on 443 |

### Shutdown

Shutdown runs the same set backwards. `poweroff`, `reboot` and `shutdown` ask init for runlevel 0
or 6, and `/etc/init.d/rcK` is the first entry of those runlevels in `/etc/inittab`; it runs each
enabled script with `stop`, in reverse order, before the remaining processes are ended, swap is
turned off and filesystems are unmounted, so a service can still save its state. A service that fails to
stop does not hold up the rest. The firewall is not stopped: it stays loaded until power-off, so
the machine is never on the network unfiltered. See
[Boot and init](../03-architecture/boot-and-init.md#shutdown).

### Services that skip themselves

A service that cannot do its job on this machine's hardware or kernel is skipped at boot, with a
`[SKIP]` line saying why, instead of being started and left to fail. The servers in the table
above that wait for a configuration file or an account skip the same way; these are the ones that
skip because of the machine:

| Script | Skipped when |
|---|---|
| `12_zram` | The `zram` module will not load |
| `43_boltd` | `/sys/bus/thunderbolt/devices` is empty: no Thunderbolt controller |
| `44_switcheroo-control` | Fewer than two GPUs under `/sys/class/drm` |
| `51_mdmonitor` | `/proc/mdstat` lists no md array |
| `53_xfs_healer` | Per filesystem, when `xfs_healer --supported` refuses it: no XFS health monitor in the kernel, or `autofsck` is `none`. When no filesystem is left, the line is `no XFS filesystem to watch` |
| `54_thermald` | The processor is not Intel |
| `48_iio-sensor-proxy` | No sensor its udev rule recognises |
| `56_energyd` | No readable CPU energy counter (`/sys/class/powercap/*/energy_uj`) |
| `57_oomd` | `/proc/pressure/memory` is not writable (pressure stall information is off) |
| `59_packd` | The kernel cannot mount EROFS, the pack filesystem |
| `64_lircd` | No infrared receiver: no `/dev/lirc*` node and no rc-core device under `/sys/class/rc` |

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

Several scripts leave nothing for the supervisor to watch: `01_udev`, `02_modules`, `03_lvm`,
`04_live-root`, `05_hostname`, `10_sysctl`, `12_zram`, `15_cgroups`, `20_dmesg`, `25_nftables`, `43_boltd`,
`50_alsa`, `55_tlp`, `62_plasma-setup` and `63_live-installer`. Most apply a setting and exit;
`25_nftables` and `12_zram` hand their result to the kernel, which keeps it. For these,
`service status` reports what the script applied rather than a running process: `sysctl: applied`,
the zram device and its nominal size, whether a firewall ruleset is loaded, the logical volumes
(`lvs`), or TLP's own summary (`tlp-stat -s`). A script with nothing to report prints nothing, and
`service list` shows `—` for it. Two report a
process after all:
`service status boltd` says whether the bus has `boltd` running, and `service status udev` says
whether `udevd` is running, because `udevd` runs on its own once the script has started it.

## Periodic jobs

Two schedulers run side by side, each with its own job. The jobs the system ships run from
`snooze`, one line per job in a file ending `.timer` under `/etc/kdos/timers.d/`; your own jobs run
from `~/.config/kdos/timers.d/` in the same format (see [Your own jobs](#your-own-jobs)) or from a
crontab, which cronie runs (`crontab -e`, or System Settings' Task Scheduler). cronie is built to
read no system crontab, so a job is never in both. A timer line looks like this:

```text
NAME  TIMESPEC...  --  COMMAND...
```

Blank lines and lines starting with `#` are ignored. For example, the shipped update check:

```text
update-check  -H 4 -M 17 -s 8h  --  kdos update check --out /var/lib/kdos/update.json
```

`snooze` is a small program that sleeps until the clock matches its options, runs a command once
and exits. At boot `18_timers` starts one `snooze` per line under the supervisor, as the service
`kdos-timer-NAME`; when `snooze` exits after a run, the supervisor starts it again and it sleeps
until the next slot. That gives one process per job and no shared schedule file to corrupt.
`service status timers` shows every job and exits non-zero when any of them is down, including one
skipped at boot because its command is not installed; `ksvc check kdos-timer-update-check` shows one.

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
| `10-update-check.timer` | `kdos update check --out /var/lib/kdos/update.json` | 04:17, slack 8 h | Compares the installed packages with the ports tree and the release information already on the machine, for Discover and the update tray. It downloads nothing; not at midnight, because it reads the whole ports tree |
| `20-fstrim.timer` | `fstrim -a` | 12:47, slack 20 h | See below |

No shipped job reaches the network. A newer release is fetched only when someone asks: Discover's
**Check for updates**, or `kdos update fetch` at a prompt. The firmware metadata comes with each
release (see [Firmware updates](#thunderbolt-phones-smart-cards-and-firmware-updates)).

`fstrim` tells an SSD which blocks are free. The installer mounts ext4 and XFS without the
`discard` option, so without this job the drive's write speed and wear degrade as it fills; btrfs
and f2fs discard on their own, and a device that cannot discard is skipped. The slot is at midday
because `fstrim` has no timefile to pass to `-t`, so a slot the machine was off for is missed — and
a desktop switched off every night would never reach an overnight one.

### Your own jobs

Put your own jobs in `~/.config/kdos/timers.d/*.timer`, in the same format. They are run by the
`timers` service of your own services (see [Services](#services)), from your first login of any
kind, the desktop or a console, to your last, and with no login at all if you linger
(`loginctl enable-linger`). Each line runs in a loop that starts `snooze` again after every run, so a
job fires at every occurrence of its slot. A file added or edited while you are logged in is read at
the next start of the service, or now with `service --user restart timers`. The `README` beside your
table says the same, with an example.

Differences from the system table:

- A line that does not parse, whose command is not installed, or whose timespec `snooze -n` rejects
  is skipped. If a job never fires, check the line with `snooze -n <timespec>`, which prints the
  next five times it would run.
- A job's error output is in `$XDG_RUNTIME_DIR/ksvc/kdos-svc.timers.log` and in the system log.
- A job that must run whether or not anyone is logged in belongs in the system table.

Give each job a minute of its own. A laptop that wakes after its slots have passed runs every job
whose slack covers the gap at the same moment, and jobs that start together compete for one disk.
That is why the backup job below sits at 03:40 and not beside the 04:17 update check.

Two jobs ship in the skeleton copied into each new home, and one in `/etc/kdos/user-services`
for everyone:

| Job | Runs | State |
|---|---|---|
| `20-updatedb.timer` | `kdos-updatedb` at 03:05 | Enabled — see [Finding a file by name](#finding-a-file-by-name) |
| `10-backup.timer` | `kdos backup run` at 03:40 | Commented out: a first backup of a home directory can take hours, and should be something you chose — see [Backups](#backups) |
| `40-box-gc.timer` | `kdos-box gc` every ten minutes | Stops boxes past their `autostop` time — see [Idle boxes](applications.md#idle-boxes) |

## The login screen

`/etc/inittab` gives `tty1` to `kdos-dm`, which starts SDDM, the login screen, once the boot
scripts have finished. The login screen runs on KWin with no X server, in the KDOS look and the
machine's accent, and lists the accounts with a user ID from 1000 to 60000. `tty2` is a plain text
login prompt: the recovery console to use when the login screen or the desktop does not come up.
When SDDM cannot start, or exits three times within a minute, `kdos-dm` puts a text password
prompt on `tty1` instead, never an automatic login.

System Settings → Login Screen (SDDM) sets the theme, the automatic login and the session, as an
administrator. Its settings, and the installer's, are in `/etc/sddm.conf.d/kde_settings.conf`;
KDOS's own SDDM settings are in `/etc/sddm.conf.d/00-kdos.conf`, which that file outranks:

| File | Holds |
|---|---|
| `/etc/sddm.conf.d/00-kdos.conf` | The Wayland greeter on KWin, the KDOS session wrapper, no X sessions, the account range and the `kdos` theme |
| `/etc/sddm.conf.d/kde_settings.conf` | `[Autologin]`: the account logged in without a password, and the session. The live medium ships the `kdos` account there; an installed machine has it only on an encrypted root that asked for it (see [6. Accounts](installation.md#6-accounts)) |

With automatic login, the account's password store, KWallet, has no login password to open with
and asks for its own the first time a program stores a secret. An automatic login can start
behind the lock screen, which then asks for the password: the installer sets this with *Start the
session locked*, as `/etc/kdos/autologin-xdg/kscreenlockerrc`, read by automatic logins alone.

A session started from the login screen runs your login shell's profile first, so `/etc/profile`,
`/etc/profile.d` and your own `~/.bash_profile` reach Plasma; a command there that waits holds the
desktop back behind the login screen. The session's own log is
`~/.local/share/sddm/wayland-session.log`, and the login screen's is `/var/log/sddm.log`.

## Users and groups

One account ships: `kdos` (UID 1000), a member of `wheel`, `users` and the hardware groups below.
The installer asks for its password; on the live image it is `kdos`.

**`wheel`** is the administrator's group. `sudo` grants on it, through the one line
`%wheel ALL=(ALL) ALL` in `/etc/sudoers.d/00-sudo`, which asks for your own password; the same
file keeps `EDITOR`, `VISUAL` and `SUDO_EDITOR` through `sudo`, so `sudoedit` and `visudo` open the
editor your shell names (without one, `nvim`, else `nano`). polkit counts `wheel` as the
administrators, so System Settings, Discover and Partition Manager ask a `wheel` member for their
own password where an administrator is needed, through Plasma's authentication dialog. A few things
a `wheel` member at the machine does as a matter of course need no password at all: editing system
network connections, mounting internal disks, managing the system's virtual machines. The root
daemons check `wheel` too, before accepting a configuration change.

What a desktop needs, every account has without a group: suspend, power-off and restart, network
and Bluetooth connections, removable media, the brightness, the power profile and the time zone are
granted to whoever is at the machine, in the active local session, and to nobody logged in from
elsewhere. The time zone is granted through `kdos timezone`; the Date & Time page asks for an
administrator whatever it changes (see [Media, colour and time](#media-colour-and-time)). An
account outside `wheel` can therefore run a desktop and power it down, but cannot administer the
machine. The rules are in
[The security model](../03-architecture/security-model.md#polkit-and-the-authentication-agent).

The hardware groups of the `kdos` account, and what removing each would silently stop:

| Group | Makes usable without root |
|---|---|
| `dialout` | Serial adapters, debug probes, SDRs, instruments, security keys, cameras, scanners, phones — see [the udev rules](#the-device-groups-and-the-udev-rules) |
| `audio` | Sound devices |
| `video` | The GPU's display nodes, cameras, and a monitor's DDC/CI line |
| `render` | GPU rendering and compute |
| `input` | Input devices and game controllers' raw HID |
| `kvm` | `/dev/udmabuf`, through which QEMU's virtio-gpu shares a guest's display buffers with the host without copying them. `/dev/kvm`, `/dev/vhost-net` and `/dev/vhost-vsock` are open to every account, so running a guest needs no group; see [Virtual machines](../../kfs/16-wiki/virtual-machines.md) |
| `cdrom` | Optical drives |
| `lpadmin` | Adding and managing printers in CUPS |
| `tty` | Writing to other terminals' devices, as `write` and `wall` do |

**Adding a user.** System Settings → Users adds, changes and removes accounts, and sets each one's
picture, as an administrator. It makes an account with a group of its own name, in `audio`, and in
`wheel` when you choose an administrator. Every account any tool makes joins `audio`: the line
`GROUPS=audio` in `/etc/default/useradd` is `useradd`'s default, and `audio` carries PipeWire's
real-time limits, without which sound drops out under load. `useradd -G` replaces that default
instead of adding to it, so a list given by hand names `audio` again. To give the new account the
hardware groups as well, or to do the same from a prompt, use the `shadow` tools:

```sh
sudo useradd -m -G users,audio,video,render,input -s /bin/bash alice
sudo passwd alice
```

Add `wheel` for an administrator, and `dialout`, `cdrom`, `kvm`, `lpadmin` and `tty` from the table
as needed. Four things to check afterwards:

- **Boxed applications** run rootless podman, which needs a range in `/etc/subuid` and
  `/etc/subgid` for the account; `grep alice /etc/subuid` shows whether `useradd` added one. The
  shipped files give `kdos` the range starting at 100000 and `root` the one starting at 200000,
  65536 IDs each, so a range added by hand for another account must not overlap those.
- **`/run/user/<uid>`** needs nothing: turnstile makes it at the account's first login and removes
  it at the last.
- **The home is private** with nothing to do: `HOME_MODE 0700` in `/etc/login.defs` makes every
  home `useradd -m` creates mode `0700`, the mode the installer gives its own account, whether
  System Settings, Plasma Setup or a prompt made it. `ls -ld /home/alice` shows `drwx------`.
- **Empty directories** are not in the home `useradd -m` makes: it copies `/etc/skel`, which
  carries no empty directory, and the image build creates the empty directories in the `kdos` home
  alone. Mail, the calendar and address book each expect theirs to exist, so create them as the
  new account: `mkdir -p ~/Mail ~/.local/share/calendars/personal ~/.local/share/addressbooks/personal`.

No account keeps a mailbox: nothing on KDOS delivers local mail, so `/etc/login.defs` names no mail
spool, `/etc/default/useradd` sets `CREATE_MAIL_SPOOL=no`, and `useradd` and `userdel` neither
make nor look for one.

**Removing a user.** End the account's processes first, then remove it with its home directory:

```sh
sudo loginctl terminate-user alice
sudo pkill -u alice
sudo userdel -r alice
```

The account's own services, its `ssh-agent` among them, stop at its last logout, which `loginctl
terminate-user` brings about; anything else it left running, such as a program started with
`nohup`, keeps running until it is ended. `userdel` refuses an account with a
running process, printing `user alice is currently used by process <pid>` and exiting 8 with
nothing removed. `userdel -r` also takes the account's own group and its `/etc/subuid` and
`/etc/subgid` ranges. If `/etc/sddm.conf.d/kde_settings.conf` logs the account in automatically,
change that first, on the Login Screen page, or the machine boots to a login nobody can complete.

## Storage

**Filesystem checks at boot.** Before anything is mounted, `rcS` runs `fsck -A -R -T -a`: every
`/etc/fstab` filesystem with a non-zero pass number is checked and repaired where that is safe,
except the root, which the initramfs checked before mounting it. The report goes to
`/run/kdos-fsck.log`. A filesystem left with uncorrected errors is still mounted, and the boot
splash says so, rather than the machine stopping behind the splash; read the log and repair it by
hand from `tty2`. A filesystem on an LVM volume group that the initramfs did not activate is
checked and mounted later by `03_lvm`, which appends to the same log.

**Swap.** `rcS` runs `swapon -a` at boot, after `mount -a`, so any swap partition or swapfile in
`/etc/fstab` is used. The installer can create a swapfile, or a swap partition on its erase plan,
and writes the `fstab` line for either, naming a partition by its UUID; see
[kinstall](../04-programs/kinstall.md#the-pages).

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

**Removable media and disks.** Disks are reached through udisks2, which the desktop's Device
Notifier, Dolphin, Partition Manager, ISO Image Writer, GNOME Disks, K3b and the file dialogs all
use; `udisksctl` is the same from a prompt. `udisksd` has no service script: the system bus starts
it on the first request. A removable device is mounted, as you, under
`/run/media/<user>/<label>`, with `nosuid,nodev`, so a setuid program on someone else's stick cannot
make anyone root. Every request is decided by polkit. Mounting, unlocking, ejecting and powering
off a removable device are granted to the person at the machine, in the active local session,
with no password; an internal disk, formatting and raw device access are the administrator's, and
the rule in `/etc/polkit-1/rules.d/50-kdos.rules` grants them to a member of `wheel` at the machine
without a password. A request from anywhere else, such as an SSH login, gets polkit's default
answer, which asks for an administrator's password or refuses.

```sh
udisksctl status                          # the drives udisks sees
udisksctl mount -b /dev/sdb1              # mount one; prints where
udisksctl unlock -b /dev/sdb1             # unlock a LUKS stick; asks for its passphrase
udisksctl power-off -b /dev/sdb           # safe to pull out
```

A Windows or Samba share is reached as `smb://server/share` in Dolphin or a file dialog; the
network place lists the servers that answer on the local network.

**Sharing a folder.** Dolphin's **Properties → Share** shares a folder over SMB as a Samba
*usershare*, and you choose who may read or write it. `/etc/samba/smb.conf` ships with the
settings this needs. The first time, the tab takes you through three steps before the share:

1. **Make Me a Group Member.** An administrator's password adds you to the `sambashare` group,
   whose members alone may add shares. Log out and in again afterwards; the tab says to restart
   the system, but a new login is enough. That login starts the Samba server, with no prompt,
   through the session's `kded-kdos` module and `kdos-powerd`, and from then on every boot starts
   it (see
   [Starting Samba for a shared folder](../04-programs/daemons.md#starting-samba-for-a-shared-folder)).
2. **Set Samba password.** A second administrator's password creates your entry in Samba's own
   password database, with the password other machines sign in to your shares with.
3. **Fix Permissions.** Home folders are private, so the server cannot reach a folder inside
   yours; the tab offers to let others pass through your home folder (not list or read it) on the
   way to the shared one.

Then **OK** makes the share. The Samba server does not run on a machine where nobody is in
`sambashare` and nothing is shared, and the share cannot be made while the server is down: the
tab then reports that a name cannot be converted to a SID. The prompts are polkit's for the person
at the machine, so they fail from a session that is not the active one on its seat.
`sudo touch /etc/service.disabled/samba` keeps the server from starting either way. Other machines
reach a share only once the `smb` name is open in [the firewall](#opening-a-service).

**Kernel limits a program raises.** A setting a KDE program changes for you, such as Dolphin's or
the file indexer's offer to raise the limit on watched files, is written as a drop-in under
`/etc/sysctl.d`, and `10_sysctl` applies every drop-in at boot before `/etc/sysctl.conf`, so the
change survives a reboot. A key in `/etc/sysctl.conf` wins over the same key in a drop-in.

## Networking

NetworkManager manages the network, with `wpa_supplicant` for wireless, ModemManager for mobile
broadband and a local `dnsmasq` for DNS. Connecting to a network, switching the radio, a hotspot
and a VPN are granted to the person at the machine with no password; editing a connection for every
account on the machine is granted to a member of `wheel` at the machine. Setting the hostname,
setting machine-wide DNS and reloading NetworkManager's configuration need `sudo`.

| Tool | For |
|---|---|
| The network item in the tray | Networks, connecting and disconnecting, the hotspot, and the passphrase prompt |
| System Settings → Wi-Fi & Internet | Every connection: wired, wireless, VPN, mobile broadband, DSL, bridges and the rest, with their IP and DNS settings |
| `nmtui` | The full text interface |
| `nmcli` | Scripting |

**Connecting to wireless.** Click the network item in the tray, choose the network and type the
passphrase; it is kept in the connection profile, or in your KWallet when the connection is yours
alone.

Any later question comes from Plasma's network agent, which runs with the session. NetworkManager
never prompts by itself: when a saved secret is missing or refused, it asks the password agents
registered with it, and fails the connection silently if none answers. So a changed wireless key,
802.1X enterprise wireless and a VPN one-time code all arrive as a dialog from the agent, not from
the window you started in.

**DNS.** NetworkManager starts a caching `dnsmasq` on `127.0.0.1` and `::1` and feeds it the
servers of each connection, as set by `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf`. DNS is
split by domain: each connection's servers answer for the domains in its search list, and a name
in no connection's list goes to the servers of the connections holding the default route. A VPN
that carries only its own networks answers only for its own domains; one that takes the default
route answers every other name as well. A file of the same name in `/etc/NetworkManager/conf.d`
overrides the shipped one.
`/etc/resolv.conf` has one writer, `resolvconf` (openresolv), which NetworkManager, the fallback
`dhcpcd` and `wg-quick` all go through, so none of them overwrites another's entries. After each
change it runs `service restart nscd` when `service status nscd` reports it running; a
`dnsmasq`, `named` or `unbound` subscriber turned on in `/etc/resolvconf.conf` restarts its own
service the same way.

**VPNs.** OpenVPN works through NetworkManager with certificate and password authentication;
PKCS#11 hardware tokens are not supported by the OpenVPN build. WireGuard works two ways:

- `nmcli connection import type wireguard file wg0.conf` makes a NetworkManager profile, and its
  `DNS=` addresses go to the local `dnsmasq`. The names on the `DNS=` line become the tunnel's
  domains; a line with addresses only gives the profile the domain `~`, so the tunnel's servers
  answer every name. A `DNS=` line is dropped for an address family the `Address=` line has none
  of.
- `wg-quick up wg0` hands a `DNS=` line to `resolvconf`, which makes the tunnel's servers the only
  ones in `/etc/resolv.conf` until `wg-quick down wg0`.

**Web servers that ask for a Windows sign-in.** libsoup, the HTTP library under WebKitGTK,
GStreamer's network sources, GeoClue and gvfs, answers both challenges an Active Directory web
service sends. *Negotiate* is answered with the Kerberos ticket `kinit` left in your credential
cache. *NTLM* first tries single sign-on through Samba's `/usr/bin/ntlm_auth`, which is why every
libsoup consumer installs `samba`; that helper signs on only from `winbindd`'s cached credentials,
and Samba here is built without winbind, so the attempt fails and libsoup answers NTLM itself with
the password the application asks for. libsoup's TLS is glib-networking's GIO
module, loaded at run time, so a system without `glib-networking` fails every `https` request at
the handshake with "TLS/SSL support not available".

**Mobile broadband** is a NetworkManager connection like any other. ModemManager finds a built-in
WWAN card or a USB modem (MBIM, QMI, QRTR or AT). Connect it with
`nmcli connection add type gsm ifname '*' apn <apn>`, or with `nmtui`'s mobile broadband entry,
which offers carriers from `mobile-broadband-provider-info`. Plasma's network agent asks for a SIM
PIN.
`mmcli -m any` shows the modem as ModemManager sees it; members of `wheel` can unlock and enable a
modem and read its text messages there without `sudo`.

**Other connection types:**

| Connection | Command |
|---|---|
| PPPoE (DSL), carried by `pppd` | `nmcli connection add type pppoe ifname ppp0 pppoe.parent <ethernet> username <user> password <password>` |
| A phone's Bluetooth tethering | `nmcli connection add type bluetooth bt-type panu bluetooth.bdaddr <phone>`, after pairing the phone in System Settings → Bluetooth |
| A phone that offers only dial-up over Bluetooth | The same with `bt-type dun`, plus an APN |

### An access point with hostapd

The network applet's hotspot is the quick way to share a connection, and needs none of this. `hostapd` is
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
| DNS (UDP and TCP 53) and DHCP (UDP 67) from a `virbr*` bridge (with `libvirt`) | Accepted |
| From a `virbr*` bridge (with `libvirt`) | Forwarded |

Why the less obvious rows are there:

- **ICMPv6** is answered because dropping it does not make IPv6 safer — it breaks neighbour
  discovery and path MTU discovery.
- **`10.42.0.0/16`** is NetworkManager's hotspot. NetworkManager gives each shared connection a `/24`
  from that range and runs `dnsmasq` on it. A packet this firewall drops cannot be let through by
  NetworkManager's own rules, so without these rows a client joins the hotspot and never gets an
  address. Outside that range the ports stay closed.
- **`podman*`** covers rootful podman's bridge networks — `sudo podman run`, rootful distrobox and
  any network `podman network create` makes. These rows live in `/etc/nftables.d/40-podman.nft`.
  Without them a container gets an address, reaches nothing, and cannot resolve names, because
  `aardvark-dns` answers on the bridge's own address. Rootless boxes forward nothing and need
  neither.
- **`virbr*`** is the bridge libvirt makes for its guests' network. The `libvirt` package installs
  these rows as `/etc/nftables.d/40-libvirt.nft`, so a guest of `libvirtd` gets its address and
  names from libvirt's `dnsmasq` and reaches the network through this machine; without the
  package the rows are absent.
- **NetBIOS** is a reply, not a service. Name lookups on a Windows network are broadcasts that
  every machine answers from its own address, so the firewall cannot match them as replies to a
  connection. Without this rule the network place in Dolphin finds no Windows file servers.
  Nothing on the image listens on port 137. WS-Discovery answers (from UDP port 3702, from private
  and link-local addresses) are accepted for the same reason.

### Opening a service

Anything that should be reachable from another machine needs a rule — sshd, a shared printer and a
served offline library are all blocked by default. There are two ways, and they do not interfere
with each other.

**By name, with System Settings → Firewall** or the command `kdos-power firewall`:

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
| `vnc` | TCP 5900 (this desktop over VNC, with krfb) |
| `rdp` | TCP 3389 (this desktop over RDP, with krdp) |
| `xonotic` | UDP 26000 (hosting a Xonotic game) |

Turning a name on or off needs an administrator: a member of `wheel` is let through, and anyone else
is asked by polkit. The Firewall page holds its switches until **Apply**. The meaning of each name
is fixed inside `kdos-powerd`, not in the page you click, so no client can open a port that is not
on this list.

**Allow on this network?** When another machine knocks on a closed port that KDE Connect, krdp,
krfb or `kiwix-serve` would answer, the program behind it is running for you, and you have not been
asked about it on this network, a notification offers **Allow** or **Not on This Network**. The
answer is remembered per network and per service, in `~/.local/state/kdos/fwask`, and **Allow**
opens the name as the Firewall page would.
`kdos-powerd` rewrites `/etc/nftables.d/50-kdos-services.nft` completely on every change and
reloads the firewall, so do not put your own rules in that file: they disappear at the next
toggle.

### Adding your own rules

Anything else is an edit. `/etc/nftables.conf` carries commented examples in its `input` chain,
and its last line, `include "/etc/nftables.d/*.nft"`, reads every `.nft` file in that directory. A
file of your own there is never touched by the Firewall page. For example,
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
sudo service start nftables           # apply
```

`25_nftables` runs the same check before loading, so a file with a mistake is refused as a whole
and the rules already loaded stay in force. `sudo service stop nftables` removes the `inet filter`
table, which leaves the machine unfiltered until the next start.

## Hardware

### Firmware

`linux-firmware` is installed with upstream's own script so that the alias links drivers ask for
exist. It ships every blob an x86 kernel can request: only firmware for hardware this kernel cannot
drive is left out (see the [ports catalogue](../06-reference/ports-catalogue.md#boot)). Trimming by
guessing at your hardware would fail silently, so nothing an x86 driver names is removed.

`sof-firmware` is separate and is needed for audio on Intel Tiger Lake and newer. It carries both
the DSP firmware and the topology files; firmware without its topology loads and produces no sound.

`wireless-regdb`, the table of which radio channels and power levels each country allows, is built
from upstream's `db.txt` and ships with upstream's signature. The kernel checks that signature, and
rejects an unsigned or altered database silently, leaving the radio on the restrictive world
settings: no 5 GHz DFS channels and reduced transmit power. The build verifies the signature
against the database it produced and fails on a mismatch, so an edited `db.txt` never ships.

The country comes from your time zone. The installer, the Date & Time page and `kdos timezone` all
write the zone's country code to `/etc/modprobe.d/kdos-regdom.conf` as cfg80211's
`ieee80211_regdom` and set the running radio to it; a zone with no country, such as `UTC`, removes
the file and returns the radio to the world domain `00` at once. `kdos doctor` warns when the radio
is still on the world domain, and `sudo iw reg set <CC>` changes it until the next boot.

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
| `70-kdos-serial.rules` | Serial ports: USB adapters (`ttyUSB*`, `ttyACM*`) and the on-board UARTs (`ttyS*`, `ttyAMA*`), plus the raw USB devices of FTDI, Prolific, Silicon Labs and WCH bridges, which libftdi1 and libserialport open | `dialout` |
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

Two more files there grant no group. `70-kdos-live-medium.rules` hides the medium a live session
is running from, the device mounted at `/mnt/iso` and the partitions of its disk, from udisks
(`UDISKS_IGNORE`), so it is not offered for opening or ejecting; another KDOS stick plugged in is
listed like any stick. `99-qemu-tablet.rules` marks QEMU's emulated USB tablet as a plain pointer,
so in a virtual machine the pointer is handled as a mouse rather than as a graphics tablet or a
joystick.

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

Upstream rules that grant access through a `plugdev` group are not installed: there is no `plugdev`
group, and udev turns an unknown group into root. The display controller's DDC line is also tagged
`uaccess`, which elogind turns into an access entry for the person at the machine, so the
brightness of an external monitor works for any account in the foreground session; the other KDOS
rules grant through their group alone, so a device is reachable from any login of a member,
console and SSH included. That is
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
| `kdos-hwmon.conf` | `nct6775` (Nuvoton), `it87` (ITE), `drivetemp` (SATA disk temperature) | `sensors` and System Monitor show CPU and GPU temperatures only — no fans, voltages or board temperatures |
| `kdos-containers.conf` | `tun`, `overlay` | Container networking and overlay storage |
| `kdos-drm.conf` | `qxl`, `vmwgfx`; it also names `virtio_gpu` and `bochs`, which are built into the kernel | Virtual machines' display drivers |

Each hwmon driver binds only a chip it finds; on other hardware, or where the firmware has claimed
the chip, the load fails and `02_modules` logs a skip line carrying `modprobe`'s own error as the
reason. `sensors-detect` is not shipped, because it
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

**Drawing tablets** need no group. KWin opens them through libinput, and System Settings →
Drawing Tablet maps the pen and its buttons. libinput uses `libwacom`'s database to pair the stylus with its pad, group the pad's buttons and rings, and tell
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

`bluetoothd` is started by `60_bluetooth`. The Bluetooth item in the tray connects devices, and
System Settings → Bluetooth scans, pairs, trusts and forgets them; the session's Bluetooth module
registers the pairing agent, without which the service refuses to pair a keyboard. Sending and
receiving files is the same module's. `bluetoothctl` is the same control from a terminal, for
scripting. Which profile and codec a headset uses is PipeWire's decision, not the
pairing tool's; see [Audio](../03-architecture/session.md#audio).

### Thunderbolt, phones, smart cards and firmware updates

None of these runs under the supervisor. `boltd` and `fwupd` are started by the system bus on
first use and run as root through `dbus-daemon-launch-helper`; `libmtp` and `opensc` are
libraries; a phone's storage is mounted by a process of your own. System Settings → Thunderbolt
lists and authorises Thunderbolt devices.

**Thunderbolt.** Where the firmware's Thunderbolt security level is `user` or `secure` — the
default on most business laptops — a TB3 or TB4 dock is detected and then never authorised, so its
ports stay dead until something approves it. `boltd` approves a dock already enrolled: `43_boltd`
starts it at boot on a machine with a Thunderbolt controller, and a udev rule starts it when a
Thunderbolt device appears later. Enrolling a new dock (`boltctl enroll <uuid>`) asks for an
administrator's password at the machine, remembered for a few minutes, and needs `sudo` anywhere
else — see
[Security model](../03-architecture/security-model.md#what-50-kdosrules-changes).

**Firmware updates.** `fwupd` updates the firmware of SSDs, docks and peripherals from the LVFS, the
Linux Vendor Firmware Service. Discover's Updates page lists what applies to this machine and
installs it; from a prompt, `fwupdmgr get-updates` shows the same and `fwupdmgr update` applies it.
Listing needs no network: each KDOS release carries the LVFS metadata as it stood when the release
was cut, as fwupd's local remote `kdos-snapshot`, and the online `lvfs` remote is disabled, so
nothing asks the LVFS on a schedule. Installing an update downloads its cabinet from the LVFS, a
download you started. For metadata newer than the release:

```sh
sudo fwupdmgr enable-remote lvfs
sudo fwupdmgr refresh
```

System firmware — the BIOS, the embedded controller, and anything the vendor ships as a UEFI
capsule — is applied at the next boot. fwupd places the capsule on the EFI system partition, which
it expects at `/boot/efi` (`EspLocation` in `/etc/fwupd/fwupd.conf`, matching the installer's
`fstab`). Then either the firmware picks it up from disk, or the machine boots once into
`fwupdx64.efi` (from `fwupd-efi`, placed in `EFI/kdos` beside the kernel), which hands it over.
fwupd finds that partition by asking udisks first; when udisks names none, the `fwupd` package is
patched to describe the FAT filesystem mounted at `EspLocation` from sysfs and the udev database. The
live image mounts no EFI partition, so there only devices updated over USB are offered. The loader is unsigned, which is
no extra restriction, since KDOS already requires Secure Boot to be off (see
[Known gaps](../06-reference/known-gaps.md#secure-boot-is-not-supported)).

**Phones.** `libmtp` recognises a phone and gives its USB device to `dialout`. To browse its
storage, unlock the phone, set it to file transfer, and open it from the Device Notifier, which
shows it in Dolphin as `mtp:/`; Android File Transfer, from Kickoff, copies to and from the phone
with no mount. For a directory instead, run `aft-mtp-mount ~/Phone` (from
`android-file-transfer`); any program can then read `~/Phone`, and `fusermount3 -u ~/Phone`
unmounts it. KDE Connect reaches a phone over the network instead, once the `kdeconnect` name is
open in [the firewall](#opening-a-service). libmtp's own `mtp-*` tools reach files by numeric ID only. `gphoto2` and `libgphoto2` cover still cameras in PTP
mode.

**Smart cards.** `pcscd` and `ccid` reach the reader, and `opensc-pkcs11.so` speaks PIV, CAC,
OpenPGP and national ID cards. Its `opensc.module` registers it with p11-kit, so openconnect accepts
a `pkcs11:` URI and GnuTLS programs see the card's certificates; `ssh -I /usr/lib/opensc-pkcs11.so`
uses it directly. `pkcs11-tool --list-slots` tells you whether the card is seen at all. Members of
`wheel` reach smart cards without `sudo`. `/etc/default/pcscd` hides readers by name or adds text
to every reader's name, through pcsc-lite's `PCSCLITE_FILTER_IGNORE_READER_NAMES` and
`PCSCLITE_FILTER_EXTEND_READER_NAMES`; `sudo service restart pcscd` applies it (see
[Configuration](../06-reference/configuration.md#other-machine-files)).

## Printing

CUPS is the print system, started by `80_cups`. The installer turns it off by default, so on an
installed machine enable it before the first print job:

```sh
sudo service enable cups
sudo service start cups
```

System Settings → Printers adds, configures and removes printers, makes one the default and shows
each queue's jobs; the print queue item in the tray shows what is printing. It talks to CUPS as you:
CUPS is built with `lpadmin` and `root` as its system groups, so membership of `lpadmin` is the
authority to manage printers, the `kdos` account has it, and `sudo lpinfo` or `sudo lpadmin` works
as well. A printer that advertises IPP Everywhere, the
driverless protocol, is set up with no driver at all; for one that needs a vendor driver, the
drivers ported with the system (HPLIP, Gutenprint, `brlaser`, SpliX and the Foomatic database) are
offered. `system-config-printer` does the same in another window. From a terminal:

```sh
lpinfo -v                                        # the device URIs CUPS can reach
lpadmin -p <name> -v <uri> -m everywhere -E      # add and enable a driverless queue
lpstat -p -d                                     # the queues, and the default
```

`81_cups-browsed` adds printers shared on the network to CUPS as they appear, and `82_ipp-usb`
serves each IPP-over-USB printer on localhost as it is plugged in, so a USB printer is discovered
the same way. Sharing a printer with other machines needs the `ipp` name in
[the firewall](#opening-a-service), which System Settings'
[Firewall](../04-programs/kdos-settings.md#firewall) page opens.

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
Everything that links it inherits that licence. `LICENSE.notice` in the ffmpeg port
(`ports/core/media-frameworks/ffmpeg/`) is the notice a redistributor should read.

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
`/etc/mpv/scripts` for every user, puts mpv on the session bus, so the media keys and Plasma's
media controller control it. `mpd` reaches the same controller through `kdos-mpctl mpris`, which
the session starts.

**Optical discs.** `mpv` opens `dvd://`, `bd://` and `cdda://`, and plays a DVD's titles and
chapters without its menus. `mpd` and `cmus` play audio CDs; `cd-paranoia` rips one with verified
reads, and `ffmpeg -f dvdvideo -i /dev/sr0` rips a DVD title with its chapters. GStreamer has
`cdiocddasrc`, `dvdreadsrc` and `rsndvdbin`, and `rsndvdbin` plays DVD menus. A CSS-encrypted DVD
opens through `libdvdcss`, which `libdvdread` links. AACS-encrypted Blu-rays do not open, and
Blu-ray Java menus do not run — see
[Known gaps](../06-reference/known-gaps.md#encrypted-blu-ray-discs-do-not-play).

**Other media abilities:**

- `cmus` plays `.m4a` (AAC and ALAC) through its ffmpeg input, alongside its own decoders for MP3,
  FLAC, Vorbis, Opus and the rest.
- GStreamer's `webrtcbin` is built, with ICE from `libnice` and SRTP from `libsrtp`, for
  peer-to-peer media pipelines.
- `mutool barcode` reads and draws QR codes and barcodes in a page or image, through `zxing-cpp`.
- Every pango text layout on the host, GTK's and GStreamer's `textoverlay` among them, breaks Thai
  text at word boundaries found by `libthai`, since Thai puts no spaces
  between words.

**Colour.** `lcms2` is the colour management engine, and the only thing on the host that can apply
an ICC profile.

**Time zones.** The zone data is a compiled zoneinfo tree that musl reads directly, built in the
"fat" format rather than "slim". Applications in containers read the same tree, and a format they
misread would make the host and its containers disagree about local time. The tree also carries
`tzdata.zi`, the text form of the same release, which C++ `std::chrono` time zones read; without it
libstdc++ falls back to the older copy compiled into it.

The zone, the date and network time are set on System Settings → Date & Time, or with
`kdos timezone <Area/City>`. The Date & Time page asks for an administrator's password on every
Apply, even one that changes only the zone, because KDE's page authorises setting the clock before
it saves anything; `kdos timezone` changes the zone with no password for the person at the machine,
and is how an account outside `wheel` changes it. Both go through `kdos-settingsd`, which serves
the clock interface KDE calls (`org.freedesktop.timedate1`) and rewrites two files together:
`/etc/localtime`, the link programs follow into the zoneinfo tree, and
`/etc/profile.d/20-timezone.sh`, which sets `TZ=:/etc/localtime` for every login shell. Programs
already running follow the new zone: KDOS's C library re-reads the link when a program calls
`tzset()`, which Qt does before every local time, so the panel clock moves at its next update. A
program that never calls it, such as a long-running C daemon, keeps the old zone until it restarts.
The same change writes the wireless regulatory country (see [Firmware](#firmware)). On a machine where nothing has chosen a zone, `rcS` links
`/etc/localtime` to UTC at boot. The clock itself is kept by `chrony` (`35_chrony`); turning network
time off on the Date & Time page disables that service. `kdos timezone`, `kdos hostname`,
`kdos locale` and `kdos keymap` read and set the same settings from a prompt.

## Keeping it current

A KDOS release is the whole system, built and tested as one: the ports tree that pins every
package's version, the packages built from it, and the root filesystem they make. An update brings
the next release onto the machine in one piece, into the root slot that is not running, and the
machine starts it at the next restart. A version that fails to start, or whose desktop never comes
up, is rolled back by itself, and the previous version stays one restart away (see
[A/B root slots](installation.md#ab-root-slots)).

**From the desktop.** Discover is where an update is found and installed:

1. **Check for updates** on Discover's Updates page fetches the newest signed release
   information, at most once an hour. The update tray in the panel says when one is ready, and how
   many known vulnerabilities in the installed packages it closes. On its own, nothing reaches the
   network: the nightly job only compares what is already on the machine (see
   [Periodic jobs](#periodic-jobs)).
2. **Update** downloads the next release and writes it into the other slot, with progress. The
   running system is not touched, so you keep working.
3. **Restart** starts the new version, on trial. Once the boot scripts have finished and a Plasma
   session has run for a minute, it is confirmed; a version that never gets that far is rolled back
   at the next start, and a notification says so.

System Settings → System Versions lists both slots, with the version, build date, tree and kernel
each holds, and its one button, **Start This Version Next Time**, boots the other slot once: the
previous version after an update, or the new one again after a rollback. Its one setting is where
updates come from: the KDOS releases, a folder or removable medium, or another address.

**From a prompt**, `kdos update` does the same steps:

```sh
kdos cve                   # which installed versions have known vulnerabilities, offline
kdos update check          # what is behind, what is offered, and the last problem
sudo kdos update fetch     # the newest signed ports tree, then the release index and what it names
sudo kdos update apply     # write the next release into the other slot and arm it
kdos update notes          # the release notes of the version running
sudo kdos update install <port>   # one more port, from the release's binaries
```

| Command | Does |
|---|---|
| `kdos update fetch` | Downloads the latest release's ports tree, checks its signature against `/etc/kdos/keys/tree` and installs it as `/ports`; beside it, the release's signed index of where every update file is, `kdos-update.idx`, and through it alone the signed binary-package index into `/var/lib/kdos/binhost` and the signed slot-image manifest into `/var/lib/kdos/image/offer`, each checked against `/etc/kdos/keys` and against the size and hash the index gives before it is kept. A tree older than the installed one is refused. `--from URL` or `--from DIR` (a stick holding the release's files) reads it from elsewhere |
| `kdos update check` | Lists installed packages whose version and release are older than the ports tree's, which of them the signed binary index covers, whether a newer signed slot image exists, what is offered, and the last problem: no tree, no network, a fetch, a signature or an apply that failed. `--json` prints JSON and `--out PATH` writes it to a file, for Discover and the update tray. `--fetch` runs `fetch` first |
| `kdos update apply` | Writes the next release into the inactive slot and arms it, by one of three lanes (below). `--acquire` and `--arm` run its two halves separately, as Discover does; `--dry-run` says what would happen; `--in-place` updates the running root of a machine with one slot, and gives up the rollback |
| `kdos update install <port>` | Installs one port, and every package it lacks, into the running system from the release's signed binaries; nothing is ever compiled. Discover's host-software installs run it |
| `kdos update theme` | Re-runs the theme generators for your home directory after an artwork upgrade |

`fetch`, `apply` and `install` run as root. Every download is HTTPS, or a file on a local medium; a
redirect to plain HTTP is refused.

**The three lanes.** `kdos update apply` takes the first that applies, and `--image`, `--binhost`
or `--source` names one:

| Lane | What arrives | Network |
|---|---|---|
| The slot image | The release's root filesystem as it was built and tested: a signed manifest of every file with its hash, and the bundles that carry the files. Files the slot or the running system already hold are copied and checked; then the delta from the version this machine runs, which holds only what changed, often as a patch against the same file; then the full image for whatever is still missing. Every file is checked against the signed manifest before it is kept, so a damaged or altered bundle is refused and the full image used instead | The delta, usually far smaller than the release |
| The binary packages | When no newer image is published: each package behind its recipe, prebuilt, from the release's signed binary index, installed into the slot. Used only when the index covers every package that is behind, so a slot is never half prebuilt and half compiled | The packages that changed, as deltas where the previous package is still cached |
| `--source` | Every source the newer recipes name, then every package that is behind, compiled here | Every changed source; a large set takes hours to build |

Whatever the lane, arming a slot carries this machine's own state into it: a configuration file you
edited keeps your copy, with the release's new default beside it as `<file>.kdos-new`; one you did
not edit takes the new default; the account databases are merged by name; and the machine's
network connections, Bluetooth pairings, crontabs, the login screen's state, the packages you added
with `kdos update install` and the paths named by `carry =` lines in `/etc/kdos/update.conf` follow.
The new slot's own `fstab` keeps its own root. The pack store and every home are on the shared
volume and are not in either slot, and so is the record of which data packs are grafted: the slot
that boots grafts every one it names into its own `/usr/share`. `tree =`, `binhost =` and
`image =` in the same file set where each comes from instead of the latest release: `binhost =`
and `image =` name a place holding a `kdos-update.idx`, such as a copy of a build's
`build/binhost` on a stick.

**Vulnerabilities.** `kdos cve` compares your installed versions against a copy of Alpine's security
database shipped in `/usr/share/kdos/secdb.txt`, with no network. It exits 1 when any package is
behind a recorded fix and 0 otherwise. A package that database does not carry is counted as
*unknown*, not clean, and the summary says how many there are. A library a program compiles into
itself, such as the AWS-LC inside six Rust programs, is listed under *bundled copies* with its
version, and checked like a package. Every run prints the database's date, and warns when it is
more than 180 days old. A notification after login says how many known vulnerabilities the
installed packages have.

**The ports tree.** What counts as "newer" is the ports tree, so every image carries one at
`/ports`: each port's `kpkgbuild`, `build.sh` and patches, `ports/shelves`, `ports/sources.idx`,
KDOS's own programs under `/ports/src` and the two books they render under `/ports/docs`, with no
source archive. `PORT_REPO` in `/etc/kpkg.conf` names where `kpkg` looks: a space-separated list of
recipe directories, by default the five of that tree, `/ports/core /ports/src/system /ports/src/art
/ports/src/desktop /ports/src/daemons`. A repository is searched for a port at `<name>/` and one
shelf down, never deeper. For a complete checkout of the KDOS repository at `DIR`:

```sh
PORT_REPO="DIR/ports/core DIR/src/system DIR/src/art DIR/src/desktop DIR/src/daemons"
```

A stick built with `KDOS_ISO_SOURCES=1` also carries the whole tree, fetched sources included,
under `/mnt/iso/sources` (see [kdos rebuild](#kdos-rebuild)). Without a ports tree `kdos update
check` says so and exits 2. The [ports catalogue](../06-reference/ports-catalogue.md) lists every
port by shelf, and [How KDOS is built](../05-developer/how-kdos-is-built.md) follows a port from
recipe to installed package.

After an update, `kdos restarts` lists the running programs still using code the update replaced,
and a notification after login offers to log out when the desktop itself was updated.

**Applications** from the catalogue have no separate update command. An application is a stack of
images this machine built, so rebuilding it is the update: `kdos app remove <id>` and then
`kdos app install <id>` rebuild the application's own image. Under the shipped `snapshot = auto`
the rebuild reads the same Debian snapshot as before, because the pulled `debian:trixie-slim` is
kept; its packages move to a newer snapshot only when the catalogue names a later date or this
machine pulls a newer `debian:trixie-slim`. See [Applications](applications.md#updating).

## Diagnosing

```sh
kdos doctor        # checks kernel, hardware, session, container and desktop set-up
kdos doctor --json # the same, as JSON
kdos status        # what this machine is and what it is running
kdos restarts      # which running processes still use code an upgrade replaced
kdos why <thing>   # what provides something, and why it is that way
kdos cve           # known vulnerabilities (kdos doctor --cve runs the same report)
```

Run `kdos doctor` first when something is wrong; System Settings' Health page, also in Info Center,
shows the same report and runs it again on **Check Again**. Each line is `ok`, `warn`, or `skip`
with a reason: much of what it checks cannot be answered in a virtual machine, and reporting those as `ok`
would claim something never tested. It exits 0 when nothing warned and non-zero otherwise, in both
the text and the JSON form, so a script can act on the status alone. `--cve` runs the
vulnerability report instead (see [Keeping it current](#keeping-it-current)).

The checks are grouped into sections:

| Section | Checks |
|---|---|
| Kernel | Landlock, the kernel's unprivileged file and network sandbox that [`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox) uses, is present and enabled; the initramfs carries this CPU's microcode, and the running revision |
| Hardware | The wireless regulatory database is signed and loaded, and the domain is not the world domain `00`; Intel audio firmware (`sof-firmware`); GPU firmware; device nodes you cannot open, naming the group that owns each (see [the udev rules](#the-device-groups-and-the-udev-rules)); the boot medium |
| Boxes | The application packs |
| Session | `WAYLAND_DISPLAY` names a socket that exists; `XDG_RUNTIME_DIR` is set; the session bus answers; elogind knows the session; KWin, the Plasma shell, the KDE portal, the polkit agent and the password store are running (in a Plasma session only) |
| Services | The root daemons the desktop leans on: `kdos-powerd`, `kdos-energyd` (a `skip` where there is no RAPL energy counter, as in most virtual machines), `kdos-oomd`, `kdos-settingsd`, `kdos-sysupdated`, udisks2 and the others, each running |
| Containers | The mount namespace root is `/`; `/etc/subuid` and `/etc/subgid` carry your range; `podman info` opens your container store |
| Desktop | The accent is applied; Xwayland is installed for X11 applications; the lock screen's PAM stack and `unix_chkpwd` are in place, so the lock screen can check a password; the setuid bits on `newuidmap` and `newgidmap`, and on `dbus-daemon-launch-helper` with group `messagebus`; `fcitx5` is running when it is installed; `~/.local/bin` is on `PATH` |
| Security | Run as root only: whether the `kdos` account still has the shipped password `kdos` |

Without `unix_chkpwd`, the lock screen cannot check a password and locks you out of your own
session, recoverable only from `tty2`; the check prints the fix. The shipped-password check needs to
read `/etc/shadow`, so an ordinary user's run has no Security section at all rather than a check
that pretends to have looked; run `sudo kdos doctor` to get it, and expect the session and container
checks of that run to warn or skip, since root has no desktop session.

`service list` and `service status <name>` show each service's state. After an upgrade,
`kdos restarts` lists the running processes that still use code the upgrade replaced or removed;
each keeps the old version until it restarts. See
[kdos restarts](../04-programs/kdos-command.md#kdos-restarts).

### Logs

`syslogd` (from sysklogd) writes the system log. It reads the kernel's messages itself, so there is
no separate `klogd`. `/etc/syslog.conf` routes them:

| File | Holds |
|---|---|
| `/var/log/messages` | Everything except authentication, the kernel's messages included; readable by root only |
| `/var/log/auth.log` | Logins, `sudo`, polkit and other authentication; readable by root only |
| `/var/log/kern.log` | Kernel messages; readable by root only |
| `/var/log/cron` | The `cron` facility: cronie's record of each user's crontab jobs, and their output |

`syslogd` rotates each file itself at 1 MB and keeps six old copies — `messages.0`, then
`messages.1.gz` to `messages.5.gz`, compressed — so no `logrotate` job exists.
`/var/log/auth.log`, `/var/log/kern.log` and `/var/log/messages` are mode `0600`: `22_syslog`
creates each and sets that mode before `syslogd` starts, and rotation keeps it, so the `.0` copies
and the compressed ones are root-only too. Read them with `sudo`. `syslogd` would otherwise make
them `0644`, and every account could read the kernel's lines that `dmesg` refuses it.
The limit matters on the live medium, where `/var/log` is in RAM and an unbounded log would take
memory rather than disk. Emergency messages are also written to every logged-in terminal.
`syslogd` runs with `-8`, so a message in UTF-8 is written as it was sent and the files stay text
that a plain `grep` reads; only ASCII control characters are rewritten, as `^x`.

Four more places hold what the system log does not, or what it missed while `syslogd` was not
running:

| Where | Holds |
|---|---|
| `dmesg` | The kernel's ring buffer, including messages from before `syslogd` started |
| `/run/kdos-init.<name>.log` | The output of each service script at boot (see [Services](#services)) |
| `/run/kdos-svc.<name>.log` | A supervised daemon's own output and its supervisor's lines, the last 64 KB, with one older file beside it as `.old` |
| `/run/kdos-fsck.log` | The boot-time filesystem check (see [Storage](#storage)) |
| `/var/log/sddm.log` | The login screen |
| `~/.local/share/sddm/wayland-session.log` | Your desktop session's start, and what Plasma and KWin wrote before anything else did |
| `$XDG_RUNTIME_DIR/ksvc/kdos-svc.<name>.log` | One of your own services, such as `pipewire` or `timers` |
| `/var/log/kinstall.log` | The installer, on the machine it ran on |

A supervised daemon's output, and its supervisor's restarts and final exits
(`[KDOS] (name) Exited with code …`), go to the system log under the service's name and to
`/run/kdos-svc.<name>.log`, not to the script's boot log. The file is what is left of the lines
written before `syslogd` started or while it restarted.

## Copying and rebuilding the medium

```sh
sudo kdos clone /dev/sdb          # write this medium to another stick
sudo kdos rebuild /mnt/disk/work  # run the build from a source tree on this machine
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
stick silently wraps around and loses data. The test writes and reads the whole stick and takes
minutes; its lines are shown as they come, with the elapsed time between them. The verify drops the page cache before reading back:
otherwise the kernel would return the bytes it just wrote from memory, not what the flash stored,
and a counterfeit stick would pass.

### kdos rebuild

`kdos rebuild <work-directory>` runs the KDOS build on a booted system, with no network. It runs
as root, because the build enters a chroot. It looks for a source tree wherever `KDOS_SOURCES`
points (passed through `sudo` as `sudo env KDOS_SOURCES=<dir> kdos rebuild …`, since `sudo` clears
the environment), then at `/mnt/iso/sources`, `/kdos` and the
current directory, and accepts one that has `script/kdosbuild.sh`, `ports/core`, `fs/etc` and
`src/devtools/kdosbuild`.

A stick built with `make build KDOS_ISO_SOURCES=1` carries a copy under `/mnt/iso/sources`: the
ports tree with every fetched source beside its recipe, `src/`, `script/` and the `fs/` overlay of
system files. The `Makefile` and the `Dockerfile` are not copied. `kdos rebuild` copies the tree to
the work directory and runs the build there, with `KDOS_WORKSPACE` naming the copy so that the
host phases read `fs/` from it rather than from `/workspace`. No rebuild from a medium has been
run through every phase; see
[Known gaps](../06-reference/known-gaps.md#an-offline-kdos-rebuild-from-the-medium-has-not-been-run-to-the-end),
[Why KDOS](../01-philosophy/why-kdos.md#kdos-can-build-kdos) and
[The kdos command](../04-programs/kdos-command.md#kdos-rebuild).

| Option | Does |
|---|---|
| `--dry-run` | Run the checks and stop |
| `--iso-only` | Rebuild only the image phase, `70_image`, which produces the ISO |

The checks are what make it safe to start. The work directory needs about 25 GB, and is refused if
it is on a temporary or overlay filesystem: a live stick's root is RAM, so a rebuild started there
reports gigabytes free, fills memory and dies hours in. It also stops if a compiler, `make`, `bash`,
`tar` or `xz` is missing, and warns that no ISO will be produced if `mksquashfs`, `xorriso` or
`mkfs.fat` is.

## Tuning for this machine

```sh
kdos march probe          # which instruction set levels this CPU has
sudo kdos march run lz4   # build lz4 twice and measure
kdos march report         # the ledger: kept, reverted, unmeasurable
```

`kdos march probe` reports which of the x86-64 instruction set levels beyond the baseline
(`x86-64-v2`, `-v3` and `-v4`) this CPU supports. `kdos march run <port>` builds a port with and
without `-march=` set to the highest of them, runs the benchmark named by the `bench =` line in
that port's recipe against both builds (the median of five runs by default), and records the
verdict `kept` only where the gain exceeds 3 per cent plus this machine's own measured noise. A
difference inside the noise is recorded as reverted, a port with no `bench =` line as
unmeasurable, and one whose `-march=` build fails as unbuildable. The verdicts are kept in
`/var/lib/kdos/march.ledger`, and `kdos march report` lists reverted ports as prominently as kept
ones. On a CPU with only the baseline level there is nothing to measure, and it says so. `run`
needs root, since the ledger and `kpkg`'s work directory, `/var/cache/kpkg/work`, are root's, and
the port's sources must be fetched first with `sudo kpkg fetch <port>`.

The ledger is a record and nothing else: a `kept` verdict installs nothing and changes nothing
about how `kpkg` or `kdos update apply` builds that port. To use the faster build, rebuild the port
by hand with the level in its flags, as
[Measuring a port with kdos march](../../kfs/16-wiki/local-rebuilds-and-march.md#measuring-a-port-with-kdos-march)
shows. A package built for a level runs only on a processor that has it.

## Backups

Backups are [restic](https://restic.net/) snapshots: encrypted, deduplicated, and readable with
`restic` alone on any machine. There are two ways to make them, and both can share one repository.

**Kup**, System Settings → Backups, is the one set up with the mouse. A *plan* names what to back
up, where to (a folder, an external drive, or a restic repository at any address restic reaches)
and when: by hand, on a timer, or after an amount of active use; a plan on an external drive runs
when the drive is connected and a backup is due. Kup on KDOS has restic as a backup type beside its own rsync synchronisation; its bup type
cannot be chosen, because `bup` is not on the image. A restic plan creates the
repository when it is empty, saves a snapshot tagged `kup`, can check the repository, and can remove
old snapshots by one fixed policy (every snapshot of two days, then daily for a month, weekly for six
months, monthly for two years, yearly after), touching only snapshots tagged `kup`. Its password is
kept in KWallet and reaches restic through a pipe, never as an argument; with the wallet turned
off, as on the live medium, the password field says Not set and the plan needs a password file.
The Backups applet in the tray shows when each plan last ran; the settings page shows the status it
read when it opened, so reopen it to see a run that finished meanwhile.

**`kdos backup run`** is the one written in a file, for the nightly timer and for a prompt.

1. Create the repository: `restic -r <repo> init`. It asks for a password twice and does not store
   it anywhere.
2. Put that password, and nothing else, in `~/.config/kdos/backup.pass`, and make it private with
   `chmod 600 ~/.config/kdos/backup.pass`. `kdos backup run` refuses to run while the file is
   readable by anyone else.
3. Edit `~/.config/kdos/backup.conf` (one `key = value` per line, `#` starts a comment):

   | Key | Means | Repeats |
   |---|---|---|
   | `repo` | Where the repository is | No; the last line wins |
   | `include` | A path to back up | Up to eight; more are ignored |
   | `exclude` | A restic pattern to leave out, such as `*.iso`, passed as written | Up to eight; more are ignored |
   | `password-file` | The password file, when it is not `~/.config/kdos/backup.pass` | No; the last line wins |

   No shell reads the file, so `~` and `$HOME` are not expanded: write every path in full.

4. Run `kdos backup run` and check that it succeeds.
5. To run it nightly, uncomment the `backup` line in `~/.config/kdos/timers.d/10-backup.timer`.

Every snapshot it makes is tagged `kdos`, so Kup's pruning, which removes only `kup` snapshots, never
removes one. It waits for restic, prints restic's own summary and errors, and exits with restic's
status: 3 for a snapshot that missed files it could not read, 10 for no repository, 11 for a locked
one, 12 for a wrong password. It refuses with 1, before touching restic, when `backup.conf` names no
`repo` or no `include`, or the password file is missing or readable by others, so an enabled timer
with no configuration does nothing harmful.

**Restoring.** Dolphin opens a restic repository as a folder, `restic:/`, one folder per snapshot,
with `latest` always the newest; copy what you need out of it. It reads without taking the
repository's lock, so it works while a backup runs and from read-only media. From a terminal the restore is
`restic -r <repo> --password-file <file> restore <snapshot> --target <dir> --include <path>`;
`restic help restore` lists the options, and
[Restoring a file or folder](../../kfs/16-wiki/backups.md#restoring-a-file-or-folder) walks through
both.

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
| An image | `chafa`, as coloured symbols, the same in every terminal. With no filter for images, aerc would instead draw a JPEG or PNG through the terminal's graphics protocol, which Konsole supports |
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
— and with it no `notmuch` index and no offline search. See
[Known gaps](../06-reference/known-gaps.md#mbsync-has-xoauth2-but-not-oauthbearer-and-no-synchroniser-has-met-a-server).

## Passwords and one-time codes

**The session's password store** is KWallet. Programs that keep a secret for you, a network's
passphrase, a mail account's password, a browser's or a backup plan's, keep it there, whether they
are KDE programs or speak the freedesktop Secret Service as GTK and Electron programs do. It opens
with your login password, so you are not asked again; with automatic login, or once you change your
password elsewhere, it asks for its own the first time a program needs it. KWallet Manager browses
and edits it. KeePassXC is installed for KeePass databases, with its own Secret Service switched
off, because two providers of the same name at login race and the loser's programs see an empty
store.

**`pass`**
is a password store for a prompt: one file per entry in a git repository, each encrypted with `gpg`.
There is no database and no format to migrate; even without `pass`, `gpg -d` reads every entry.

For a site that asks for a six-digit code, save the `otpauth://` link its QR code contains, then
ask for a code when you need one:

```sh
pass otp insert site/example        # paste the otpauth:// link
pass otp site/example               # the code for right now
pass otp -c site/example            # and copy it to the clipboard
```

A copy with `-c` is marked as a password, so Klipper keeps it out of the clipboard history, and
after 45 seconds the clipboard goes back to what it held before.

Nothing needs enabling: `pass` loads its installed extensions automatically. The code comes from
`oathtool`, which also works on its own: `oathtool --totp -b <secret>`.

There is no one-time code for logging in to this machine. `oath-toolkit`'s PAM module is not built:
a wrong line in the login stack locks everyone out, including whoever is trying to fix it. These
codes are for other people's websites.

## Calendar and contacts

Calendars and contacts are plain files, like the mail. `~/.local/share/calendars` holds calendars
and `~/.local/share/addressbooks` holds address books, each as a *vdir*: one directory per collection,
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
one empty collection for each, and the image build creates both in the `kdos` home (an account
added later makes its own; see [Users and groups](#users-and-groups)): `khal` reads every directory
under `~/.local/share/calendars/`, of which the image ships one, `personal`, the calendar
`khal import` and `khal new` write into; `khard` reads the one address book
`~/.local/share/addressbooks/personal`. To add a calendar, make a directory under
`~/.local/share/calendars` and put `.ics` files in it; `khal` discovers it with no registration.
`~/.local/share/contacts` is Akonadi's default address book, which KAddressBook and Merkuro
use, and Akonadi treats every directory in it as its own; nothing else writes there.

These are the terminal's calendar and address book. On the desktop, Thunderbird is the default
calendar and address book, and KOrganizer, Merkuro and KAddressBook keep theirs through Akonadi;
each can subscribe to the same CalDAV and CardDAV server `vdirsyncer` syncs with, so the two sides
meet on the server. `ikhal` is on `Meta+Shift+C`.

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
`locate` is the same program under its usual name. An account with no index yet, a new one or a
live session, gets its first at login: the timers service builds it in the background with
`kdos-updatedb --if-missing`, which does nothing when the index exists. Run `kdos-updatedb` to
rebuild it now.

`updatedb` is plocate's, in `/usr/sbin`. findutils' own `locate` and `updatedb` are not installed,
because they read and write a different index format.

Upstream plocate is a setgid program reading one shared index for the whole machine; KDOS ships
neither the setgid bit nor the shared index. The reasoning is in
[the security model](../03-architecture/security-model.md#no-setgid-binaries-and-a-per-user-locate-index).

## Language

KDOS is in English, and only English. Every package is made without its translations: the
message catalogs, the Qt translations, the translated KDE handbooks and the translated manual pages
are removed as it is built (see
[the size and language policy](../05-developer/writing-ports.md#size-and-language-policy)), so
there is no other language for a program to switch to. `/etc/locale.conf` ships
`LANG=en_US.UTF-8`, which SDDM gives every session and `/etc/profile` gives a login shell that has
no `LANG`: American English, with dates such as `11/9/26` and the dollar sign. Setting `LANG` or
`LC_MESSAGES` to another language changes no program's menus or messages.

Writing in another language is unaffected. The fonts cover every script, the keyboard layout is
yours to choose on System Settings → Keyboard, and the input methods below type Chinese, Japanese
and Korean. System Settings → Region & Language sets the formats, dates, numbers, currency and
paper size, by region, which every KDE program follows. What is missing is
listed in [Known gaps](../06-reference/known-gaps.md#every-program-speaks-english).

## Input methods

`fcitx5` is the input method engine, for typing languages that need more than a keyboard layout:
Chinese (pinyin, shuangpin and the table methods), Japanese (Anthy) and Korean (Hangul). KWin starts
it as the session's input method and restarts it if it dies, as `[Wayland] InputMethod` in
`kwinrc` names; System Settings → Keyboard → Virtual Keyboard chooses another, such as the
on-screen keyboard. Qt 6, GTK 3 and GTK 4 programs reach it through KWin's text input, each
toolkit choosing by itself, so no `QT_IM_MODULE` or `GTK_IM_MODULE` is set; fcitx5's own GTK
module serves a GTK program under Xwayland ([Input methods](../03-architecture/session.md#input-methods)). System Settings → Input Method adds and orders the input methods.

`Ctrl+Space` turns the input method on and off, between the keyboard layout and the method used
last; that is fcitx5's own binding. Pressed again with `Ctrl` still held, the second `Space` turns
it off and each one after steps through the methods of the group. The method can also be chosen
from the input method's tray menu. `Meta+Alt+Space` and `Meta+Alt+Shift+Space` cycle fcitx5's
input-method groups, which the Input Method page adds; the image ships one group, so they change
nothing until a second exists. They are set in `/etc/xdg/fcitx5/config`, off fcitx5's default
`Meta+Space`, because `Meta+Space` opens KRunner and KWin's shortcuts are matched before a key
reaches the input method. Keyboard layouts added on System Settings → Keyboard are KWin's, not
fcitx5's, and switch with KWin's `Meta+Alt+K`. fcitx5 draws its candidate window beside the text
being typed, in a program that tells it where the cursor is.

Cloud pinyin is compiled out: it would send what you type to a remote service.

## See also

- [Configuration](../06-reference/configuration.md) — every file and key named here
- [The daemons](../04-programs/daemons.md) — what each root service owns
- [KDOS settings pages](../04-programs/kdos-settings.md) — the Firewall, Health, System Versions and Backups pages
- [Discover](../04-programs/discover.md) — system updates, firmware and the KDOS backend
- [Boot and init](../03-architecture/boot-and-init.md) — the boot path and the service convention
- [The kdos command](../04-programs/kdos-command.md) — every diagnostic on this page
- [Packaging](../03-architecture/packaging.md) — ports, the binary host and updates
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port `kdos update` can install, by shelf
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — why services, packages and settings are arranged this way
- [Security model](../03-architecture/security-model.md) — what `wheel` means and what is not protected
- [Installation](installation.md) — the services, accounts and swap the installer sets up
- [Known gaps](../06-reference/known-gaps.md) — what does not exist

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 10.* Previous: [9. Theming](theming.md) · [Contents](../README.md) · Next: [11. Accessibility](accessibility.md)
