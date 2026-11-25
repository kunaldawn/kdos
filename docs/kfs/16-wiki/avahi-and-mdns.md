# Avahi and mDNS

[Avahi]{.idx} answers and asks multicast [DNS]{.idx} ([mDNS]{.idx}) and DNS service discovery (DNS-SD) on the
local network: it is how this machine finds [printers]{.idx term="printer"}, [scanners]{.idx term="scanner"} and file servers without a DNS
server, and how other machines find it as `<hostname>.local`. This page covers the daemon, its
configuration, the commands that browse, resolve and publish, and what to check when nothing is
found. It also covers how an ordinary program looks up a `.local` name on KDOS, whose C library
has no NSS for `nss-mdns` to plug into: through DNS, by way of
[kdos-mdnsd](#port-kdos-mdnsd).

## Installation

The [avahi](#port-avahi) port provides the daemon, [`avahi-daemon`]{.idx}, and the command-line tools
([`avahi-browse`]{.idx}, `avahi-resolve-host-name`, [`avahi-publish`]{.idx} and the rest). Every image carries it.
Its `postinstall.sh` creates the system accounts the daemon and [`avahi-autoipd`]{.idx} drop to:

<!-- kfs:include ports/core/net-libs/avahi/postinstall.sh lines=24-32 pin=df89e3cad53e -->

It then gives `/var/lib/avahi-autoipd`, which the package carries, to `avahi-autoipd`: the
directory is that account's home and the root `avahi-autoipd` chroots into.

The service script is part of the base filesystem, `fs/etc/init.d/45_avahi.sh`, so it runs at every
boot on the live medium and on an installed system alike; the installer offers no choice for it.
It runs after `40_dbus`, because the daemon is a system-bus service:

<!-- kfs:include fs/etc/init.d/45_avahi.sh -->

The service is named `avahi` (from the file name) and its supervised process `avahi-daemon` (from
`NAME`), which is what `service status avahi` reports: `[ OK ] avahi-daemon  (pid <n>)`.
`avahi-autoipd` and `avahi-dnsconfd` are installed, and no script starts either.

Avahi is a library as well as a daemon. The ports that discover things through it include
[cups](#port-cups) and [cups-browsed](#port-cups-browsed) (printers), [ipp-usb](#port-ipp-usb) (a
USB printer presented as a network one), [sane-backends](#port-sane-backends) and
[sane-airscan](#port-sane-airscan) (scanners), [samba](#port-samba) (which advertises its shares),
[gvfs](#port-gvfs) (the GTK file managers' network view), [kio-extras](#port-kio-extras)
(Dolphin's `network:/` and `smb:/` views) and [pipewire](#port-pipewire) (network audio sinks). The port's
`--enable-compat-libdns_sd` also builds Avahi's `libdns_sd`, for programs written against
Apple's DNS-SD interface.

## Configuration

### /etc/avahi/avahi-daemon.conf

The daemon's configuration is upstream's file as the avahi package installs it; KDOS changes
nothing in it. The keys are described in `avahi-daemon.conf(5)`. After an edit:

```bash
sudo service restart avahi
```

### /etc/avahi/services/

Each `.service` file here is a service the daemon publishes for as long as it runs. Upstream ships
two, for SSH and SFTP; the port deletes them, so this machine advertises no SSH server, whether or
not one is running:

<!-- kfs:include ports/core/net-libs/avahi/build.sh lines=52-52 pin=b6918143adf3 -->

### The firewall

mDNS is UDP port 5353, and the shipped [`/etc/nftables.conf`]{.idx} accepts it in its base rules, before
any [firewall]{.idx} name is turned on. Beside it are the rules for NetBIOS name replies and
WS-Discovery answers, which Dolphin's network browsing asks for alongside its mDNS query:

<!-- kfs:include fs/etc/nftables.conf lines=67-100 pin=2280c755ab79 -->

### /etc/nsswitch.conf

On a glibc system, `nss-mdns` in this file is what lets every program resolve `.local` names. KDOS's
C library, musl, does not read the file at all, and the file says so:

<!-- kfs:include fs/etc/nsswitch.conf -->

### .local names through DNS

musl sends every host lookup to the resolver `/etc/resolv.conf` names, `127.0.0.1`, which is
NetworkManager's dnsmasq. One line in dnsmasq's configuration directory forwards the `local`
domain to [`kdos-mdnsd`]{.idx} on UDP port 5380:

<!-- kfs:include fs/etc/NetworkManager/dnsmasq.d/kdos-mdns.conf -->

`kdos-mdnsd` asks this daemon for each `A` or `AAAA` question, over Avahi's simple-protocol
socket `/run/avahi-daemon/socket`, and answers with the address at a ten-second TTL, or with an
empty answer when Avahi has none within three seconds; dnsmasq caches both. It runs as `nobody`,
started by its own service script after this one:

<!-- kfs:include fs/etc/init.d/45_mdns.sh -->

Its answers, and why a miss is never `NXDOMAIN`, are in
[kdos-mdnsd](../../kdos/04-programs/daemons.md#kdos-mdnsd).

### Who may change the daemon's host name

Any user may browse, resolve and publish through the daemon. Its D-Bus policy,
`/usr/share/dbus-1/system.d/avahi-dbus.conf`, keeps `SetHostName`, which renames the machine on
the local network, to root and the members of `wheel`, the group the avahi port's
`--with-avahi-priv-access-group=wheel` names. A member of `wheel` may run
`avahi-set-host-name <name>`; the name lasts until the daemon restarts.

## Usage

### Seeing what is on the network

1. List every service on the network, resolved to host and port, and exit when the list is
   complete:

   ```bash
   avahi-browse -art
   ```

2. Narrow it to one service type, for example printers or SMB file servers:

   ```bash
   avahi-browse -rt _ipp._tcp
   avahi-browse -rt _smb._tcp
   ```

### Resolving a .local name

1. Ask Avahi for the address:

   ```bash
   avahi-resolve-host-name -4 printer.local
   ```

   It prints the name and one address, separated by a tab. For a machine with several
   interfaces the address may be one the asker cannot reach: asked for its own name, a KDOS
   machine with libvirt's `default` network started answers with `192.168.122.1`, the address of
   its `virbr0` bridge.

2. Or give the name to any program: `ping printer.local`, `ssh kdos@pi.local` and
   `http://printer.local` in a browser resolve through the C library, dnsmasq and `kdos-mdnsd`
   (see [.local names through DNS](#local-names-through-dns)). Check the path with:

   ```bash
   getent hosts printer.local
   service status mdns
   ```

   Every program that connects through libcups — `cupsd`, `lpadmin`, `lpstat`, cups-browsed and
   System Settings' **Printers** page — also asks Avahi itself when the C library cannot answer,
   because of the cups port's `local-names-through-avahi.patch`, so a printer is added by its
   `.local` name on a machine without NetworkManager too. There, for anything else, use the
   address `avahi-resolve-host-name` prints, or put it in `/etc/hosts`.

### Publishing a service

1. For as long as a command runs, `avahi-publish` advertises a service, here a web server on
   port 8080:

   ```bash
   avahi-publish -s "My web page" _http._tcp 8080
   ```

2. For a permanent one, write a file in [`/etc/avahi/services/`]{.idx term="/etc/avahi/services"}. This advertises the SSH server
   that the port leaves unadvertised:

   ```xml
   <?xml version="1.0" standalone="no"?>
   <!DOCTYPE service-group SYSTEM "avahi-service.dtd">
   <service-group>
     <name replace-wildcards="yes">%h</name>
     <service>
       <type>_ssh._tcp</type>
       <port>22</port>
     </service>
   </service-group>
   ```

   Save it as [`/etc/avahi/services/ssh.service`]{.idx}, then run `sudo service restart avahi`.

3. Advertising a service does not open the firewall for it. Open the service's firewall name too;
   see [Opening a service](../../kdos/02-user-guide/administration.md#opening-a-service).

### Turning it off

```bash
sudo service disable avahi
sudo service stop avahi
```

Printers and scanners on the network are then not discovered, Dolphin's network view finds only
the machines that answer NetBIOS or WS-Discovery, `45_mdns` is skipped at the next boot, and no
program resolves a `.local` name. UDP 5353 stays
accepted by the base firewall rules until you edit `/etc/nftables.conf`.

## Tips

- The name other machines find this one by is the hostname set at boot from `/etc/hostname`,
  followed by `.local`. `hostname` prints it, and `pgrep -a avahi` shows the name the daemon holds
  in its process title, as in `avahi-daemon: running [kdos.local]`.
- The daemon announces on every interface with an address, `lo` and libvirt's `virbr0`, while
  its network is started, among them, so `avahi-browse -rt` lists a service of this machine once per interface.
- `avahi-browse -p` prints one record per line, separated by semicolons, for scripts; `-k` shows
  service types by their DNS names rather than translated ones.
- Dolphin's `smb:/` lists file servers from `avahi-browse`'s `_smb._tcp` records, a NetBIOS
  query and a WS-Discovery probe together.

## Troubleshooting

**`avahi-browse` says `Failed to create client object: Daemon not running`.** Check the service:

```bash
service status avahi
cat /run/kdos-init.avahi.log
```

`[DOWN] avahi-daemon` with no log at all is a service turned off with `service disable`, which
the boot skips; `service list` shows it as `[DISABLED]`, and `sudo service enable avahi` followed
by `sudo service start avahi` brings it back.

**The daemon is restarted every five seconds.** `service status avahi` then reports
`[FAIL] avahi-daemon` with the daemon's exit code and the number of failed starts in a row. Read the daemon's own messages in the
supervisor's log:

```bash
tail -n 40 /run/kdos-svc.avahi-daemon.log
```

The daemon exits at once when it cannot claim `org.freedesktop.Avahi` on the system bus, so check
`service status dbus` first.

**Another machine finds this one as `<hostname>-2.local`.** Another machine on the network already
has that name. The log above records the conflict; give one of the two machines a different
hostname.

**A printer or file server is missing from every list.** mDNS does not cross a router, so a
machine on another subnet is never listed; reach it by its address. A machine that is asleep, or
whose own firewall drops UDP 5353, does not answer either.

**`ping name.local` fails.** It prints `ping: name.local: Name does not resolve`, and
`getent hosts name.local` prints nothing after about three seconds. Check each step of the path:

1. `avahi-resolve-host-name -4 name.local` answers: if it does not, the machine is not on this
   network or not announcing, and nothing below can help.
2. `service status mdns` reports `[ OK ] mdns`; its log is `/run/kdos-svc.mdns.log`.
3. `drill -p 5380 name.local @127.0.0.1` asks `kdos-mdnsd` directly, and `drill name.local`
   asks dnsmasq. When the first answers and the second does not, dnsmasq has not read
   `/etc/NetworkManager/dnsmasq.d/kdos-mdns.conf`: NetworkManager is not running with
   `dns=dnsmasq`, or the network is run by `30_network`'s dhcpcd, whose resolver knows nothing of
   `.local`.

A host whose only IPv6 address is link-local resolves to its IPv4 address alone, because a DNS
record cannot say which interface a link-local address belongs to.

## Known gaps

- [The list of Windows shares on the network is whatever answered a broadcast](../../kdos/06-reference/known-gaps.md#the-list-of-windows-shares-on-the-network-is-whatever-answered-a-broadcast):
  a Windows machine announces itself with WS-Discovery, which nothing here speaks, and not with
  mDNS
- [`.local` names resolve only through NetworkManager's dnsmasq](../../kdos/06-reference/known-gaps.md#local-names-resolve-only-through-networkmanagers-dnsmasq):
  on the dhcpcd fallback `ping`, `ssh` and the rest take an address

## See also

- [Services](../../kdos/02-user-guide/administration.md#services): `45_avahi` and `45_mdns` in the
  boot order
- [kdos-mdnsd](../../kdos/04-programs/daemons.md#kdos-mdnsd): the DNS bridge to this daemon
- [The firewall](../../kdos/02-user-guide/administration.md#the-firewall): the base rules, which accept UDP 5353
- [File sharing](file-sharing.md), [Printing and scanning](printing-and-scanning.md),
  [Firewall](firewall.md), [SSH server](ssh-server.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 420.* Previous: [419. File sharing](file-sharing.md) · [Contents](../README.md) · Next: [421. udev and device groups](udev-and-device-groups.md)
