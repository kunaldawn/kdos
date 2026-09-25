# Administration

This page covers running a KDOS machine after the install: services, periodic jobs, networking,
the firewall, storage, users, mail, backups, hardware enablement, updates and diagnosis. Each task
is given with the command that performs it.

There is no configuration abstraction layer here. Every setting below is a file that takes effect,
and [Configuration](../06-reference/configuration.md) is the complete list of them.

## Services

KDOS runs no systemd. Services are scripts in `/etc/init.d`, run in numeric order by `rcS` at
boot and supervised by `ksvc`.

```sh
service list                  # every service, its autostart flag and its state
service status <name>
service start   <name>
service stop    <name>
service restart <name>
service enable  <name>        # start it at boot
service disable <name>        # do not start it at boot
```

`ksvc` is the same program under its other name, so `ksvc sshd status` and `service status sshd`
do the same thing. Enabling and disabling is a flag file: `service disable bluetooth` writes
`/etc/service.disabled/bluetooth`, and `service enable` removes it. Nothing is rewritten in
`/etc/init.d`.

The shipped set, in boot order:

| Script | What it starts |
|---|---|
| `01_udev` | Device management, and the coldplug that loads drivers. First it creates the device nodes of modules that load when their node is opened — `/dev/uhid`, `/dev/uinput`, `/dev/snd/seq`, `/dev/vhost-net` and the rest of `modules.devname` — which no uevent ever names |
| `02_modules` | Modules listed in `/etc/modules-load.d` |
| `03_lvm` | LVM volume groups, activated once at boot, and then any fstab entry on a logical volume checked and mounted. On a disk boot the initramfs has already activated the groups it saw, and `rcS` reaches their entries; this pass catches the rest — every group on a live boot, and any on a disk that appeared after the initramfs ran. Nothing activates a group plugged in later; `vgchange -aay` does it by hand |
| `05_hostname` | The hostname |
| `10_sysctl` | Kernel parameters from `/etc/sysctl.conf` — an unprivileged `ping`; a fatal signal named in the kernel log, without which a program that segfaults takes its window and its reason with it; the `fs.protected_*` link protections, without which a user can redirect a root job's write in `/tmp` onto any file; per-task delay accounting, which `iotop`'s IO% and `htop`'s delay columns read, and which costs the scheduler a clock read per accounted wait |
| `12_zram` | Compressed swap in RAM. Once it is up, zswap is turned off and `vm.page-cluster` is set to 0, since read-ahead on zram is decompression nobody asked for; stopping it restores both, because the setting covers every swap device and disk swap wants the read-ahead |
| `15_userdirs` | `/run/user/<uid>`, and a cgroup subtree per user |
| `18_timers` | Periodic jobs: one supervised `snooze` per line of `/etc/kdos/timers.d` |
| `20_dmesg`, `22_syslog` | Kernel and system logging. `/etc/syslog.conf` sets `secure_mode 2`, so syslogd opens no network socket: it neither accepts messages from other hosts nor forwards to one. `secure_mode 1` allows forwarding to an `@host` action without listening |
| `25_nftables` | The firewall — before the network comes up |
| `30_network` | dhcpcd, the fallback DHCP client. It stands down when NetworkManager is installed and `/etc/service.disabled/networkmanager` is absent — NetworkManager's DHCP client is internal and never defers to a running dhcpcd, so two clients on one link means two leases, two default routes and two writers of `/etc/resolv.conf` |
| `35_chrony` | Time synchronisation |
| `40_dbus` | The system message bus, generating the machine id in `/var/lib/dbus/machine-id` on its first start and linking `/etc/machine-id` to it |
| `41_polkitd` | polkit — before NetworkManager, which asks it on its first privileged call |
| `42_modemmanager` | ModemManager: mobile broadband modems, for NetworkManager and `mmcli`. Started here rather than by D-Bus activation for the reason polkit is |
| `42_networkmanager` | NetworkManager |
| `43_boltd` | Thunderbolt: pings `org.freedesktop.bolt` so the bus activates `boltd`, only when `/sys/bus/thunderbolt/devices` is non-empty. A device plugged in later is covered by the `bolt` package's udev rule |
| `45_avahi` | mDNS |
| `45_seatd` | Seat management, which the desktop needs |
| `46_hostapd` | A wireless access point, only when `/etc/hostapd/hostapd.conf` exists — see [Networking](#networking) |
| `47_pcscd` | Smart cards and security keys: the PC/SC daemon, with readers arriving through udev |
| `50_alsa` | Sound card state |
| `51_mdmonitor` | `mdadm --monitor --scan --syslog`: a failed member, a degraded array or a missing spare reaches the system log. Skipped when `/proc/mdstat` lists no array; a `MAILADDR` or `PROGRAM` line in `/etc/mdadm.conf` adds its own route |
| `52_smartd` | Disk health: `smartd` polls every disk that answers SMART each half hour and logs a failing health check, bad sectors, drive errors and failed self-tests. It stops, logging why, where there is no such disk — any virtual machine |
| `53_xfs_healer` | One `xfs_healer` per XFS filesystem mounted at boot, which logs the metadata damage the kernel reports and, where the filesystem's `autofsck` property asks, has it repaired online. Skipped on a kernel without the XFS health monitor |
| `54_thermald` | Intel thermal management |
| `55_powerd` | Suspend, poweroff and reboot for the desktop |
| `55_tlp` | Laptop power management: `tlp init start` at boot and `tlp init stop` at shutdown, the two verbs that apply the startup and shutdown radio settings in `/etc/tlp.d`. `kdos-powerd` runs `tlp suspend` and `tlp resume` around a suspend |
| `56_energyd` | Per-application energy attribution |
| `57_oomd` | Memory-pressure protection |
| `58_mountd` | Removable media |
| `59_packd` | Application packs |
| `60_bluetooth` | Bluetooth |
| `63_gssd` | `rpc.gssd`, the Kerberos half of an NFS client, which a `sec=krb5` mount cannot work without. It mounts `rpc_pipefs` at `/var/lib/nfs/rpc_pipefs` first. Skipped until `/etc/krb5.keytab` exists |
| `65_brltty` | The console screen reader, skipped until `/etc/brltty.conf` exists. Its speech goes straight to the sound card, so it cannot speak while a desktop session's PipeWire holds the card. See [Accessibility](accessibility.md) |
| `70_sshd` | SSH, generating every missing host key type with `ssh-keygen -A` first |
| `72_nfsd` | The NFSv4 server, skipped until `/etc/exports` names a share. It loads `nfsd`, supervises `nfsdcld` (client records the kernel reclaims after a server reboot) and `nfsv4.exportd`, runs `exportfs -r`, then starts the kernel threads with `rpc.nfsd -N 3 -V 4`. v3 is off, so no portmapper, `rpc.mountd` or lock daemon runs. After editing `/etc/exports`, run `exportfs -r`. Other machines reach it through the `nfs` firewall name |
| `73_mosquitto` | The MQTT broker, as the `mosquitto` account, skipped until `/etc/mosquitto/mosquitto.conf` exists. With no configuration the broker listens on loopback only. A `listener 1883` line there, plus the `mqtt` firewall name, is what lets devices on the LAN reach it |
| `74_prosody` | The XMPP server, as the `prosody` account, skipped until it has an account: `prosodyctl adduser <user>@<host>` makes the first one. Clients are refused until the host has a certificate, and `prosodyctl cert generate <host>` makes a self-signed one. Other machines reach it through the `xmpp` firewall name |
| `76_postgresql` | One shared PostgreSQL server, as the `postgres` account, skipped until a cluster exists: `sudo -u postgres initdb -D /var/lib/postgres/data` makes it. It listens on loopback and a socket in `/tmp` |
| `80_cups` | Printing |
| `81_cups-browsed` | Printers shared on the network, added to CUPS as they appear |
| `82_ipp-usb` | Driverless printing and scanning over USB: each IPP-over-USB device served on localhost as it is plugged in |
| `83_samba` | `smbd`, skipped until a non-empty `/etc/samba/smb.conf` exists. Other machines reach it through the `smb` firewall name |

Shutdown runs the same set backwards: `/etc/init.d/rcK` is the first `::shutdown` entry in
`/etc/inittab`, and runs each enabled script with `stop` in reverse order before anything is
unmounted. The firewall is the exception and stays loaded until power-off. See [Boot and init](../03-architecture/boot-and-init.md#shutdown).

A daemon that cannot do its job on this machine is skipped rather than started and left to fail.
`54_thermald` checks for an Intel processor, `56_energyd` for a readable CPU energy counter,
`57_oomd` that the kernel's pressure interface is writable, and `59_packd` that the kernel can
mount the pack filesystem. Each check happens *before* supervision begins, because a refusing
daemon under a respawn loop is a boot that never settles. Where only the daemon can tell — thermald
knows which Intel models it supports, and exits 2 on one it does not — the script names that status
with `supervise --final-exit 2`, and the supervisor logs the refusal once and stops instead of
restarting it every five seconds.

Supervision is for the scripts that leave a daemon running. Twelve do not: `01_udev`, `02_modules`,
`03_lvm`, `05_hostname`, `10_sysctl`, `12_zram`, `15_userdirs`, `20_dmesg`, `25_nftables`,
`43_boltd`, `50_alsa` and `55_tlp`. `43_boltd` pings `org.freedesktop.bolt` so the bus activates
boltd, and its status asks the bus whether the name has an owner. Most of those apply a setting and exit, and two carry the argument written out in the
script — `25_nftables` and `12_zram` both hand their result to the kernel, which then holds it, so
a supervisor would be a respawn loop around a program that is meant to exit. `01_udev` is the one
that does leave a daemon: `udevd --daemon` forks away from the script, so there is no child for
`ksvc` to watch, and the script's `status` asks `pgrep` instead.

## Periodic jobs

There is no cron daemon. A job is a line in `/etc/kdos/timers.d/*.timer`:

```
NAME  TIMESPEC...  --  COMMAND...
```

`18_timers` starts one supervised `snooze` per line. `snooze` sleeps until its next matching time,
runs the command once, and exits; the supervisor starts it again, so it sleeps again. What a cron
daemon would add over that is a scheduler, and this machine already has a supervisor — one process
per timer, and no shared state to corrupt.

The timespec is `snooze`'s own, and nothing is invented here: `-H 4 -M 17` for 04:17, `-d /2` for
every second day. `snooze(1)` is the whole reference.

`-s` is the missed-run rule, also `snooze`'s. A machine asleep at the slot runs the job once when
it wakes, if it wakes within the slack, rather than once per missed occurrence. A laptop shut for
a fortnight would otherwise run fourteen catch-up jobs at breakfast.

`-s` covers a machine that was *asleep*, not one that was *off*. A `snooze` that has just started
begins looking one second from now, so a slot that passed before it started is the same slot
next time round, whatever the slack says. `-t FILE` is what reaches back: it starts looking from
that file's modification time, and the slack then covers the gap. A job that must survive a reboot
across its slot needs both.

The command is an argument vector rather than a shell line. No pipe, no redirection, no `&&`: a
program that must write a file takes a flag naming it, which is why `kdos update check` carries
`--out`. That is what lets the table be parsed rather than sourced, and it is why a line here
cannot run something nobody wrote. It also means a command here cannot take an argument with a
space in it — the row is split into words and never quoted, so `-- kdos notify "Good morning"`
arrives as `"Good` and `morning"`. A program that needs a sentence reads it from a file of its own,
which is what `kdos remind` does.

A line that does not parse is reported and skipped rather than guessed at. A timer that silently
did not start is indistinguishable from one that has not fired yet, and the difference can be
months.

The shipped system table holds two jobs: the update check, at 04:17 with eight hours of slack, and
`fstrim -a` daily at 12:47 with twenty hours. The installer mounts ext4 and xfs without `discard`,
so the trim is what tells an SSD which blocks are free; without it the drive's write speed and wear
degrade as it fills. btrfs and f2fs discard on their own, and a device that cannot discard is
skipped. The slot is midday because `fstrim` cannot touch a timefile, so a slot the machine was off
for is missed, and a desktop switched off every night would never reach one overnight. The `fwupd`
package adds a third, `30-fwupd-refresh.timer`: the firmware metadata, daily at 13:43, midday for the same reason.

### Your own jobs

Put them in `~/.config/kdos/timers.d/`, in the same shape. Those are started by the session at
login and die with it: a job writing into your home has no business outliving the login that
started it, and `ksvc` could not supervise them anyway, since its pidfiles are in `/run`.

That means one run per login rather than a schedule. `snooze` runs its command once and exits; the
system table repeats only because its supervisor starts it again. A per-user line therefore fires
at most once a session, which is right for a reminder and is not a nightly job — put that in the
system table.

Two jobs ship in the skeleton. The plocate index rebuild at 03:05 is enabled; the nightly backup
at 03:40 is commented out, because a backup nobody asked for is a surprise and the first run over
a home directory can take hours.

## Backups

`kdos-backup` drives restic. Configure it before enabling its timer:

1. Create the repository yourself: `restic -r <repo> init`. It asks for a password twice and
   writes it down nowhere.
2. Put that password in `~/.config/kdos/backup.pass` at mode 0600, with nothing else in the file.
   `kdos-backup` refuses to run while that file is group- or world-readable.
3. Point `repo` at the repository in `~/.config/kdos/backup.conf`, and set `include` and `exclude`
   lines for what to carry. Each may repeat up to eight times; `repo` and `password-file` are the
   other two keys and are last-one-wins.
4. Run `kdos-backup --once` to prove it works, then uncomment the `backup` line in
   `~/.config/kdos/timers.d/10-backup.timer`.

`kdos-backup --once` refuses to run without both the repository and the password file, so an
enabled timer with no configuration writes a message and stops rather than half-running. The
shipped timer sits at 03:40 rather than at 04:17, because two jobs at the same second on a laptop
that has just woken are two jobs competing for one disk.

## Networking

NetworkManager is the manager, with `wpa_supplicant` for wireless, ModemManager for mobile
broadband, `dnsmasq` for DNS, and polkit granting `wheel` the right to change things without a
password.

DNS goes through a local `dnsmasq` that NetworkManager starts on `127.0.0.1` and `::1` and feeds
over D-Bus, set by `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf`. It caches, and it splits: a
VPN's servers answer for the VPN's own domains rather than replacing every server the machine has.
A file of the same name in `/etc/NetworkManager/conf.d` overrides it. `/etc/resolv.conf` has one
writer, `resolvconf` (openresolv), which NetworkManager, the fallback dhcpcd and `wg-quick` all go
through, so none of them overwrites what another wrote.

| Tool | For |
|---|---|
| `kdos-net` (`Super+F4`) | The desktop network manager |
| `kdos-netagent` | The passphrase box NetworkManager raises; started with the session |
| `nmtui` | The full text interface |
| `nmcli` | Scripting |

To connect to a wireless network, open `kdos-net`, choose the network and type the passphrase; it
is written into the profile.

Every later question is `kdos-netagent`'s. NetworkManager never prompts on its own: when it is
activating a profile whose secret is missing or refused, it asks the agents registered with it, and
fails the activation in silence if none answers. So a key that has changed since, 802.1X enterprise
wireless and a VPN one-time code all arrive as a box from the agent rather than from the window you
started in.

The agent stores nothing. NetworkManager also polls its agents for saved secrets on paths nobody is
watching, and those requests are answered at once rather than with a dialog; only a request that
carries the interaction flag raises the box.

OpenVPN is available through NetworkManager, with certificate and password authentication.
Hardware tokens are not built.

WireGuard works both ways. `nmcli connection import type wireguard file wg0.conf` makes it a
NetworkManager profile, whose `DNS=` servers go to the local `dnsmasq` for the tunnel's domains.
`wg-quick up wg0` works too: it hands a `DNS=` line to `resolvconf`, which makes the tunnel's
servers the only ones in `/etc/resolv.conf` until `wg-quick down`.

Mobile broadband is a NetworkManager connection like any other. `42_modemmanager` starts
ModemManager, which finds a built-in WWAN card or a USB modem (MBIM, QMI, QRTR or AT), and
`nmcli connection add type gsm ifname '*' apn <apn>` — or `nmtui`'s mobile broadband entry, which
offers the carriers from `mobile-broadband-provider-info` — connects it. A SIM PIN is asked for by
`kdos-netagent`. `mmcli -m any` shows the modem as ModemManager sees it; `wheel` may unlock and
enable one and read its text messages from there without `sudo`.

Two more connection types come with the same build. PPPoE for a DSL line is
`nmcli connection add type pppoe ifname ppp0 pppoe.parent <ethernet> username <user> password <password>`,
carried by `pppd`. A phone's Bluetooth tethering is `nmcli connection add type bluetooth bt-type panu
bluetooth.bdaddr <phone>` (or `bt-type dun` with an APN for a phone that offers only dial-up), once
the phone is paired in `kdos-bt`.

### An access point with hostapd

`kdos-net`'s hotspot is the short way to share a connection, and needs nothing below. `hostapd` is
for a machine that *is* the network — an access point that stays up across reboots, with no uplink
at all if need be:

1. Write `/etc/hostapd/hostapd.conf`, starting from `hostapd.conf.example` beside it.
   `46_hostapd` starts `hostapd` at boot only when that file exists.
2. Tell NetworkManager to leave the card alone, or both drive it: a file in
   `/etc/NetworkManager/conf.d` with `[keyfile]` and
   `unmanaged-devices=interface-name:<interface>`.
3. Give the interface an address and run a `dnsmasq` of your own on it for DHCP and DNS
   (`interface=`, `bind-interfaces`, `dhcp-range=`), under a script in `/etc/init.d` of your
   own. `bind-interfaces` is required: NetworkManager's own `dnsmasq` already answers on
   `127.0.0.1:53` and `::1:53`, and a second one without it binds the wildcard address, fails
   with the port in use and serves no clients.
4. Let the clients in. The shipped firewall opens DHCP and DNS only for NetworkManager's
   `10.42.0.0/16`, so add a file in `/etc/nftables.d` that accepts `udp dport { 53, 67 }` and
   `tcp dport 53` with `iifname "<interface>"` in the `input` chain, and — if the clients should
   reach anything beyond this machine — accepts forwarding from that interface.

### Syncing and serving on the network

Three programs answer other machines, and each has a name in [the firewall](#the-firewall) that
has to be on first; the shipped policy drops them all.

| Program | Start it with | Firewall name |
|---|---|---|
| Syncthing | **File Sync** in the menu, which runs `kdos-syncthing` in a terminal and opens the web interface on `127.0.0.1:8384` | `syncthing`, on both machines |
| mosh | `mosh <user>@<host>` from the other machine; `mosh-server` is started over SSH | `ssh` and `mosh` |
| Caddy | `caddy run --config /etc/caddy/Caddyfile`, which serves `kiwix-serve` (on 8080) over HTTPS on 8443 | `caddy` |

`kdos-syncthing` gives a user who has no Syncthing configuration yet one with local discovery on
and global discovery, relays, NAT traversal and usage reporting off, so two machines on one network
find each other and nothing reaches a server outside it. A configuration that already exists is
left as it is. Closing the terminal stops the sync.

## The firewall

`/etc/nftables.conf` ships a default workstation policy and `25_nftables` loads it before the
network starts, so there is no window in which the machine is up and unfiltered.

| | |
|---|---|
| Input | Drop by default |
| Forward | Drop by default |
| Output | Accept |
| Established and related | Accept, in both input and forward |
| Invalid | Drop |
| Loopback | Accept |
| ICMP and ICMPv6 | Accept the necessary types |
| mDNS (5353), DHCPv6 (546) | Accept |
| A NetBIOS name reply (137 to 137) | Accept |
| From 10.42.0.0/16: DNS (53) and DHCP (67) | Accept |
| To or from 10.42.0.0/16 | Forward |
| From a `podman*` bridge: DNS (53) | Accept |
| From a `podman*` bridge, or a published port to one | Forward |

ICMPv6 is answered rather than dropped, because dropping it does not harden IPv6 — it breaks
neighbour discovery and path MTU discovery.

The two `10.42.0.0/16` rows are `kdos-net`'s hotspot and nothing else. That is NetworkManager's
`ipv4.method = shared`, which allocates every shared connection a /24 out of that pool and starts
`dnsmasq` on the access point's interface — and NetworkManager's own tables carry no input chain
and cannot rescue a packet this one drops, because in netfilter an `accept` ends only the chain
that issued it while a drop on the hook is final. Without these rows a client associates, ARPs,
answers a ping, and never gets an address. Off that subnet those ports stay closed.

The two `podman*` rows are rootful podman's bridge networks — `sudo podman run`, rootful distrobox
and any network `podman network create` makes — and they live in `/etc/nftables.d/40-podman.nft`
rather than the main file. netavark names every bridge `podman<N>` and adds a forward chain of its
own, which cannot rescue a packet this one drops for the same reason NetworkManager's cannot;
without the rows a container gets an address and reaches nothing, and its names do not resolve,
because `aardvark-dns` answers on the bridge's own address. Rootless boxes forward nothing and need
neither.

The NetBIOS row is a reply rather than a service. A name query is a broadcast and every machine
answers from its own address, so conntrack holds no entry for any of them and "the connection this
machine opened, coming back" does not match. Without the rule, `kdos-mount browse` finds nothing on
a network full of file servers. The rule is scoped to the port pair a reply carries rather than
opening the NetBIOS service: nothing on this image listens there.

Anything that should be reachable needs a rule of its own. sshd, a shared printer and a served
corpus are all blocked by the shipped policy. There are two places to put the rule, and they do not
tread on each other.

`kdos-firewall` (Settings' System page) is the short way, for a short list of named services. It
knows thirteen — `ssh`, `http`, `https`, `ipp`, `smb`, `kiwix`, `mdns`, `mqtt` (1883 and 8883),
`xmpp` (5222), `nfs` (2049), `caddy` (8443, the shipped `Caddyfile`'s site), `mosh` (UDP 60000 to
61000, which also needs `ssh`) and `syncthing` (22000 over TCP and UDP, and discovery on UDP
21027) — and the table of what each
name means lives in `kdos-powerd`, not in the client: a surface that could name a port could open
any port. `kdos-power firewall list` prints the table, and `kdos-power firewall <name> on|off`
flips one. The daemon rewrites `/etc/nftables.d/50-kdos-services.nft` **whole** on every change, so
a rule it does not recognise is dropped on the next toggle.

Everything else is an edit. `/etc/nftables.conf` carries commented examples inside its `input`
chain, and `include "/etc/nftables.d/*.nft"` at the bottom pulls in every file there — so a file of
your own beside `50-kdos-services.nft` re-opens the same chain and is never touched by the surface.
The include is last because nft evaluates in the order rules were added and the chain's policy is
what applies when none matched, so an accept appended there is still reached.

The loader runs a syntax check first, so an unloadable ruleset leaves the previous state standing
rather than half-applying a flush:

```sh
sudo nft -c -f /etc/nftables.conf     # check
sudo service nftables start           # apply
```

A change through `kdos-firewall` reloads the whole file for the same reason: `/etc/nftables.conf`
deletes and rebuilds its `inet filter` table, and the included files are reached only from there.
Only that table is replaced — netavark's `inet netavark` (rootful containers' NAT and published
ports) and NetworkManager's `nm-shared-*` (the hotspot) are left alone — so a file under
`/etc/nftables.d` must add to `table inet filter` rather than declare a table of its own.

## Finding a file by name

`plocate` searches an index instead of the disk, which is what makes a whole-home "where is that
file called…" instant where `fd` has to walk the tree.

The index is yours rather than the machine's. It is rebuilt nightly at 03:05 by a timer in
`~/.config/kdos/timers.d/`, scoped to `$HOME`, and written into `~/.cache/kdos/plocate.db`, so it
can only ever contain paths you could already list. `$LOCATE_PATH` in `/etc/profile.d` is what
points `plocate` at it, and `locate` is the same program under its usual name. Run `kdos-updatedb`
to rebuild it now rather than waiting for the timer. `updatedb` is plocate's, in `/usr/sbin`;
findutils is built without its own `locate` and `updatedb`, which read and write a different format.

Upstream ships a setgid binary reading one shared database for the whole machine, with a permission
check per result; KDOS ships neither the bit nor the shared file. The reasoning is in
[the security model](../03-architecture/security-model.md#and-no-setgid-ones-which-is-why-plocates-index-is-per-user).

## Mail

Four programs share one directory. `~/Mail` is the Maildir, and everything that touches mail is
pointed at it:

| Program | Does |
|---|---|
| `mbsync` | Fetches. Makes an IMAP mailbox and `~/Mail` equal in both directions |
| `notmuch` | Indexes what is there. It never fetches and never sends |
| `aerc` | Reads and writes |
| `msmtp` | Sends. `/usr/sbin/sendmail` and `/usr/bin/sendmail` are both links to it |

`notmuch new` is the only command you type. A `pre-new` hook runs `mbsync -a` before the scan and a
`post-new` hook tags what arrived, so one command fetches, files and indexes.

A non-zero exit from `pre-new` aborts `notmuch new` outright, and the hook uses that deliberately.
It checks three things — that `mbsync` is on the path, that `~/.mbsyncrc` is readable, and that the
file names a `Channel` — and steps over itself with exit 0 if any is missing, so an unconfigured
machine still indexes. Past those three it runs `mbsync -a` and lets a failure take the run down,
because indexing after a fetch that did not happen reports "0 new messages" and reads as an empty
inbox.

Three files to fill in, all shipped commented out:

```
~/.mbsyncrc                            the server, and mode 600
~/.msmtprc                             the outgoing server, and mode 600
~/.config/notmuch/default/config       your own address
```

The fourth, `~/.config/aerc/accounts.conf`, is deliberately not shipped. aerc refuses to start on
one that anyone but you can read, and its own wizard writes it at mode 600 the first time you run
`aerc`. Let the wizard do it.

Keep passwords out of all of them. `pass` is on the image, and both `PassCmd "pass show …"` and
`passwordeval "pass show …"` are in the shipped templates.

### Rendering attachments

Five kinds of part render as text, and nothing else does. `~/.config/aerc/aerc.conf` names one
script, `kdos-part`, for each of them; plain text, a delivery status and an attached message go
through aerc's own `colorize`, and a type the file does not list gets aerc's *No filter configured*
card and its `:open`, `:save` and `:pipe` hints. That section replaces aerc's whole default set
rather than adding to it, which is why the plain-text rows are written out again. The script spools
the part to a temporary file first, because `mutool` opens a document by path and has no form that
reads a pipe.

| Part | Shown as |
|---|---|
| HTML | `w3m -dump`, which lays tables out on the grid, so marketing mail is readable |
| An invitation | aerc's calendar filter, run through `gawk`. It reads and never imports — a filter runs every time a message scrolls past, and one that accepted meetings by being looked at would accept them all |
| PDF | `mutool draw -F txt` |
| An image | `chafa`, as coloured symbols. This row is also what turns *off* aerc's own inline picture: with no filter matching, aerc draws a jpeg or png through the terminal's graphics protocol, which `kdos-term` has. The trade is the same picture in every terminal rather than a better one in some |
| A `.docx` | `docx2txt` |

HTML is rendered behind something that cannot reach the network. `kdos-part` tries
`unshare --map-root-user --net` first and runs w3m behind an unroutable proxy when that is refused;
either way a tracking pixel has nowhere to go. Which one it gets is the boot's: an initramfs whose
`switch_root` chroot()s leaves every process on the machine unable to make a user namespace at all
(see [what is missing](../06-reference/known-gaps.md)), and the filter probes rather than assumes
for exactly that reason. aerc's own HTML filter assumes, by testing whether the `unshare` *binary*
exists, and so puts `unshare: Operation not permitted` where the message should be — a filter's
error output *is* the message body here, because aerc hands it the pager's own pipe.

### OAuth

XOAUTH2 works in all three programs, and `mbsync` needs one line for it. `aerc` speaks it itself
and `msmtp` has it built in; `mbsync` reaches it through `cyrus-sasl` and the `libxoauth2.so`
plugin beside it, which this image carries. `pizauth` mints and refreshes the token in every case —
the plugin asks SASL for the password and wraps whatever it gets as the bearer token, so the
account's `PassCmd` is the whole of the configuration:

```
IMAPAccount work
AuthMechs XOAUTH2
PassCmd   "pizauth show work"
```

OAUTHBEARER is the one mechanism still out of `mbsync`'s reach. The plugin implements XOAUTH2 and
nothing else, so a provider that offers only the newer mechanism is read in `aerc` and sent through
`msmtp` and has no local Maildir — and with it no `notmuch` index and no offline search.

## Passwords and one-time codes

`pass` is the store: a file per entry in a git repository with `gpg` over each. There is no database
and no format to migrate, and if `pass` itself vanished, `gpg -d` would still read every entry.

For a site that wants a six-digit code, save the `otpauth://` URI its QR code encodes and ask for a
code when you need one:

```sh
pass otp insert site/example        # paste the otpauth:// URI
pass otp site/example               # the code for right now
pass otp -c site/example            # and onto the clipboard
```

Nothing has to be enabled. `pass` reads its system extensions with no opt-in, so the command exists
as soon as the package is installed. It generates the code with `oathtool`, which is also usable on
its own: `oathtool --totp -b <secret>`.

There is no one-time password for logging in to this machine. `oath-toolkit`'s PAM module is
deliberately not built: a wrong line in a PAM stack is a machine nobody can log into, including the
person trying to fix it. The codes here are for other people's websites.

## Calendar and contacts

One directory of files, the same shape as the mail. `~/.local/share/calendars` holds calendars and
`~/.local/share/contacts` holds address books, and each is a *vdir*: a directory per collection,
one `.ics` or `.vcf` file per item. That is greppable, diffable, and backed up by copying it. A
file with two events in it is not a vdir, which is why `khal import` exists rather than a text
editor.

| Program | Does |
|---|---|
| `khal` | Prints what is on: `khal list today 7d`, `khal import invite.ics` |
| `ikhal` | The same calendar to move around in, full screen. This is the *Calendar* menu row |
| `khard` | The address book: `khard list`, `khard show`, `khard new` |
| `vdirsyncer` | Makes a server's collection and the local vdir equal — the job `mbsync` does for mail. Nothing is configured, so `vdirsyncer sync` does nothing and exits 0 |

Both are configured and both are empty, which is not the same as unconfigured. `khal` with no
`[calendars]` section and `khard` with no address book both refuse to start, so the shipped files
name a store that has nothing in it. To add a calendar, make a directory under
`~/.local/share/calendars` and drop `.ics` files in it; nothing needs to be registered.

The panel's calendar reads the same store. A day with something on it is marked and today's events
are listed under the month. That popup asks `khal` when it opens and when you change month, so it
costs nothing while it is on screen.

Syncing with a server is `vdirsyncer`, set up in `~/.config/vdirsyncer/config`. A *pair* is two
storages plus the rule for reconciling them, and there is no safe default for that rule: `a wins`
silently discards the server's edit and `b wins` discards yours, so the shipped example makes you
choose. Run `vdirsyncer discover` once, then `vdirsyncer sync` whenever you want, and keep the
password out of the file with `password.fetch = ["command", "pass", "show", "…"]`.

`khard list` exits 1 on an empty address book. It prints `Found no contacts` and means it — that is
khard's answer for "nothing matched" rather than a failure, so anything scripting it has to read a
non-zero exit as an empty result.

## Storage

Swap is activated by `swapon -a` at boot, after `mount -a`. The installer can create a swapfile;
its `fstab` entry is what makes it active.

zram gives you compressed swap in RAM. `/etc/kdos/zram.conf` has two keys:

```
size = 50
algorithm = zstd
```

`size` is a percentage of RAM rather than an absolute size, and it is how much swap the device may
claim to hold — never how much memory it will occupy, since the compressed pages live in that same
memory. It is clamped to 1–90 and anything outside that is reported and replaced by 50. An
`algorithm` the running kernel does not carry is reported too, and the kernel's own default is kept
rather than the device failing to come up.

Once the device is swap, `12_zram` turns zswap off. The kernel starts with zswap on, and zswap is a
compressed cache in front of whatever swap is behind it: in front of zram, every page is compressed
twice and zswap's pool holds memory zram would have used. A machine whose zram did not come up
keeps zswap in front of its disk swap, and `service stop zram` turns it back on.

It also sets `vm.page-cluster` to 0: swap-in read-ahead is sized for a disk's seek, and on zram every
extra page is a decompression nobody asked for. The setting covers every swap device, so it is not
in `/etc/sysctl.conf` — a machine whose zram did not come up keeps the kernel's read-ahead for its
disk swap, and `service stop zram` puts back the default of 3.

Removable media are handled by a root daemon and reached through `kdos-devices` (`Super+F6`).
Everything removable is mounted `nosuid,nodev` and, by default, `noexec`. A setuid binary on
someone else's stick is a local root hole that predates every other consideration, so relaxing that
is explicit: create `/etc/kdos/mountd.conf`, which is not shipped, and put `exec = yes` in it.
`format = yes` in the same file is what allows formatting, on the same argument.

The daemon refuses to offer an internal disk, a filesystem the kernel cannot mount, anything named
in `/etc/fstab`, and the medium the system booted from.

## The session on tty1

A login on `tty1` reaches the desktop. `/etc/inittab` gives `tty1` to `kdos-getty`, which loads the
VT font and hands over to `kdos-login`; that reads `/etc/kdos/login.conf` and either autologins the
account it names or leaves the ordinary password prompt. `tty2` stays a plain getty and is the
recovery console.

`/etc/kdos/login.conf` carries one key:

| Key | Does |
|---|---|
| `autologin` | Which account tty1 logs in without asking. This is the only place the desktop's account is named; commented out means a password prompt |

Renaming the desktop user rewrites `autologin`. It is named in one place and `kinstall` rewrites
that place; a second copy elsewhere would log in an account the installed system does not have,
leaving the machine reachable only from `tty2`. `kdos-getty`'s fallback getty reads the same key
for the same reason.

## Users and groups

One human account ships: `kdos`, in `wheel` and in the hardware groups. `wheel` is what `sudo` and
polkit grant on, and what the root daemons check with `SO_PEERCRED` before answering. `seat` also
reaches suspend, power-off, reboot and removable-media mounting, so an account in `seat` and not in
`wheel` runs a desktop that can power itself down and cannot administer the machine.

Adding a user is `useradd` plus adding them to the groups you want. There is no wizard, though
`kdos-users` gives the same job a surface.

The desktop user's group memberships are load-bearing rather than cosmetic. `dialout`, `audio`,
`video`, `render`, `input`, `kvm`, `cdrom`, `seat`, `lpadmin` and `tty` are each what makes a class
of hardware or administration usable without root, and removing one has a specific, silent
consequence.

`tty` is the one that is not about hardware. `/dev/tty0` is `0620 root:tty` and `/dev/console` is
`0600 root:root`, and a session that has been backgrounded has no controlling terminal for
`/dev/tty` to resolve to — so without that group a backgrounded session cannot carry a
`VT_OPENQRY` on any device and is refused a terminal on a machine that has plenty.

## Hardware

### Firmware

`linux-firmware` ships whole and unpruned, installed with upstream's own script so that the alias
symlinks drivers actually request are created. A curated subset would be a bet on which hardware
you have, and losing that bet is silent.

`sof-firmware` is separate and is required for audio on Tiger Lake and newer — both the DSP
firmware and the topology files, because firmware with no topology loads and binds nothing, which
is still silence.

`wireless-regdb` is built from upstream's `db.txt` and ships with upstream's own signature. The
kernel verifies that signature, and a database it does not cover is rejected silently and leaves the
radio in the world regulatory domain: working, with no 5 GHz DFS channels and reduced transmit
power, and nothing anywhere saying why. The build therefore verifies upstream's signature against
the database it generated and fails if it does not match, so an edited `db.txt` never reaches the
image.

The database holds the rules; the country that picks among them comes from the timezone. The
installer and `kdos-power timezone` both write the zone's country code to
`/etc/modprobe.d/kdos-regdom.conf` as cfg80211's `ieee80211_regdom`, and `kdos doctor` warns when
the radio is still on the world domain `00`. `iw reg set <CC>` changes it until the next boot.

### Microcode

CPU microcode is loaded by the kernel's early loader, which runs before any filesystem exists, so
it rides in front of the initramfs as an uncompressed archive. Late loading is disabled, so this is
the only path.

```sh
kdos doctor        # reports the running microcode revision
```

That check matters because an initramfs rebuilt without the microcode step has no symptom: the
processor keeps whatever the firmware loaded.

### The device groups and the udev rules

The console user can open hardware because of two halves that are both required: membership of a
group, and a rule granting that group the device class. The group alone grants nothing; the rule
alone has no group to grant to. Most of these are `dialout`; the two display ones are `video`, and
game controllers are `input`.

| Rules file | Devices | Group |
|---|---|---|
| `70-kdos-serial.rules` | USB serial adapters — FTDI, CP210x, CH341, CDC-ACM | `dialout` |
| `70-kdos-debug.rules` | In-circuit debuggers and programmers, raw USB and HID: CMSIS-DAP from any vendor, ST-Link, J-Link, Atmel-ICE, PICkit, USBasp, USB-Blaster, XDS110, Nu-Link, KitProg and the rest of openocd's and openFPGALoader's cables | `dialout` |
| `70-kdos-sdr.rules` | Software-defined radio front ends, with every RTL2832U stick librtlsdr knows | `dialout` |
| `70-kdos-usbtmc.rules` | USB Test & Measurement: scopes, meters, function generators — `/dev/usbtmc*` and the raw USB device pyvisa-py opens | `dialout` |
| `70-kdos-sigrok.rules` | Every logic analyser, scope and meter libsigrok's `60-libsigrok.rules` marks | `dialout` |
| `70-kdos-gpio.rules` | `/dev/gpiochip*`, for libgpiod's tools and the GPIO cables of openocd and openFPGALoader | `dialout` |
| `70-kdos-fido.rules` | FIDO2/U2F security keys, for `ssh-keygen -t ed25519-sk` and the `fido2-*` tools | `dialout` |
| `70-kdos-camera.rules` | PTP/MTP cameras, for gphoto2 | `dialout` |
| `70-kdos-scanner.rules` | Flatbed and sheet-fed scanners, for SANE | `dialout` |
| `70-kdos-gamepad.rules` | Game controllers' raw HID, for SDL's HIDAPI drivers | `input` |
| `70-kdos-i2c.rules` | The DDC/CI line of a display controller, for `ddcutil` | `video` |
| `70-kdos-backlight.rules` | The panel's brightness for `kdos-osd`, and the keyboard backlight where there is one | `video` |

Every rule that names a node grants `MODE="0660"` rather than world-readable: these are devices
other users on a multi-user machine have no business reading. The backlight is the exception, and
it cannot be otherwise — see below.

The i2c rule is scoped, and the scoping is the point. `/dev/i2c-*` covers the graphics cards' DDC
lines and the chipset SMBus alike, and every DIMM's SPD EEPROM hangs off the SMBus — a stray write
there is a machine that will not boot. The rule matches only adapters whose PCI parent is a display
controller (`ATTRS{class}=="0x03*"`), so the SMBus is never in it. `ddcutil` ships an unscoped rule
of its own, and the recipe deletes it.

The gpio rule is not scoped, and does not need to be. The kernel refuses a request for a line a
driver already holds, so the lines a user can drive are ones nothing claimed, and a GPIO keeps no
state across a reset.

Upstream rules that grant through `plugdev` or `uaccess` are not installed: there is no `plugdev`
group, eudev resolves an unknown group to gid 0, and nothing here consumes `uaccess`. That is why
libfido2's `70-u2f.rules`, rtl-sdr's, hackrf's, airspy's, bladeRF's, openocd's and openFPGALoader's are absent, and the KDOS
files above carry their devices instead. stlink's are deleted for a different reason: they set
`MODE:="0666"`, final, which would make every ST-Link writable by every account. Where upstream
separates marking from granting, the mark is kept and the grant is ours: libsigrok's
`60-libsigrok.rules` sets `ID_SIGROK`, sane-backends' `20-sane.hwdb` and `65-libsane.rules` set
`libsane_matched`, eudev's `fido_id` sets `ID_SECURITY_TOKEN`, and each `70-kdos-*` file turns its
mark into the group. The FIDO rule therefore covers every key whose HID descriptor declares FIDO,
and the scanner rule every scanner a SANE backend supports.

The backlight rule carries neither `GROUP=` nor `MODE=`, and cannot. A backlight is a class device
with no node in `/dev`, and udev's `GROUP=` and `MODE=` apply to a node — worse, a rule carrying
either is discarded whole for such a device, taking its `RUN+=` with it and reporting nothing. So
that file runs `chgrp video` and `chmod 0664` on the `brightness` attribute instead, on the `add`
event that `01_udev.sh`'s coldplug replays at boot. `0664` is the kernel's own `0644` plus the
group write the rule exists to add.

`/dev/i2c-*` needs a module nothing autoloads. `i2c-dev` declares no modalias, so udev can never
name it; `/etc/modules-load.d/kdos-i2c.conf` is what loads it, and without that the rule has
nothing to grant.

A desktop board's fans, voltage rails and motherboard temperatures are the same case. They sit
behind a Super-I/O chip whose driver — `nct6775` for Nuvoton, `it87` for ITE — declares no modalias,
and so does `drivetemp`, which reads a SATA disk's temperature. `/etc/modules-load.d/kdos-hwmon.conf`
loads all three. Each binds only a chip it finds, so on other hardware, or where the firmware's
ACPI tables have claimed the chip's ports, the load fails and `02_modules` logs a skip. Without them
`sensors` and `kdos-res` show CPU and GPU temperatures and nothing else. `sensors-detect` is not
shipped, because it probes the SMBus by writing to it; `fancontrol` is, and nothing starts it.

One blacklist is load-bearing. `/etc/modprobe.d/kdos-sdr.conf` blacklists `dvb_usb_rtl28xxu`,
because the kernel otherwise claims an RTL2832U dongle as a DVB-T tuner on plug-in and the SDR
library cannot open a device that is present, enumerated and listed by `lsusb`.

Every radio the SDR rule names has its own library and tools — `rtl-sdr`, `hackrf`, `airspy` and
`bladerf` — and a SoapySDR module beside it: `soapyrtlsdr`, `soapyhackrf`, `soapyairspy` and
`soapybladerf`, in `/usr/lib/SoapySDR/modules0.8`. `SoapySDRUtil --find` lists what is plugged
in, and a Soapy program such as `rtl_433 -d driver=hackrf` reaches it. A bladeRF streams nothing
until its FPGA is loaded: the bitstream is Nuand's and is not shipped, so it goes in
`~/.config/Nuand/bladeRF/` or `/etc/Nuand/bladeRF/`, or a bladeRF 2.0 loads it from its own flash.
SDRplay receivers are granted and have no driver: SDRplay's API is a closed library.

A drawing tablet or pen display needs no group: the compositor opens it through libinput, which
looks it up in `libwacom`'s database to pair the stylus with its pad, group the pad's buttons and
rings, and tell a screen-integrated tablet from a desk one for touch arbitration. `libwacom-list-local-devices`
names what it recognised. A model the database lacks works as a plain absolute pointer; describing
it in a `.tablet` file under `/etc/libwacom` and running `libwacom-update-db --skip-systemd-hwdb-update`
and then `sudo udevadm hwdb --update` adds it — the tool's own last step calls `systemd-hwdb`,
which this system does not have.

A GPS receiver starts `gpsd`; nothing else does, and no init script runs it at boot. gpsd's
`70-kdos-gpsd.rules` runs `gpsd.hotplug` for receivers whose USB id names them as one — u-blox,
Garmin, DeLorme, the Holux MediaTek, the Telit module — and for the kernel's `gnss` class, and
`gpsdctl` then adds the node to a running `gpsd` or launches one. Upstream's own rule also matches
the generic CP2102 and ATEN serial bridges, which would have `gpsd` open and probe every ESP32
board and USB adapter plugged in, so it is not installed. A receiver behind a generic bridge is
handed over with `sudo gpsdctl add /dev/ttyUSB0`; until one is, `localhost:2947` refuses
connections.

Without the groups and the rules, every serial programmer, development board, instrument and GPS
receiver in the catalogue is installed and unopenable — which presents as a broken cable rather
than as a permission. `kdos doctor` walks the attached devices and reports each one you cannot
open, naming the group that owns it, because "add yourself to dialout" is an instruction and
"permission denied" is not.

### Thunderbolt, phones, smart cards and firmware updates

No daemon here is supervised. `boltd` and `fwupd` are started by the system bus when something
first calls them, and run as root through `dbus-daemon-launch-helper`; `libmtp` and `opensc` are
libraries, and a phone's filesystem is a FUSE process of the user's own.

`bolt` authorises Thunderbolt devices. Where the firmware's security level is `user` or `secure` —
the shipping default on most business laptops — a TB3 or TB4 dock is enumerated and then never
authorised, so the dock's ports stay dead until something says yes. `boltd` is that something for a
dock already enrolled, and it is started for it: `/etc/init.d/43_boltd.sh` calls it over the bus at
boot when the machine has a Thunderbolt controller, and a udev rule does the same whenever a
Thunderbolt device appears later. Enrolling a new dock is `sudo boltctl enroll <uuid>`. `boltd` is
built with `-Dprivileged-group=wheel` and ships a polkit rule granting that group, but the rule
also requires an active session, which nothing here ever has, so it never fires and the command
needs `sudo` — see [Security model](../03-architecture/security-model.md).

`fwupd` updates firmware on SSDs, docks and peripherals. Updating is `sudo fwupdmgr update`, for
the same reason. `/etc/kdos/timers.d/30-fwupd-refresh.timer` downloads the LVFS metadata daily at
13:43, so `fwupdmgr get-updates` has something to compare against.

System firmware — the BIOS, the embedded controller, and the docks and drives the vendor ships as
UEFI capsules — is applied at the next boot. fwupd stages the capsule on the ESP, which it expects
mounted at `/boot/efi` (`EspLocation` in `/etc/fwupd/fwupd.conf`, the installer's fstab line), and
either asks the firmware to take it from disk or boots once into `fwupdx64.efi` from `fwupd-efi`,
placed in `EFI/kdos` beside the kernel, which hands it over. fwupd finds that partition through
udisks elsewhere; here the `fwupd` recipe patches it to read the partition's number, offset, UUID
and type from sysfs and the udev database instead, because the boot entry for the loader is built
from them. The live image mounts no ESP, so there the UEFI plugin reports none and only devices
updated over USB are offered. The loader is unsigned, which is no restriction here: KDOS already
boots with Secure Boot off.

`libmtp` recognises a phone and grants its USB node to `dialout`; `gphoto2` and `libgphoto2` cover
PTP-mode still cameras only. `aft-mtp-mount ~/Phone`, from `android-file-transfer`, puts the phone's
storage under a directory any file manager reads, and `fusermount3 -u ~/Phone` takes it away. The
phone must be unlocked and set to file transfer, and it is mounted by hand: `kdos-mountd` does not
offer an MTP device. libmtp's own `mtp-*` tools reach files by numeric id only.

A smart card is used through `opensc`: pcscd and ccid reach the reader, and `opensc-pkcs11.so`
speaks PIV, CAC, OpenPGP and the national eID cards to it. Its `opensc.module` puts it in every
p11-kit consumer, so openconnect takes a `pkcs11:` URI and GnuTLS programs see the card's
certificates; `ssh -I /usr/lib/opensc-pkcs11.so` names it directly. `pkcs11-tool --list-slots`
answers whether the card is seen at all.

## Media, colour and time

The host's `ffmpeg` is built with the full codec set: H.264, HEVC, VP8/VP9, AV1 encode and decode,
JPEG XL, MP3, Opus, Vorbis, FLAC, LC3, subtitle burn-in, HDR tone mapping (`zscale`,
`libplacebo`), high-quality resampling (`soxr`), time-stretch (`rubberband`), SVG input
(`librsvg`), text recognition (the `ocr` filter, through `tesseract`), QR codes (`qrencode` and
`qrencodesrc`), the OpenCL filters (`tonemap_opencl`, `nlmeans_opencl`, `unsharp_opencl`,
`overlay_opencl` and the rest) and hardware acceleration through VA-API and Vulkan. The OpenCL
filters run on Mesa's rusticl, which offers an Intel or AMD GPU only because
`/etc/profile.d/50-opencl.sh` sets `RUSTICL_ENABLE`; a machine with neither has no OpenCL device.
One encoder per format, deliberately — a second one for the
same format earns nothing. There is no `ffplay`: it needs SDL, and SDL reaches back to `ffmpeg`
through PipeWire.

Building it that way relicenses the shipped binary to GPL-2-or-later, and everything that links it
inherits that. `ports/core/ffmpeg/LICENSE.notice` is the record a redistributor is expected to read.

Hardware decode goes through VA-API, and which driver answers depends on the GPU. mesa's
`gallium-va` covers r600, radeonsi, nouveau and virgl; Intel integrated graphics from Broadwell
forward are `intel-media-driver` over `intel-gmmlib`, whose versions are coupled hard enough that
a mismatch fails partway through the compile rather than at configure. Sandy Bridge, Ivy Bridge
and Haswell, which that driver refuses, are `libva-intel-driver`'s i965: libva tries `iHD` first and
falls back to `i965`, so each machine loads the one that serves it. Without a driver there is no
VA-API entry point at all and ffmpeg and mpv fall back to software decode, silently. `vainfo`, from
`libva-utils`, is the one command that says which profiles a machine actually has.

`mpv` is the native player. Its `--vo=gpu-next` renderer runs on Vulkan where a device answers
and on OpenGL through EGL where none does, because `libplacebo` is built with both backends. An
external subtitle in a legacy encoding — CP1251, GBK, Shift-JIS — is detected by `uchardet` and
converted rather than drawn as mojibake, and `libass` breaks a long subtitle line where Unicode
allows (through `libunibreak`) rather than only at spaces. The `mpv-mpris` plugin, which `mpv`
loads from `/etc/mpv/scripts` for every user, puts it on the session bus, so the transport keys
and the panel's now-playing cell reach it.

An optical drive plays and rips. `mpv` opens `dvd://`, `bd://` and `cdda://`, and plays a DVD's
titles and chapters without its menus, which `mpv` does not draw; `mpd` and `cmus` play an audio
CD; `cd-paranoia` rips one with verified reads, and `ffmpeg -f dvdvideo -i /dev/sr0` rips a DVD
title with its chapters. GStreamer has `cdiocddasrc`, `dvdreadsrc` and `rsndvdbin`, and
`rsndvdbin` is the one that plays a DVD's menus. A CSS-encrypted DVD or an AACS-encrypted Blu-ray
does not open and a Blu-ray's Java menus do not run — see [Known gaps](../06-reference/known-gaps.md#hardware-and-platform).

`cmus` plays `.m4a` — AAC and ALAC in an MP4 container — through its `ffmpeg` input, beside its
own decoders for MP3, FLAC, Vorbis, Opus and the rest. GStreamer's `webrtcbin` is built, with ICE
from `libnice` and SRTP from `libsrtp`, for peer-to-peer media pipelines. `mutool barcode` reads QR
codes and barcodes out of a page or an image, and draws them, through `zxing-cpp`. Every pango
layout on the host — the compositor's window titles, GStreamer's `textoverlay` — wraps Thai at word
boundaries found by `libthai`, because the script puts no spaces between words.

`lcms2` is the colour management engine, and it is the only thing on the host that can apply an ICC
profile.

Time zone data is a compiled zoneinfo tree that musl reads directly. It is built fat rather than
slim, because applications inside containers read the same tree through the shared filesystem and a
format the container misreads would make host and container disagree about local time on one
machine.

## Input methods

`fcitx5` is the engine, with Chinese (pinyin, shuangpin and the table methods), Japanese and Korean
available. It is started by name from the session — there is no XDG autostart agent here, and the
port is built with `ENABLE_XDGAUTOSTART=Off` to match — speaks `input-method-v2` to `kdos-comp`, and
is configured through text files under `~/.config/fcitx5/`. There is no configuration tool, because
that tool is Qt. `Ctrl+Space` switches method, which is fcitx5's own binding rather than the
compositor's: nothing in `rc.xml` claims it.

The candidate window is ours. `kdos-ime` starts just before fcitx5 and owns `org.kde.impanel`, so
the preedit and the candidate list are drawn as cells like every other KDOS surface; fcitx5's
kimpanel module outranks its own classic interface the moment that name appears, so starting
`kdos-ime` is the whole of selecting it. Without it fcitx5 draws its own window with its own
toolkit.

Cloud completion is compiled out. Sending what you are typing to a remote service is not something
a distribution that builds offline should do by default.

## Keeping it current

```sh
kdos cve                  # which pinned versions carry known vulnerabilities, offline
kdos cve --json           # the same, for a surface
kdos update check         # what the ports tree pins that is not installed
kdos update apply         # take it — binhost first, source second; A/B aware
kdos app install <id>     # rebuild one application against the current snapshot
```

`kdos cve` compares your pinned versions against a vendored security database. A package that
database does not carry is reported *unknown* rather than clean, and the summary says how many are
in that state — a checker that counted them as fine would be reporting a number it had not earned.
The database's age is printed with every run.

Host packages come from ports built here. A signed binary host is available if you run one — see
[Packaging](../03-architecture/packaging.md) — but there is no public archive to pull from.

Applications have no separate update verb. An application is a stack of images this machine built,
so rebuilding it is what an update is, and `kdos app install` over an existing one does exactly
that. See [Applications](applications.md#updating).

## Diagnosing

```sh
kdos doctor       # the things that actually break on this distribution
kdos doctor --cve # and the vulnerability check with it
kdos status       # what this machine is and what it is running
kdos restarts     # which supervised services have been restarting
kdos why <thing>  # what provides something, and why it is that way
kdos stutter      # why the desktop hiccuped, with the application's name
```

`kdos doctor` is the first thing to run when something is wrong. It has a third report level
besides ok and warn — skip, with a reason — because half of what it asks cannot be answered in a
virtual machine, and reporting those as ok would be a green line for something never tested.

Its most useful check is *device present but unopenable*, described above. It also verifies the
setuid bits, which is the worst silent failure in the system: without its setuid bit the password
checker refuses every password and locks you out of your own session.

## Copying and rebuilding the medium

```sh
sudo kdos clone /dev/sdb        # write this medium to another stick
kdos rebuild /mnt/disk/work     # rebuild the ISO from the sources on the medium
```

`kdos clone` is a raw copy, and deliberately nothing cleverer: the boot arrangement is whatever the
medium already carries, so a copy boots exactly what the original boots.

Its length comes from the image's own self-description rather than from the device, so copying a
3 GB image to a 64 GB stick copies 3 GB. Confirmation is typing the device *name* rather than `y`.
Before writing a byte it refuses four things: the medium this system booted from, any disk with a
filesystem mounted anywhere, anything named in `/etc/fstab`, and anything smaller than the image.

The verify afterwards drops the page cache before re-reading, because re-reading without that hands
back the bytes this process just produced rather than the bytes the flash stored — which is exactly
what a counterfeit stick does and exactly what the verify exists to catch.

`kdos rebuild` rebuilds the ISO from sources carried on the medium, with no network, on a medium
built with `make build KDOS_ISO_SOURCES=1`. Its checks are the valuable half: the work directory is
refused when it is on a temporary or overlay filesystem, because a live stick's root is RAM and a
rebuild started there reports gigabytes free, eats memory, and dies hours in.

## Tuning for this machine

```sh
kdos march probe          # which instruction set levels this CPU has
kdos march run lz4        # build lz4 twice and measure
kdos march report         # the ledger: kept, reverted, unmeasurable
```

This builds a port with and without the newer instruction set, runs that port's own benchmark
against both, and keeps the flags only where the win clears both a fixed floor and the machine's
own measured noise. A win inside the noise is not a win, and the tool says so. The report lists
reverts as prominently as wins, which is the evidence that the measuring is real.

## See also

- [Configuration](../06-reference/configuration.md) — every file and key named here
- [The daemons](../04-programs/daemons.md) — what each root service owns
- [Boot and init](../03-architecture/boot-and-init.md) — the boot path and the service convention
- [The kdos command](../04-programs/kdos-command.md) — every diagnostic on this page
- [Packaging](../03-architecture/packaging.md) — ports, the binhost and updates
- [Security model](../03-architecture/security-model.md) — what `wheel` means and what is not protected
