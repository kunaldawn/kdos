# Administration

This page is for the person who looks after a KDOS machine once it is installed or booted: starting
and stopping services, scheduling jobs, networking and the firewall, storage, accounts, mail,
backups, hardware, updates and diagnosis. Each task is given as the command or file that does it.

If you are new to KDOS, read [Getting started](getting-started.md) first; this page assumes you can
open a terminal and use `sudo`. Two things are worth knowing before anything below:

- **There is no systemd and no settings database.** Services are shell scripts in `/etc/init.d`,
  and every setting on this page is a plain file that takes effect when the program reads it.
  [Configuration](../06-reference/configuration.md) lists every one of those files and keys.
- **The first account is `kdos`, and on the live image its password is `kdos`.** The installer asks
  for a new one. On a machine that still has the shipped password, run `passwd` before anything
  else; `kdos doctor` warns until you do.

When something is wrong, start with [Diagnosing](#diagnosing).

## Services

A *service* is a script in `/etc/init.d` named `NN_name.sh`. At boot `rcS` runs the enabled ones
in numeric order, and `ksvc`, the KDOS service supervisor, restarts any daemon that exits. You
manage them with `service`:

```sh
service list                  # every service, whether it starts at boot, and its state
service status <name>
service start   <name>
service stop    <name>
service restart <name>        # stop, wait one second, start
service enable  <name>        # start it at boot
service disable <name>        # do not start it at boot
```

`ksvc` is the same program under another name, so `ksvc status sshd` and `service status sshd` do
the same thing.

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
| `15_userdirs` | `/run/user/<uid>` for every account with a UID from 1000 to 65533, and a delegated cgroup subtree for each |
| `18_timers` | Periodic jobs: one supervised `snooze` per line of `/etc/kdos/timers.d` — see [Periodic jobs](#periodic-jobs) |
| `20_dmesg`, `22_syslog` | Kernel and system logging. `/etc/syslog.conf` sets `secure_mode 2`, so `syslogd` opens no network socket: it neither accepts messages from other hosts nor forwards to one. `secure_mode 1` allows forwarding to an `@host` action without listening |
| `25_nftables` | The firewall, loaded before the network comes up — see [The firewall](#the-firewall) |
| `30_network` | `dhcpcd`, the fallback DHCP client. It stays down when NetworkManager is installed and not disabled, because NetworkManager's own DHCP client does not defer to it: two clients on one link means two leases, two default routes and two writers of `/etc/resolv.conf` |
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
| `52_smartd` | Disk health: `smartd` polls every disk that answers SMART every half hour and logs a failing health check, bad sectors, drive errors and failed self-tests. Where no disk answers SMART — any virtual machine — it logs why and stops |
| `53_xfs_healer` | One `xfs_healer` per XFS filesystem mounted at boot. It logs the metadata damage the kernel reports and, where the filesystem's `autofsck` property asks for it, has it repaired online. Skipped on a kernel without the XFS health monitor |
| `54_thermald` | Intel thermal management |
| `55_powerd` | `kdos-powerd`: suspend, power-off and reboot for the desktop, and a few system settings (time zone, accent, autologin, firewall names) |
| `55_tlp` | Laptop power management: `tlp init start` at boot and `tlp init stop` at shutdown, which apply the startup and shutdown radio settings in `/etc/tlp.d`. `kdos-powerd` runs `tlp suspend` and `tlp resume` around a suspend |
| `56_energyd` | Per-application energy use |
| `57_oomd` | Memory-pressure protection: ends the heaviest application before the machine stalls, sparing the desktop itself |
| `58_mountd` | Removable media for the desktop |
| `59_packd` | Application packs |
| `60_bluetooth` | Bluetooth |
| `63_gssd` *(nfs-utils)* | `rpc.gssd`, the Kerberos half of an NFS client, which a `sec=krb5` mount needs. It mounts `rpc_pipefs` at `/var/lib/nfs/rpc_pipefs` first. Skipped until `/etc/krb5.keytab` exists |
| `65_brltty` *(brltty)* | The console screen reader, skipped until `/etc/brltty.conf` exists. Its speech goes straight to the sound card, so it cannot speak while a desktop session's PipeWire holds the card. See [Accessibility](accessibility.md) |
| `70_sshd` | The SSH server. Before the first start it generates every missing host key type with `ssh-keygen -A`. The shipped firewall blocks port 22 until you open `ssh` |
| `72_nfsd` *(nfs-utils)* | The NFSv4 server, skipped until `/etc/exports` names a share. It loads `nfsd`, supervises `nfsdcld` (the client records a restarted server needs) and `nfsv4.exportd`, runs `exportfs -r`, then starts the kernel threads with `rpc.nfsd -N 3 -V 4`. NFSv3 is off, so no portmapper, `rpc.mountd` or lock daemon runs. After editing `/etc/exports`, run `sudo exportfs -r`. Open the `nfs` firewall name for other machines |
| `73_mosquitto` *(mosquitto)* | The MQTT broker, as the `mosquitto` account, skipped until `/etc/mosquitto/mosquitto.conf` exists. With no configuration the broker listens on loopback only; a `listener 1883` line there, plus the `mqtt` firewall name, lets devices on the LAN reach it |
| `74_prosody` *(prosody)* | The XMPP chat server, as the `prosody` account, skipped until it has an account: `sudo prosodyctl adduser <user>@<host>` makes the first. Clients are refused until the host has a certificate; `sudo prosodyctl cert generate <host>` makes a self-signed one. Open the `xmpp` firewall name for other machines |
| `76_postgresql` *(postgresql)* | One shared PostgreSQL server, as the `postgres` account, skipped until a cluster exists: `sudo -u postgres initdb -D /var/lib/postgres/data` makes it. It listens on loopback and on a socket in `/tmp` |
| `80_cups` | Printing |
| `81_cups-browsed` | Printers shared on the network, added to CUPS as they appear |
| `82_ipp-usb` | Driverless printing and scanning over USB: each IPP-over-USB device is served on localhost as it is plugged in |
| `83_samba` *(samba)* | `smbd`, skipped until a non-empty `/etc/samba/smb.conf` exists. Open the `smb` firewall name for other machines |

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
restarting` and leaves it stopped, and `service status` reports it as not running. How the supervisor tells that exit from a crash is
in [The daemons](../04-programs/daemons.md#the-shape-they-share).

### Services with no daemon

Twelve scripts leave nothing for the supervisor to watch: `01_udev`, `02_modules`, `03_lvm`,
`05_hostname`, `10_sysctl`, `12_zram`, `15_userdirs`, `20_dmesg`, `25_nftables`, `43_boltd`,
`50_alsa` and `55_tlp`. Most apply a setting and exit — `25_nftables` and `12_zram` hand their
result to the kernel, which keeps it. For these, `service status` reports what the script applied
— `sysctl: applied`, or for zram the swap device and its size — rather than a running process.
Two are exceptions: `service status boltd` says whether the bus has `boltd` running, and
`service status udev` says whether `udevd` is running, because `udevd` runs on its own once the
script has started it.

## Periodic jobs

There is no cron daemon. A periodic job is one line in a file ending `.timer` under
`/etc/kdos/timers.d/`:

```
NAME  TIMESPEC...  --  COMMAND...
```

For example, the shipped update check:

```
update-check  -H 4 -M 17 -s 8h  --  kdos update check --json --out /var/lib/kdos/update.json
```

At boot `18_timers` starts one `snooze` per line under the supervisor, as the service
`kdos-timer-NAME`. `snooze` is a small program that sleeps until the clock matches its options and
then runs a command once. `snooze` sleeps until the next matching time, runs the command once and exits;
the supervisor starts it again, and it sleeps until the next slot. One process per job, and no
shared schedule file to corrupt. `service status timers` shows every job, and
`ksvc check kdos-timer-update-check` shows one.

**The timespec** is `snooze`'s own options, unchanged: `-H 4 -M 17` is 04:17, `-d /2` is every
second day. `snooze(1)` is the full reference.

**Missed runs.** `-s` sets the slack. A machine that was *asleep* at the slot runs the job once
when it wakes, if it wakes within the slack — once, not once per missed slot, so a laptop closed
for a fortnight does not run fourteen catch-up jobs.

`-s` does not cover a machine that was *off*. A `snooze` that has just started looks forward from
now, so a slot that passed before boot is simply missed until the next one. `-t FILE` changes that:
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

## Networking

NetworkManager manages the network, with `wpa_supplicant` for wireless, ModemManager for mobile
broadband and a local `dnsmasq` for DNS. polkit lets members of `wheel` change network settings
without a password.

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

   ```
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
| `caddy` | TCP 8443 (the shipped `Caddyfile`'s site) |
| `mosh` | UDP 60000–61000; also needs `ssh` |
| `syncthing` | TCP and UDP 22000, and UDP 21027 for discovery |

The meaning of each name is fixed inside `kdos-powerd`, not in the tool you click, so no client
can open a port that is not on this list. `kdos-powerd` rewrites
`/etc/nftables.d/50-kdos-services.nft` completely on every change and reloads the firewall, so do
not put your own rules in that file — they disappear at the next toggle.

### Adding your own rules

Anything else is an edit. `/etc/nftables.conf` carries commented examples in its `input` chain,
and its last line, `include "/etc/nftables.d/*.nft"`, reads every `.nft` file in that directory. A
file of your own there is never touched by `kdos-firewall`. For example,
`/etc/nftables.d/60-local.nft`, opening TCP 3000 for a development server, a port no firewall
name covers:

```
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

## Finding a file by name

`plocate` searches an index instead of the disk, so "where is that file called…" across your whole
home is instant, where `fd` has to walk the tree.

The index is per user. A timer in `~/.config/kdos/timers.d/` rebuilds it nightly at 03:05, covering
only `$HOME`, and writes it to `~/.cache/kdos/plocate.db`, so it can only contain paths you could
already list. `LOCATE_PATH`, set in `/etc/profile.d/40-plocate.sh`, points `plocate` at it, and
`locate` is the same program under its usual name. Run `kdos-updatedb` to rebuild it now.

`updatedb` is plocate's, in `/usr/sbin`. findutils' own `locate` and `updatedb` are not installed,
because they read and write a different index format.

Upstream plocate is a setgid program reading one shared index for the whole machine; KDOS ships
neither the setgid bit nor the shared index. The reasoning is in
[the security model](../03-architecture/security-model.md#and-no-setgid-ones-which-is-why-plocates-index-is-per-user).

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
| `~/.config/notmuch/default/config` | Your own address | — |

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

XOAUTH2 sign-in works in all three programs. `aerc` speaks it itself, `msmtp` has it built in, and
`mbsync` reaches it through `cyrus-sasl` and its `libxoauth2.so` plugin, which the image carries.
`pizauth` obtains and refreshes the token. The plugin takes whatever SASL returns as the password
and sends it as the token, so `PassCmd` is the only configuration `mbsync` needs:

```
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
empty ones. To add a calendar, make a directory under `~/.local/share/calendars` and put `.ics`
files in it; `khal` discovers it with no registration.

The panel's calendar reads the same store: days with something on are marked, and today's events
are listed under the month. It asks `khal` when it opens and when you change month, and costs
nothing while it is on screen.

**Syncing with a server** is `vdirsyncer`, configured in `~/.config/vdirsyncer/config`. A *pair* is
two storages plus the rule for settling conflicts. There is no safe default: `a wins` discards the
server's edit and `b wins` discards yours, so the shipped example makes you choose. Run
`vdirsyncer discover` once, then `vdirsyncer sync` whenever you like. Keep the password out of the
file with `password.fetch = ["command", "pass", "show", "…"]`.

`khard list` exits 1 on an empty address book and prints `Found no contacts`. That is its answer
for "nothing matched", not a failure, so a script using it must treat that exit as an empty result.

## Storage

**Swap.** `rcS` runs `swapon -a` at boot, after `mount -a`, so any swap partition or swapfile in
`/etc/fstab` is used. The installer can create a swapfile and writes its `fstab` line.

**zram** is compressed swap held in RAM, set up by `12_zram` from `/etc/kdos/zram.conf`:

```
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
letters, digits, `.`, `_` and `-` replaced), private to you and removed again on unmount. Every removable mount is `nosuid,nodev`, and `noexec` by
default: a setuid program on someone else's USB stick would otherwise be a way to become root.
Create `/etc/kdos/mountd.conf` (not shipped) to change that:

| Key | Effect |
|---|---|
| `exec = yes` | Mount removable media without `noexec`, so programs on them can run |
| `format = yes` | Allow formatting a removable device |

The daemon never offers an internal disk, a filesystem the kernel cannot mount, anything already
mounted, anything named in `/etc/fstab`, or the medium the system booted from. Members of `seat` or
`wheel` may use it.

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

One account ships: `kdos`, a member of `wheel` and of the hardware groups. The installer asks for
its password; on the live image it is `kdos`.

- **`wheel`** is what `sudo` and polkit grant on, and what the root daemons check (through the
  socket's peer credentials) before accepting a configuration change.
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
  `/etc/subgid` for the account; `grep alice /etc/subuid` shows whether `useradd` added one.
- **`/run/user/<uid>`** is created by `15_userdirs` at boot for the accounts that exist then. For an
  account added since, run `sudo service start userdirs` or reboot before its first desktop login.

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
kdos doctor        # says whether the initramfs carries this CPU's microcode, and the running revision
```

The check exists because an initramfs rebuilt without microcode has no symptom: the processor
simply keeps whatever revision the firmware loaded.

### The device groups and the udev rules

Opening a device as a normal user takes two things: membership of a group, and a udev rule that
gives that group the device. Neither works alone. The KDOS rules live in `/etc/udev/rules.d`:

| Rules file | Devices | Group |
|---|---|---|
| `70-kdos-serial.rules` | USB serial adapters — FTDI, CP210x, CH341, CDC-ACM | `dialout` |
| `70-kdos-debug.rules` | In-circuit debuggers and programmers, raw USB and HID: CMSIS-DAP from any vendor, ST-Link, J-Link, Atmel-ICE, PICkit, USBasp, USB-Blaster, XDS110, Nu-Link, KitProg and the rest of openocd's and openFPGALoader's cables | `dialout` |
| `70-kdos-sdr.rules` | Software-defined radio front ends, including every RTL2832U stick librtlsdr knows | `dialout` |
| `70-kdos-usbtmc.rules` | USB Test & Measurement: scopes, meters, function generators — `/dev/usbtmc*` and the raw USB device pyvisa-py opens | `dialout` |
| `70-kdos-sigrok.rules` | Every logic analyser, scope and meter libsigrok's `60-libsigrok.rules` marks | `dialout` |
| `70-kdos-gpio.rules` | `/dev/gpiochip*`, for libgpiod's tools and the GPIO cables of openocd and openFPGALoader | `dialout` |
| `70-kdos-fido.rules` | FIDO2/U2F security keys, for `ssh-keygen -t ed25519-sk` and the `fido2-*` tools | `dialout` |
| `70-kdos-camera.rules` | PTP/MTP cameras, for gphoto2 | `dialout` |
| `70-kdos-scanner.rules` | Flatbed and sheet-fed scanners, for SANE | `dialout` |
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
looks like a broken cable rather than a permission problem. `kdos doctor` lists every attached
device you cannot open and names the group that owns it.

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

**Firmware updates.** `fwupd` updates the firmware of SSDs, docks and peripherals from the LVFS.
Run `sudo fwupdmgr get-updates` to see what is available and `sudo fwupdmgr update` to apply it;
the metadata is refreshed daily by `/etc/kdos/timers.d/30-fwupd-refresh.timer`.

System firmware — the BIOS, the embedded controller, and anything the vendor ships as a UEFI
capsule — is applied at the next boot. fwupd places the capsule on the EFI system partition, which
it expects at `/boot/efi` (`EspLocation` in `/etc/fwupd/fwupd.conf`, matching the installer's
`fstab`). Then either the firmware picks it up from disk, or the machine boots once into
`fwupdx64.efi` (from `fwupd-efi`, placed in `EFI/kdos` beside the kernel), which hands it over.
fwupd's usual way of finding that partition needs udisks, which KDOS does not run; the `fwupd`
package is patched to read it from sysfs and the udev database instead. The live image mounts no
EFI partition, so there only devices updated over USB are offered. The loader is unsigned, which is
no extra restriction, since KDOS already requires Secure Boot to be off (see
[Known gaps](../06-reference/known-gaps.md#hardware-and-platform)).

**Phones.** `libmtp` recognises a phone and gives its USB device to `dialout`. To browse its
storage, unlock the phone, set it to file transfer, and run `aft-mtp-mount ~/Phone` (from
`android-file-transfer`); any file manager can then read `~/Phone`, and `fusermount3 -u ~/Phone`
unmounts it. `kdos-mountd` does not offer MTP devices, so this is always by hand. libmtp's own
`mtp-*` tools reach files by numeric ID only. `gphoto2` and `libgphoto2` cover still cameras in PTP
mode.

**Smart cards.** `pcscd` and `ccid` reach the reader, and `opensc-pkcs11.so` speaks PIV, CAC,
OpenPGP and national ID cards. Its `opensc.module` registers it with p11-kit, so openconnect accepts
a `pkcs11:` URI and GnuTLS programs see the card's certificates; `ssh -I /usr/lib/opensc-pkcs11.so`
uses it directly. `pkcs11-tool --list-slots` tells you whether the card is seen at all. Members of
`wheel` reach smart cards without `sudo`.

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

This build makes the `ffmpeg` binary GPL-2-or-later, and so is everything that links it.
`ports/core/ffmpeg/LICENSE.notice` is the notice a redistributor should read.

**Hardware video decoding** goes through VA-API, and the driver depends on the GPU:

| GPU | Driver |
|---|---|
| AMD (r600, radeonsi), NVIDIA through nouveau, virgl | Mesa's `gallium-va` |
| Intel, Broadwell and newer | `intel-media-driver` (`iHD`), over `intel-gmmlib` |
| Intel Sandy Bridge, Ivy Bridge, Haswell | `libva-intel-driver` (`i965`) |

libva tries `iHD` first and falls back to `i965`, so each machine loads the one that serves it.
With no driver, ffmpeg and mpv silently fall back to software decoding. `vainfo`, from
`libva-utils`, lists the profiles a machine actually has.

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

## Keeping it current

```sh
kdos cve                  # which installed versions have known vulnerabilities, offline
kdos cve --json           # the same, as JSON
kdos update check         # what the ports tree pins that is not installed
kdos update apply         # install it: prebuilt package first, else compile; A/B aware
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
for KDOS's own programs — so name all three. On a stick built with `KDOS_ISO_SOURCES=1`, which
carries the source tree under `/mnt/iso/sources`:

```
PORT_REPO="/mnt/iso/sources/ports/core /mnt/iso/sources/src/packages /mnt/iso/sources/src/desktop"
```

For a checkout of the KDOS repository at `DIR`, name the same three subdirectories of `DIR`. Without
a ports tree `kdos update` says so and exits 2.

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
rebuild it against the catalogue's current snapshot of the Debian archive. Installing an application
that is already installed does nothing. See [Applications](applications.md#updating).

## Diagnosing

```sh
kdos doctor        # checks for the things that actually break on KDOS
kdos doctor --json # the same, as JSON
kdos status        # what this machine is and what it is running
kdos restarts      # which running processes still use code an upgrade replaced
kdos why <thing>   # what provides something, and why it is that way
kdos stutter       # why the desktop hiccuped, naming the application
kdos cve           # known vulnerabilities (kdos doctor --cve runs the same report)
```

Run `kdos doctor` first when something is wrong. Each line is `ok`, `warn`, or `skip` with a
reason: much of what it checks cannot be answered in a virtual machine, and reporting those as `ok`
would claim something never tested.

Among its checks:

- **The shipped password.** It warns while the `kdos` account's password is still `kdos`.
- **Devices you cannot open**, naming the group that owns each one — see
  [the udev rules](#the-device-groups-and-the-udev-rules).
- **The setuid bits** on `kdos-checkpass` and `kdos-resctl`. Without its bit, `kdos-checkpass`
  cannot read `/etc/shadow`, so the lock screen refuses every password and locks you out of your
  own session — recoverable only from `tty2`. The fix it prints is `chown root` and `chmod 4755`.
- **Microcode** and the **wireless regulatory domain**, described above.

`service list` and `service status <name>` show each service's state. After an upgrade,
`kdos restarts` lists the running processes that still use code the upgrade replaced or removed;
each keeps the old version until it restarts. See
[kdos restarts](../04-programs/kdos-command.md#kdos-restarts).

## Copying and rebuilding the medium

```sh
sudo kdos clone /dev/sdb        # write this medium to another stick
kdos rebuild /mnt/disk/work     # rebuild the ISO from the sources on the medium
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

`kdos rebuild <work-directory>` rebuilds KDOS from the sources carried on the medium, with no
network. It needs a stick built with `make build KDOS_ISO_SOURCES=1`, which copies the source tree
— including every source archive `make fetch` placed for that build — onto the ISO at
`/mnt/iso/sources`. It also finds a tree at `/kdos`, in the current directory, or wherever
`KDOS_SOURCES` points.

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

`kdos march run <port>` builds a port with and without the newer instruction set level this CPU
supports, runs that port's own benchmark against both, and keeps the faster flags only where the
gain clears both a fixed minimum and this machine's own measured noise. A gain inside the noise is
not a gain, and the tool says so. The report lists reverted ports as prominently as kept ones. On a
CPU with only the baseline x86-64 level there is nothing to measure, and it says so.

## See also

- [Configuration](../06-reference/configuration.md) — every file and key named here
- [The daemons](../04-programs/daemons.md) — what each root service owns
- [Boot and init](../03-architecture/boot-and-init.md) — the boot path and the service convention
- [The kdos command](../04-programs/kdos-command.md) — every diagnostic on this page
- [Packaging](../03-architecture/packaging.md) — ports, the binary host and updates
- [Security model](../03-architecture/security-model.md) — what `wheel` means and what is not protected
- [Known gaps](../06-reference/known-gaps.md) — what does not exist yet
