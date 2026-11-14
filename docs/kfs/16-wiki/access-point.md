# Access point

This page covers making a KDOS machine's wireless card an [access point]{.idx} that other devices
join. There are two ways. The **[hotspot]{.idx}** shares the machine's own connection: the network applet, System
Settings or `nmcli` asks NetworkManager for it, and NetworkManager runs everything it needs. **hostapd** makes
the machine itself the network, an access point that stays up across reboots with or without an
uplink, configured by hand.

## Installation

| Port | Provides |
|---|---|
| [networkmanager](#port-networkmanager), [wpa_supplicant](#port-wpa_supplicant) | The hotspot: `ipv4.method = shared` on an access-point profile |
| [dnsmasq](#port-dnsmasq) | The [DHCP]{.idx} and [DNS]{.idx} server, for the hotspot and for a [`hostapd`]{.idx} network |
| [nftables](#port-nftables) | The [firewall]{.idx}, and the address translation NetworkManager installs for a hotspot |
| [hostapd](#port-hostapd) | `hostapd` and `hostapd_cli` |
| [iw](#port-iw) | `iw`, which shows what a card can do |
| [plasma-nm](#port-plasma-nm) | The network applet's **Hotspot** button, and the hotspot's name and passphrase in System Settings → **Wi-Fi & Networking** |

All but hostapd are in the base the ISO carries; `sudo kdos update install hostapd` adds it (see
[Installing software](updating.md#installing-software)). Its service script, [`/etc/init.d/46_hostapd.sh`]{.idx}, is part of the base
filesystem and enabled, and starts nothing while the program is missing
(`[SKIP] hostapd: /usr/sbin/hostapd not found`) or until [`/etc/hostapd/hostapd.conf`]{.idx} exists: at boot it then writes
`[SKIP] hostapd: no /etc/hostapd/hostapd.conf` to `/run/kdos-init.hostapd.log`, and
`service status hostapd` reports `[DOWN] hostapd`. The hotspot needs no service of its own beyond
NetworkManager.

Before either, check that the card can be an access point:

```bash
iw list
```

Under `Supported interface modes`, the list must include `* AP`. `iw list` prints nothing on a
machine with no wireless card, and `nmcli device wifi hotspot` there answers *Error: No Wi-Fi
device found.* The network applet offers its Hotspot button only for a radio that reports the
capability.

## Configuration

### The firewall's hotspot rules

The shipped firewall drops incoming and forwarded traffic by default. These lines of
[`/etc/nftables.conf`]{.idx}, from `fs/`, let a hotspot's clients get an address and a resolver:

<!-- kfs:include fs/etc/nftables.conf lines=105-118 pin=de6e257bf90b -->

and let their traffic through the machine:

<!-- kfs:include fs/etc/nftables.conf lines=132-146 pin=648bdf43390b -->

Both are scoped to `10.42.0.0/16`, the range NetworkManager gives shared connections. A network of
your own made with `hostapd` gets none of this; see
[Serving a network with hostapd](#serving-a-network-with-hostapd). How the file is loaded and why
NetworkManager's own tables survive a reload is in
[/etc/nftables.conf](../../kdos/06-reference/configuration.md#etcnftablesconf).

### The hotspot profile

The network applet creates a profile named *Hotspot*, with the name and passphrase set under
System Settings → **Wi-Fi & Networking** → **Wi-Fi Hotspot**, and `autoconnect` off, so a hotspot
never takes the radio at the next boot. The passphrase must be eight characters or more.
`nmcli device wifi hotspot` creates a profile named *Hotspot* as well, also with `autoconnect`
off. Both are kept in `/etc/NetworkManager/system-connections/`.

NetworkManager starts a separate [`dnsmasq`]{.idx} for each shared connection. It reads any files in
[`/etc/NetworkManager/dnsmasq-shared.d/`]{.idx term="/etc/NetworkManager/dnsmasq-shared.d"}, and keeps its leases in
`/var/lib/NetworkManager/dnsmasq-<interface>.leases`.

### hostapd

<!-- kfs:include fs/etc/init.d/46_hostapd.sh -->

The service script starts `hostapd` only when `/etc/hostapd/hostapd.conf` exists, and runs it in
the foreground with `-s`, so it logs to the system log. The package installs upstream's annotated
example beside that path, from its `build.sh`:

<!-- kfs:include ports/core/network/hostapd/build.sh lines=64-68 pin=989c47c3d1b0 -->

Every key is described in [`/etc/hostapd/hostapd.conf.example`]{.idx} itself; `hostapd(8)` and
`hostapd_cli(1)` cover the two commands.

## Usage

### Sharing the connection from the desktop

1. Set the hotspot's name and a passphrase of at least eight characters once, on System Settings →
   **Wi-Fi & Networking** → **Wi-Fi Hotspot**.
2. Click the network applet in the tray, then **Hotspot**. The applet shows the network's name
   and passphrase while it runs.
3. On the other device, join that network with that passphrase.
4. Click **Hotspot** again to stop. The radio then goes back to the saved network it is normally
   on.

### Sharing the connection from a terminal

1. Start a hotspot with a name and a passphrase of your choice:

   ```bash
   nmcli device wifi hotspot ifname <interface> ssid <name> password <passphrase>
   ```

   `ifname` is a wireless interface from `iw dev`; `nmcli device wifi hotspot` also takes `band`
   and `channel`.

2. Stop it:

   ```bash
   nmcli connection down Hotspot
   ```

   Stop it this way rather than with `nmcli device disconnect`, which also keeps the radio from
   rejoining its usual network on its own.

Members of `wheel` may start a hotspot without `sudo`; the polkit rule grants both
`wifi.share` actions (see [NetworkManager](networkmanager.md#who-may-change-the-network)).

### Serving a network with hostapd

This is the long way, for a machine that is the network. The procedure follows
[An access point with hostapd](../../kdos/02-user-guide/administration.md#an-access-point-with-hostapd).

1. Write the access point's configuration, starting from the example:

   ```bash
   sudo cp /etc/hostapd/hostapd.conf.example /etc/hostapd/hostapd.conf
   sudoedit /etc/hostapd/hostapd.conf
   ```

   Set at least `interface`, `ssid`, `hw_mode` and `channel`, and the `wpa` and `wpa_passphrase`
   keys for a protected network. Set `country_code` to the country the machine is in; see
   [Wireless regulatory domain](wireless-regulatory-domain.md).

2. Tell NetworkManager to leave the card alone, or both drive it at once. Create
   [`/etc/NetworkManager/conf.d/50-hostapd.conf`]{.idx} holding:

   ```ini
   [keyfile]
   unmanaged-devices=interface-name:<interface>
   ```

   then restart NetworkManager with `sudo service restart networkmanager`
   ([Restarting the daemon](networkmanager.md#restarting-the-daemon)).
   `NetworkManager --print-config` shows the `[keyfile]` line, and `nmcli device status` lists
   the card as `unmanaged`. NetworkManager leaves an address and a route the card already had in
   place; `sudo ip addr flush dev <interface>` removes them.

3. Give the interface an address, and run a `dnsmasq` of your own on it for DHCP and DNS, from an
   init script of your own in `/etc/init.d` ([Writing a service](writing-a-service.md)).
   `46_hostapd.sh` is a model for one. Its `dnsmasq`
   needs `interface=<interface>`, `bind-interfaces` and a `dhcp-range=` line, and
   `--keep-in-foreground` on the command line so the supervisor watches it. Without
   `bind-interfaces` it collides with NetworkManager's resolver on port 53 and serves no clients.

4. Let the clients through the firewall: a file of your own in [`/etc/nftables.d`]{.idx}, with a
   name ending in `.nft` (the only files `/etc/nftables.conf` includes), accepting
   `udp dport { 53, 67 }` and `tcp dport 53` with `iifname "<interface>"` in the `input` chain,
   and, if they should reach anything beyond this machine, accepting forwarding from that
   interface. See [Adding a rule of your own](firewall.md#adding-a-rule-of-your-own) for the
   file's shape and how to load it.

5. Start it, or reboot:

   ```bash
   sudo service start hostapd
   ```

KDOS ships no example of the address, the `dnsmasq` script or the firewall file for this route,
and no procedure for routing the clients' traffic to an uplink.

### On the live medium and on an installed system

Both ways are available on the live medium. A `hostapd` network made there lasts until the next reboot
unless the medium has a [persistence store](live-persistence.md); on an installed system `46_hostapd` starts it at every
boot.

## Tips

- `sudo hostapd_cli all_sta` lists the clients associated with a `hostapd` access point. It
  talks to `hostapd` through the `ctrl_interface` the example configuration sets, so keep that
  line.
- A hotspot's clients and their addresses are in its lease file,
  `/var/lib/NetworkManager/dnsmasq-<interface>.leases`, which root may read.
- `sudo nft list tables` shows a `nm-shared-<interface>` table while a hotspot is up: that is the
  address translation NetworkManager installed. A change made with `kdos-firewall` leaves it in
  place. Without a hotspot the list is the firewall's `inet filter` and libvirt's
  `ip libvirt_network` and `ip6 libvirt_network`.

## Troubleshooting

**The network applet offers no Hotspot button.**
No wireless device reported the access-point capability. `iw list` shows whether the card lists
`AP` among its interface modes; if it does not, the card or its driver cannot serve.

**A client joins and never gets an address.**
The firewall dropped its DHCP request. For a hotspot, `sudo nft list ruleset | grep 10.42` should
show the shipped lines above. They are missing only when `/etc/nftables.conf` did not load at boot
even without `/etc/nftables.d`, and the boot fell back to its minimal policy. For
`hostapd`, check that your own `.nft` file in `/etc/nftables.d` is in `sudo nft list ruleset`
and that your `dnsmasq` is running. In both
cases `/run/kdos-init.nftables.log` holds the firewall's boot output.

**A client has an address and reaches nothing beyond this machine.**
For a hotspot, check that the machine itself is online and that `nm-shared-<interface>` is among
`sudo nft list tables`. For `hostapd`, forwarding and address translation are yours to add.

**`hostapd` keeps restarting.**
It exits on an error in its configuration and the supervisor starts it again five seconds later.
`sudo grep hostapd /var/log/messages` shows the line it refused. Running it once in the foreground
with debugging, `sudo hostapd -d /etc/hostapd/hostapd.conf`, after `sudo service stop hostapd`,
prints the same reason on the terminal.

**`hostapd` reports that the interface is busy or cannot be put in AP mode.**
NetworkManager still manages the card. `nmcli device status` must list it as `unmanaged`; see step
2 of [Serving a network with hostapd](#serving-a-network-with-hostapd).

**5 GHz channels are refused.**
The radio is still in the world regulatory domain, where channels that need radar detection are
closed. See [Wireless regulatory domain](wireless-regulatory-domain.md).

## Known gaps

No access point has been started on KDOS hardware: the hotspot, `hostapd` and the lease files are
described from the tree, and on the virtual machine used for checking only the absent-device
answers, the shipped firewall lines, the service's skip and the `unmanaged-devices` setting were
run. The KDOS book lists no gap specific to access points. The one that bears on them is [kdos doctor cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have):
in a virtual machine there is no wireless device to check.

## See also

- [An access point with hostapd](../../kdos/02-user-guide/administration.md#an-access-point-with-hostapd)
- [Firewall](../../kdos/04-programs/kdos-settings.md#firewall): the named services the access point
  needs open
- [The firewall](../../kdos/02-user-guide/administration.md#the-firewall)
- [NetworkManager](networkmanager.md), [DNS](dns.md),
  [Wireless regulatory domain](wireless-regulatory-domain.md)
- [Firewall](firewall.md), [Writing a service](writing-a-service.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 417.* Previous: [416. Mobile broadband](mobile-broadband.md) · [Contents](../README.md) · Next: [418. Wireless regulatory domain](wireless-regulatory-domain.md)
