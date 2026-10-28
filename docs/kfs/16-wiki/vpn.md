# VPN

This page covers the three kinds of [VPN]{.idx} a KDOS machine can join: [OpenVPN]{.idx} and [WireGuard]{.idx},
which NetworkManager manages and [`kdos-net`]{.idx} lists, and the corporate VPNs [`openconnect`]{.idx} speaks
(AnyConnect, GlobalProtect, Fortinet, Pulse and others), which run from a terminal. It gives the
files each one reads, how to bring a tunnel up and down, and where its name servers go.

## Installation

Every port here is in the `network` shelf's list of `41_system`, so every image ships them:

| Port | Provides |
|---|---|
| [openvpn](#port-openvpn) | [`openvpn`]{.idx}, the OpenVPN client and server |
| [networkmanager-openvpn](#port-networkmanager-openvpn) | NetworkManager's OpenVPN plugin: `nmcli connection import type openvpn`, and the service that runs `openvpn` for a profile |
| [wireguard-tools](#port-wireguard-tools) | `wg` and [`wg-quick`]{.idx} |
| [openconnect](#port-openconnect) | `openconnect`, the client for AnyConnect, GlobalProtect, Fortinet, Pulse, F5 and Array |
| [vpnc-script](#port-vpnc-script) | The script `openconnect` runs to give a tunnel its routes and name servers |

WireGuard support in NetworkManager is part of the [networkmanager](#port-networkmanager) port
itself and needs no plugin. The kernel side is two modules of the [linux](#port-linux) port:
`wireguard`, loaded when the first WireGuard device is created, and `tun`, the device OpenVPN and
`openconnect` use, which `/etc/modules-load.d/kdos-containers.conf` loads at every boot. No VPN has
a service script. A NetworkManager profile comes up when you start it, `wg-quick` and `openconnect`
run when you run them, and [Starting a tunnel on its own](#starting-a-tunnel-on-its-own) covers
bringing one up without being asked.

## Configuration

### The OpenVPN plugin

<!-- kfs:include ports/core/network/networkmanager-openvpn/postinstall.sh lines=22-34 pin=1e036feb972f -->

The package's `postinstall.sh` creates the `nm-openvpn` account and its chroot directory.
NetworkManager's OpenVPN service runs `openvpn` as that account, inside that directory when it
exists, and refuses every connection when the account is missing. The plugin is built with
`--without-gnome`, so it has no dialog of its own; a password or a one-time code is asked for by
[`kdos-netagent`]{.idx} (see [kdos-netagent](../../kdos/04-programs/kdos-shell.md#kdos-netagent)).

<!-- kfs:include ports/core/network/openvpn/build.sh lines=12-20 pin=4917d9bd938a -->

`--disable-pkcs11` leaves out PKCS#11, so a certificate on a smart card or a hardware token cannot
authenticate an OpenVPN connection. Certificates and keys kept in files, and passwords, are not
affected.

### WireGuard

<!-- kfs:include ports/core/network/wireguard-tools/build.sh lines=12-15 pin=b03f36207363 -->

`wg-quick <name>` reads `/etc/wireguard/<name>.conf`. KDOS ships no such file; the one your VPN
provider gives you goes there, owned by root with mode 600, since it holds the private key. The
format is in `wg-quick(8)` and `wg(8)`. A WireGuard profile made in NetworkManager lives in
`/etc/NetworkManager/system-connections/` like every other profile.

### openconnect and its script

<!-- kfs:include ports/core/network/openconnect/build.sh lines=12-25 pin=8171dfb4e6fc -->

`--with-vpnc-script` compiles in the path of the script `openconnect` runs on every connect and
disconnect. `--without-stoken` leaves out RSA SecurID software tokens (`--token-mode=rsa`), and
`openconnect --help` says so under `--token-secret`; the TOTP, HOTP and YubiKey OATH token modes
are there, and the *Features present* line at the top of the help lists them.
`--with-external-browser` names the program a single sign-on login opens.

[`vpnc-script`]{.idx} picks the first resolver manager it finds. On KDOS `/sbin` is a link to `/usr/sbin`,
so the test below finds openresolv's [`resolvconf`]{.idx}:

<!-- kfs:include ports/core/network/vpnc-script/vpnc-script lines=199-203 pin=e0ce40eebcf8 -->

and the tunnel's servers are handed to it under the tunnel's interface name, and taken back on
disconnect:

<!-- kfs:include ports/core/network/vpnc-script/vpnc-script lines=814-829 pin=4d927636d216 -->

The script also sources every file in `/etc/vpnc/<hook>.d/` (such as `connect.d` and
`disconnect.d`) when that directory exists; none is shipped.

## Usage

### An OpenVPN profile

1. Import the `.ovpn` file your provider gave you. The profile takes the file's name, without
   `.ovpn`:

   ```bash
   nmcli connection import type openvpn file ~/client.ovpn
   ```

2. Bring it up. In `kdos-net` (`Super+F4`) the profile is a row of its own below the radios:
   select it and press `Enter`. From a terminal:

   ```bash
   nmcli --ask connection up client
   ```

   `--ask` asks for a password on the terminal. Without it, run as the desktop user, `nmcli`
   prints *A password is required to connect to 'client'* and the question goes to
   `kdos-netagent`, which opens a dialog headed *NetworkManager*: *client wants its password.*
   Run as root, the request reaches no agent and fails with *No valid secrets*.

3. Take it down with `Enter` on the same row, or `nmcli connection down client`.

### A WireGuard tunnel through NetworkManager

1. Name the configuration file after the interface the tunnel should have, followed by `.conf`,
   and import it. NetworkManager refuses a file whose name is not a valid interface name (one of
   more than fifteen characters, for example) with *The name of the WireGuard config must be a
   valid interface name followed by ".conf".*:

   ```bash
   nmcli connection import type wireguard file ~/wg0.conf
   ```

   The import saves the profile with `connection.autoconnect yes`, and the tunnel comes up at
   once and at every later start of NetworkManager. To bring it up only when asked, turn that off:
   `nmcli connection modify wg0 connection.autoconnect no`.

2. Bring it up and down: `Enter` on its row in `kdos-net`, or `nmcli connection up wg0` and
   `nmcli connection down wg0`.

The `DNS=` servers of the file go to NetworkManager's local resolver. When the file names no
search domain, the import adds the wildcard domain `~` to the profile, so the resolver asks the
tunnel's servers about every name; with domains named, it asks them about those. See [DNS](dns.md).

### A WireGuard tunnel with `wg-quick`

1. Put the file at [`/etc/wireguard/wg0.conf`]{.idx}, root's and mode 600:

   ```bash
   sudo install -m 600 ~/wg0.conf /etc/wireguard/wg0.conf
   ```

2. Bring the tunnel up, look at it, and take it down:

   ```bash
   sudo wg-quick up wg0
   sudo wg show
   sudo wg-quick down wg0
   ```

A `DNS=` line is handed to `resolvconf` as the tunnel's only servers (`resolvconf -a wg0 -m 0
-x`), so while the tunnel is up `/etc/resolv.conf` names them alone and every name is asked of
them; `wg-quick down` removes them, and the file names the servers it named before.
`nmcli device status` lists the tunnel as `connected (externally)`, and `kdos-net` shows it as a *wireguard* row: NetworkManager reports it and does
not manage it, so take it down with `wg-quick down`.

### A corporate VPN with `openconnect`

1. Connect, naming the protocol unless it is AnyConnect, the default:

   ```bash
   sudo openconnect --protocol=gp vpn.example.com
   ```

   The protocol names are `anyconnect`, `nc`, `pulse`, `gp` (GlobalProtect), `f5`, `fortinet`
   and `array`. `openconnect` asks for the user name, the password and any code on the terminal.

2. Leave it running for as long as the tunnel should stay up; `Ctrl+C` disconnects, and
   `vpnc-script` removes the routes and the servers it added.

`openconnect` runs as root because it creates the tunnel device itself.

### Starting a tunnel on its own

A WireGuard profile in NetworkManager is an ordinary connection, and an imported one comes up
whenever NetworkManager starts, because the import sets `connection.autoconnect yes`; see
[A WireGuard tunnel through NetworkManager](#a-wireguard-tunnel-through-networkmanager).

NetworkManager does not bring up a VPN profile such as an OpenVPN one on its own, although the
import also saves it with `connection.autoconnect yes`. Instead, name it as a
secondary of the connection it should ride on, and it comes up each time that connection does:

```bash
nmcli connection modify "<Wi-Fi or wired profile>" +connection.secondaries "<OpenVPN profile>"
```

`nmcli` splits the value at spaces and commas and turns each name into the profile's UUID, so a
VPN profile whose name has a space in it is given by its UUID, from `nmcli connection show`. Only a
VPN profile can be a secondary; `nmcli` refuses a WireGuard one.

### Opening a port for a WireGuard server

A machine that other WireGuard peers connect to must accept their UDP port, which none of the
firewall's names opens. Put a rule for the port the configuration's `ListenPort =` names in a
`.nft` file of your own under `/etc/nftables.d`, such as `/etc/nftables.d/60-wireguard.nft` for
port 51820:

```text
table inet filter {
    chain input {
        udp dport 51820 accept
    }
}
```

Check it with `sudo nft -c -f /etc/nftables.conf` and apply it with `sudo service start nftables`,
as [Adding a rule of your own](firewall.md#adding-a-rule-of-your-own) describes. A client needs no
rule: replies to a tunnel this machine opened are accepted.

### On the live medium and on an installed system

The two behave the same. On the live medium an imported profile and a file under
[`/etc/wireguard`]{.idx} last until the next reboot unless the medium has a
[persistence store](live-persistence.md), and the installer copies both to the disk with the rest
of the live tree.

## Tips

- `nmcli connection show --active` lists the tunnels that are up, with their device names.
- `sudo wg show` reports each WireGuard peer's last handshake and the bytes sent each way, which
  is the first thing to look at when a tunnel is up and nothing passes.
- `openconnect -b` continues in the background after it has started, which frees the terminal.

## Troubleshooting

**An OpenVPN profile fails as soon as it starts.**
`grep '^nm-openvpn:' /etc/passwd` should print the account. If it does not, create it as the
package's `postinstall.sh` does, shown under [The OpenVPN plugin](#the-openvpn-plugin):

```bash
sudo groupadd -r nm-openvpn
sudo useradd -r -g nm-openvpn -d /var/lib/openvpn/chroot -s /sbin/nologin nm-openvpn
```

`openvpn` itself logs to the system log under the name `nm-openvpn`:
`sudo grep nm-openvpn /var/log/messages`.

**A VPN profile fails without asking for a password.**
The question went to a secret agent and none answered. In a session, `pgrep -a kdos-netagent`
should show the agent; from a terminal, use `nmcli --ask connection up <profile>`.

**`nmcli connection import` says "failed to load VPN plugin".**
The whole message names the type, as in *Error: failed to load VPN plugin: unknown VPN plugin
"org.freedesktop.NetworkManager.vpnc".* `openvpn` is the only VPN type with a NetworkManager
plugin here (its file is `/usr/lib/NetworkManager/VPN/nm-openvpn-service.name`); `wireguard`
needs none. There is no plugin for openconnect or vpnc: use `openconnect` from a terminal.

**The tunnel is up and names do not resolve.**
`resolvconf -l` shows whether the tunnel's servers reached `resolvconf`. For a NetworkManager
profile, `nmcli -f IP4.DNS,IP4.DOMAIN device show <tunnel>` shows what the local resolver was
given. See [DNS](dns.md).

**`wg-quick up` prints `signature mismatch: /etc/resolv.conf` and takes the tunnel down.**
`/etc/resolv.conf` does not start with `# Generated by resolvconf`, so `resolvconf` refused the
tunnel's servers and `wg-quick` undid everything it had done. `sudo resolvconf -u` rewrites the
file with the signature; see [DNS](dns.md#etcresolvconf). The refused entry stays listed in
`resolvconf -i` until `sudo resolvconf -d <tunnel>` removes it.

**`wg-quick up` stops at its `ip link add` line.**
The kernel could not create the WireGuard device, which usually means the `wireguard` module did
not load. `sudo modprobe wireguard` loads it, or prints the reason it cannot.

## Known gaps

- [A corporate VPN is a terminal program](../../kdos/06-reference/known-gaps.md#a-corporate-vpn-is-a-terminal-program)
- [Features whose library has no port](../../kdos/06-reference/known-gaps.md#features-whose-library-has-no-port):
  the `glib-networking`/`openconnect` row (no PAC proxy configuration)

## See also

- [Networking](../../kdos/02-user-guide/administration.md#networking): VPNs, and DNS split by
  connection
- [kdos-net](../../kdos/04-programs/kdos-shell.md#kdos-net): the rows for VPN and WireGuard profiles
- [The firewall](../../kdos/02-user-guide/administration.md#the-firewall)
- [NetworkManager](networkmanager.md), [DNS](dns.md)
- [Firewall](firewall.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 389.* Previous: [388. DNS](dns.md) · [Contents](../README.md) · Next: [390. Mobile broadband](mobile-broadband.md)
