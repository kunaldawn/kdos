# NetworkManager

[NetworkManager]{.idx} brings up and keeps up every link a KDOS machine has: Ethernet, Wi-Fi,
mobile broadband, PPPoE, [Bluetooth]{.idx} tethering, VPNs and a shared [hotspot]{.idx}. This page covers the
daemon itself: how it is started, the files it reads, the tools that drive it ([`kdos-net`]{.idx},
[`kdos-netagent`]{.idx}, `nmtui` and `nmcli`), and how to hand the network back to [`dhcpcd`]{.idx} on a machine
that should not run it. Name resolution, VPNs, modems, access points and the Wi-Fi country each
have a page of their own, listed under [See also](#see-also).

## Installation

The [networkmanager](#port-networkmanager) port builds the daemon, `nmcli`, `nmtui` and the client
library `libnm`. Its `depends` line brings in the programs NetworkManager runs or talks to:
[wpa_supplicant](#port-wpa_supplicant) for wireless authentication,
[modemmanager](#port-modemmanager) for modems, [ppp](#port-ppp) for PPPoE and serial modems,
[dnsmasq](#port-dnsmasq) for the local resolver and the hotspot, [openresolv](#port-openresolv) for
[`/etc/resolv.conf`]{.idx}, and [nftables](#port-nftables) for a hotspot's address translation. All of
them are in the `network` shelf's list of `41_system`, so they are part of every image. The desktop's two
front ends, `kdos-net` and `kdos-netagent`, are programs of [kdos-shell](#port-kdos-shell); they
talk to the daemon over the [system bus]{.idx} and do not link `libnm`.

Nothing has to be installed. To confirm the package and its files on a running machine:

```bash
kpkg info networkmanager
sudo kpkg check networkmanager
```

NetworkManager is started at boot by [`/etc/init.d/42_networkmanager.sh`]{.idx}, which comes from the
tree's `fs/` and not from the package. It sorts after `40_dbus`, `41_polkitd` and
`42_modemmanager`, and each of those three returns only once its daemon holds its bus name, so
the system bus, polkit and ModemManager are already answering when NetworkManager starts;
the comment in `42_modemmanager.sh`, shown on [Mobile broadband](mobile-broadband.md#the-service-script),
says why ModemManager must come first. The service is enabled on the live medium. The installer's
*Services* page has it ticked by default; untick it, and the installed system gets the marker
[`/etc/service.disabled/networkmanager`]{.idx} (see [Services](../../kdos/02-user-guide/installation.md#services)).

[`/etc/init.d/30_network.sh`]{.idx} starts `dhcpcd`, the fallback [DHCP client]{.idx} from the
[dhcpcd](#port-dhcpcd) port, and stands down whenever NetworkManager is installed and not
disabled, so only one of the two runs. Nothing else starts NetworkManager: the image ships no
[D-Bus activation]{.idx} file for its bus name (see [D-Bus activation](#d-bus-activation)).

## Configuration

### The service script

<!-- kfs:include fs/etc/init.d/42_networkmanager.sh -->

`--no-daemon` keeps NetworkManager in the foreground, so that `ksvc` supervises the daemon itself
and restarts it five seconds after it exits. `--restart-with dbus` ties it to the system bus:
NetworkManager does not exit when its bus connection closes, so whenever the bus's daemon is
restarted the supervisor stops NetworkManager and starts it again a second after the bus is back,
so that it takes its name on the new bus. The service's name for `service` is
[`networkmanager`]{.idx}, the file name without its number and `.sh`; its boot output goes to
`/run/kdos-init.networkmanager.log`.

### Which DHCP client runs

<!-- kfs:include fs/etc/init.d/30_network.sh lines=15-28 pin=1a8efb9edf93 -->

The test reads the marker, not the binary alone, so disabling NetworkManager hands [DHCP]{.idx} to
`dhcpcd` instead of leaving the machine with no client. Why two clients on one link cannot work is
in [One DHCP client on the link](../../kdos/03-architecture/boot-and-init.md#one-dhcp-client-on-the-link).

### The daemon's configuration

KDOS ships no [`/etc/NetworkManager/NetworkManager.conf`]{.idx}. NetworkManager reads its drop-in
directories instead: [`/usr/lib/NetworkManager/conf.d`]{.idx} for files a package owns, and
[`/etc/NetworkManager/conf.d`]{.idx} for yours. A file in `/etc/NetworkManager/conf.d` with the same name
as one in `/usr/lib/NetworkManager/conf.d` replaces it. The port installs one such file, written by
its `build.sh`:

<!-- kfs:include ports/core/network/networkmanager/build.sh lines=64-69 pin=954e879afefd -->

`dns=dnsmasq` is the whole of the KDOS-specific configuration; what it does is on the
[DNS](dns.md) page. The keys a drop-in may set are in `NetworkManager.conf(5)`, which the port
installs. `NetworkManager --print-config` prints the configuration the files add up to. A changed
drop-in takes effect when the daemon restarts; see [Restarting the daemon](#restarting-the-daemon).

### Restarting the daemon

```bash
sudo service restart networkmanager
```

The restart works the same in a desktop session as outside one. `kdos-netagent` calls
NetworkManager's bus name within moments of the daemon leaving the bus, and while the name has no
owner the bus refuses the call, because nothing lets it start NetworkManager itself (see
[D-Bus activation](#d-bus-activation)); the agent tries again every five seconds, and at once
when the name gains an owner, so it registers with the supervised daemon as soon as that has the
name. `pgrep -a NetworkManager` then shows one
process, `/usr/sbin/NetworkManager --no-daemon`.

### Connection profiles

Every network you join is saved as a **profile**, a file in
`/etc/NetworkManager/system-connections/` named after the profile with `.nmconnection` added. The
files hold passphrases in plain text, are owned by root with mode 600, and NetworkManager ignores a
file there that any other user can read or write. `kdos-net` and `nmcli` write them for you;
`nm-settings-keyfile(5)` documents the format. NetworkManager does not notice a file edited by
hand until it is told to read the directory again:

```bash
sudo nmcli connection reload
```

Every profile that works on KDOS is a system profile, one with no owner in its
`connection.permissions` list. NetworkManager is built without session tracking, so it sees no
user as logged in, and a profile that names an owner never connects on its own. It is still
listed by `nmcli connection show`, and `nmcli connection up` brings it up by hand, which is why the
fault looks like a network that is forgotten at every boot. The reason, and what it costs, is in
[Why settings.modify.system is granted](../../kdos/03-architecture/security-model.md#why-settingsmodifysystem-is-granted).

### Who may change the network

There is no password prompt for network changes on the desktop. Instead, the polkit rule
`/etc/polkit-1/rules.d/50-kdos.rules`, from `fs/`, answers yes to these NetworkManager actions for
members of `wheel`, and leaves every other action to polkit's default:

<!-- kfs:include fs/etc/polkit-1/rules.d/50-kdos.rules lines=60-86 pin=96b22754754a -->

Setting the host name, setting machine-wide DNS and reloading NetworkManager's configuration are
left out on purpose, and need `sudo`. The full list, and why a prompt cannot work on this desktop,
is in
[What is granted instead: 50-kdos.rules](../../kdos/03-architecture/security-model.md#what-is-granted-instead-50-kdosrules).

### D-Bus activation

There is no `/usr/share/dbus-1/system-services/org.freedesktop.NetworkManager.service`.
NetworkManager's build installs none when systemd is off, and the image adds none, so the system
bus never starts NetworkManager; only `42_networkmanager.sh` does. A call to
`org.freedesktop.NetworkManager` while the name has no owner fails at once with
*org.freedesktop.DBus.Error.ServiceUnknown*, and `kdos-netagent`, the network surface and `nmcli`
report NetworkManager as not running. With such a file, the call `kdos-netagent` makes whenever the
name has no owner would start a second NetworkManager outside the supervisor: at every login on a
machine where the service is disabled, and in the gap of every restart, after which the supervised
one would exit with *NetworkManager is already running* every five seconds. The same holds for
ModemManager and polkit, whose ports delete the activation file their builds install; see
[How polkit and ModemManager are started](../../kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).

The services NetworkManager itself starts on the bus, its dispatcher
(`org.freedesktop.nm_dispatcher`) and its privileged helper (`org.freedesktop.nm_priv_helper`),
keep their activation files: nothing supervises them, and the bus is what starts them.

### The menu entry

<!-- kfs:include ports/core/network/networkmanager/build.sh lines=71-84 pin=c3852013b69a -->

The port's `build.sh` adds this entry, so `nmtui` appears in the menu as *Network* and opens in a
terminal.

## Usage

### Checking that it runs

1. Ask the supervisor:

   ```bash
   service status networkmanager
   ```

2. Ask the daemon for its overall state and its devices:

   ```bash
   nmcli general status
   nmcli device status
   ```

   A device listed as `unmanaged` is one NetworkManager has been told to leave alone, such as the
   card of an [access point](access-point.md) run by `hostapd`; the kernel's `dummy0` is listed
   so on every machine. `virbr0`, the bridge of libvirt's default network, and `lo` are
   `connected (externally)`: NetworkManager reports them and leaves their configuration to
   whoever made them. The service's own output is in `/run/kdos-init.networkmanager.log`, and
   the daemon's in the system log.

### Joining a wireless network from the desktop

1. Open `kdos-net` with `Super+F4`, or click the network readout on the panel.
2. Select the network and press `Enter`.
3. Type the passphrase and press `Enter`. It is written into the new profile.

The window's bottom row holds buttons for the commonest actions (*Connect*, *Forget*, *Rescan*,
*Wi-Fi Off*, *Close*), and each action has a key as well: `f` forgets the selected network, `r`
rescans, `a` turns the Wi-Fi radio off and on, `c` copies the selected network's name to the
clipboard, and `h` starts or stops a [hotspot](access-point.md), offered only for a radio that can
be an access point. Every later
question for a secret, such as a changed passphrase, comes from `kdos-netagent` in a dialog of
its own; see [kdos-netagent](../../kdos/04-programs/kdos-shell.md#kdos-netagent).

### Joining a wireless network from a terminal

1. List the networks in range:

   ```bash
   nmcli device wifi list
   ```

2. Join one. `--ask` makes `nmcli` ask for the passphrase on the terminal itself, so this works on
   a text console where no `kdos-netagent` is running:

   ```bash
   nmcli --ask device wifi connect "<SSID>"
   ```

   Add `hidden yes` for a network that does not broadcast its name, and `name <profile>` to choose
   the profile's name. Do not add `private yes`: that writes an owner into the profile, which on
   KDOS makes it a profile NetworkManager never connects on its own.

`nmcli device wifi connect` creates profiles for open, WEP and WPA-PSK networks only. An 802.1X
(enterprise) network needs a profile made with `nmcli connection add` or with `nmtui`'s *Edit a
connection*; the settings it takes are in `nm-settings-nmcli(5)`.

### A wired connection

NetworkManager brings up an Ethernet cable on its own with DHCP: for a managed Ethernet device that
has no profile, it creates one in memory, *Wired connection 1*. To give the machine a static
address instead, save that profile with the address in it:

1. Find the profile's name:

   ```bash
   nmcli connection show
   ```

2. Set the address, gateway and DNS server, and bring the profile up again:

   ```bash
   nmcli connection modify "Wired connection 1" ipv4.method manual \
       ipv4.addresses 192.0.2.10/24 ipv4.gateway 192.0.2.1 ipv4.dns 192.0.2.1
   nmcli connection up "Wired connection 1"
   ```

### Saved networks

```bash
nmcli connection show                    # every profile, the active ones first
nmcli device wifi show-password          # the active Wi-Fi network, with its passphrase
nmcli connection delete "<profile>"      # forget one
nmcli radio wifi off                     # the radio; `on` turns it back on
```

### Other connection types

| Connection | Command |
|---|---|
| PPPoE (DSL), carried by `pppd` | `nmcli connection add type pppoe ifname ppp0 pppoe.parent <ethernet> username <user> password <password>` |
| A phone's Bluetooth tethering, after pairing it in `kdos-bt` | `nmcli connection add type bluetooth bt-type panu bluetooth.bdaddr <phone>` |
| A modem, or a phone that offers only dial-up over Bluetooth | See [Mobile broadband](mobile-broadband.md) |
| A VPN | See [VPN](vpn.md) |

`nmcli` accepts both commands and saves the profile.
[Mobile broadband, PPPoE and Bluetooth tethering](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem)
are built and have not been connected on hardware.

### Turning NetworkManager off

On a machine that should use plain DHCP, such as a server:

1. Disable the service, so it does not start at the next boot, and stop it. The marker is the
   whole of it: nothing but `42_networkmanager.sh` starts NetworkManager (see
   [D-Bus activation](#d-bus-activation)), and the configuration reference's entry for
   [the service.disabled markers](../../kdos/06-reference/configuration.md#etcservicedisabledname)
   says what else the marker changes.

   ```bash
   sudo service disable networkmanager
   sudo service stop networkmanager
   ```

   `pgrep -a NetworkManager` then prints nothing.

2. Start `dhcpcd`, which `30_network` starts once the marker exists:

   ```bash
   sudo service start network
   ```

3. Reboot. A stopped NetworkManager leaves its address, its default route, its `dnsmasq` and its
   `resolvconf` entry in place, so until the next boot `ip route` shows two `default` routes, one
   NetworkManager's and one `dhcpcd`'s.

To bring NetworkManager back, run `sudo service enable networkmanager` and reboot.

### On the live medium and on an installed system

The live medium starts NetworkManager like an installed system does, and saves profiles the same
way, but its `/etc` is in the overlay's upper layer: without a [persistence]{.idx term="persistence store"} store a network you
joined is forgotten at reboot (see
[The live medium and persistence](../../kdos/03-architecture/boot-and-init.md#the-live-medium-and-persistence)).
The installer copies the running live tree to the disk, and leaves out neither
[`/etc/NetworkManager`]{.idx} nor `/etc/resolv.conf`, so a network joined in the live session before
installing is joined on the installed system too.

## Tips

- `nmcli general permissions` lists each action NetworkManager may be asked for and whether your
  account may ask. On KDOS an answer other than `yes` is a refusal: no authentication agent is
  running to turn `auth` into a prompt. For a member of `wheel` the actions the polkit rule leaves
  out answer `auth` (`reload`, `settings.modify.global-dns`, `settings.modify.hostname`,
  `checkpoint-rollback`) or `no`.
- `nmcli connection down "<profile>"` leaves the device free to join its other saved networks.
  `nmcli device disconnect <interface>` also stops the device from connecting again on its own
  until you next bring a profile up on it.
- `nmcli monitor` prints a line for every change of state as it happens, which shows where a
  connection attempt stops.
- Give a profile a short name with `nmcli connection modify "<profile>" connection.id <name>`;
  every later `nmcli connection` command takes it.
- `kdos why /usr/sbin/NetworkManager` names the package that owns a file and the recipe it was
  built from; a file `fs/` provides, such as the service script, is reported as owned by none.

## Troubleshooting

**`kdos-net` says "NetworkManager is not answering".**
Check `service status networkmanager`, then read `/run/kdos-init.networkmanager.log`, the
supervisor's `/run/kdos-svc.NetworkManager.log`, and the lines NetworkManager wrote to the system
log with `sudo grep NetworkManager /var/log/messages`.
Start it with `sudo service start networkmanager`, the command the message names.

**A network you joined does not come back after a reboot.**
On the live medium with no persistence store, that is expected; see
[above](#on-the-live-medium-and-on-an-installed-system). Otherwise the profile names an owner. Check with
`nmcli -f connection.id,connection.permissions connection show "<profile>"`; anything after
`connection.permissions:` is an owner. Clear it with
`sudo nmcli connection modify "<profile>" connection.permissions ""`.

**The machine holds two leases, or `ip route` shows two `default` routes.**
NetworkManager and `dhcpcd` both ran since the last boot: `dhcpcd` was started by hand while
NetworkManager was enabled, or NetworkManager was stopped and `dhcpcd` started without the reboot
that ends [Turning NetworkManager off](#turning-networkmanager-off). `pgrep -a dhcpcd` and
`service status network` show `dhcpcd`, and `service status networkmanager` shows NetworkManager.
Finish turning NetworkManager off in its order, or run `sudo service stop network`, and reboot:
neither program withdraws the route the other left.

**The Wi-Fi list is empty.**
On a machine with no wireless device, `nmcli device wifi list` prints nothing, `nmcli general
status` reports `WIFI-HW missing`, and the other `nmcli device wifi` commands answer *Error: No
Wi-Fi device found.* Run `nmcli radio wifi`; if it says `disabled`, run `nmcli radio wifi on`. `rfkill list` shows a
radio blocked by a switch or a key (`Hard blocked: yes` is the switch). `iw dev` lists the
wireless interfaces the kernel has; with none, the driver or its firmware did not load, and
`sudo dmesg` says which. If the networks are there but no 5 GHz ones are, see
[Wireless regulatory domain](wireless-regulatory-domain.md).

**A connection fails at once with no question asked.**
NetworkManager never prompts for a missing secret; it asks the registered agents and gives up when
none answers. In a desktop session the agent is `kdos-netagent`: `pgrep -a kdos-netagent` should
show it, and it answers requests made as the desktop user. A request made as root, from a root
shell or through `sudo`, reaches no agent and fails with *Error: Connection activation failed: No
valid secrets*. On a text console, or as root, use `nmcli --ask connection up "<profile>"`.

**More detail is needed.**
Raise NetworkManager's log level until the next restart, retry, and read the system log:

```bash
sudo nmcli general logging level DEBUG domains ALL
sudo tail -f /var/log/messages
```

`kdos doctor` checks the wireless regulatory database, and `dbus-daemon-launch-helper`, through
which the system bus starts the root services NetworkManager asks for: `wpa_supplicant`, the
dispatcher and the privileged helper; see [kdos doctor](../../kdos/04-programs/kdos-command.md#kdos-doctor).

## Known gaps

- [A corporate VPN is a terminal program](../../kdos/06-reference/known-gaps.md#a-corporate-vpn-is-a-terminal-program)
- [A USB modem that first appears as a storage device is not switched into a modem](../../kdos/06-reference/known-gaps.md#a-usb-modem-that-first-appears-as-a-storage-device-is-not-switched-into-a-modem)
- [kdos doctor cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  the `dnsmasq` and `glib-networking`/`openconnect` rows

## See also

- [Networking](../../kdos/02-user-guide/administration.md#networking): the user guide's account of
  the same tools
- [kdos-net](../../kdos/04-programs/kdos-shell.md#kdos-net) and
  [kdos-netagent](../../kdos/04-programs/kdos-shell.md#kdos-netagent): the desktop front ends
- [polkit, and why the desktop has no authentication agent](../../kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent)
- [One DHCP client on the link](../../kdos/03-architecture/boot-and-init.md#one-dhcp-client-on-the-link)
- [Services](../../kdos/02-user-guide/administration.md#services): `service` and the boot logs
- [DNS](dns.md), [VPN](vpn.md), [Mobile broadband](mobile-broadband.md),
  [Access point](access-point.md), [Wireless regulatory domain](wireless-regulatory-domain.md)
- [Services](services.md), [Firewall](firewall.md), [Bluetooth](bluetooth.md),
  [sudo, PAM and polkit](sudo-pam-polkit.md), [Logs](logs.md), [Live persistence](live-persistence.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 387.* Previous: [386. Signing keys and trust](signing-keys-and-trust.md) · [Contents](../README.md) · Next: [388. DNS](dns.md)
