## About
NetworkManager is the daemon that brings links up and keeps them up: Ethernet, Wi-Fi through
`wpa_supplicant`, mobile broadband through ModemManager, PPPoE and serial modems through `pppd`,
Bluetooth tethering, VPNs and a shared hotspot. It also installs `nmcli`, `nmtui` and `libnm`, the
client library that [networkmanager-qt](#port-networkmanager-qt) and the OpenVPN plugin
[networkmanager-openvpn](#port-networkmanager-openvpn) build on. KDOS's own front ends,
`kdos-net` and `kdos-netagent` in [kdos-shell](#port-kdos-shell), do not link `libnm`: they speak
to the daemon over the system bus. `/etc/init.d/42_networkmanager.sh` starts it under the
supervisor with `--no-daemon`, after polkit and ModemManager.

What sets the KDOS build apart is what is missing around it. There is no systemd and no logind,
so the build names no unit directory, no journal and no session tracker, and NetworkManager runs
with no idea which users are logged in. The consequences of that for who may change a connection
are in [Why `settings.modify.system` is granted](../../../../docs/kdos/03-architecture/security-model.md#why-settingsmodifysystem-is-granted).
Every helper program it runs (`dnsmasq`, `pppd`, `dhcpcd`, `nft`, `resolvconf`, `modprobe`) is
named by absolute path at configure time, so the paths compiled into the binary are the ones the
image installs.

`build.sh` adds two files of its own. `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf` sets
`dns=dnsmasq`, so NetworkManager starts a caching [dnsmasq](#port-dnsmasq) on `127.0.0.1` and
`::1` and feeds it each connection's servers ([Networking](../../../../docs/kdos/02-user-guide/administration.md#networking)
describes the result). `nmtui.desktop` gives `nmtui` a menu entry as *Network*, run in a terminal.

## Flags
- `--libexecdir=/usr/lib` — Put the helper programs NetworkManager runs itself (`nm-dhcp-helper`,
  `nm-dispatcher`, `nm-daemon-helper` and the rest) in `/usr/lib` rather than `/usr/libexec`.
- `-Dintrospection=false` — Build no GObject introspection data for `libnm`, so no language binding
  can load it through GObject introspection. The Vala bindings and the API documentation are made
  from that data, so they cannot be built either.
- `-Ddocs=false` — Build no API reference for `libnm`. Upstream refuses `-Ddocs=true` without
  introspection.
- `-Dman=true` — Install the manual pages: `NetworkManager(8)`, `NetworkManager.conf(5)`,
  `nmcli(1)`, `nmtui(1)` and the settings references. The release tarball carries them already
  built.
- `-Dvapi=false` — Build no Vala bindings. They are generated from the introspection data, which
  this build does not make.
- `-Dtests=no` — Build none of upstream's test programs.
- `-Dsystemd_journal=false` — Log without `libsystemd`'s journal API. KDOS has no systemd
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)),
  and upstream's default needs it.
- `-Dsystemdsystemunitdir=no`, `-Dsystemdsystemgeneratordir=no` — Install no systemd unit files
  and no generator. Left empty, each is looked up through systemd's pkg-config file and the
  configure step stops when that is missing.
- `-Dsession_tracking=no` — Build no logind session tracker (upstream's choices are `systemd`,
  `elogind` and `no`). Neither logind exists here, so NetworkManager treats every user as having
  no session. A profile that names an owner in its `permissions` list is then invisible and never
  connects on its own, which is why `kdos-net` creates every profile as a system connection; see
  [Why `settings.modify.system` is granted](../../../../docs/kdos/03-architecture/security-model.md#why-settingsmodifysystem-is-granted).
- `-Dsession_tracking_consolekit=false` — Build no ConsoleKit session tracker either. ConsoleKit
  is not in the tree, and with it on NetworkManager would watch ConsoleKit's database for sessions
  that never appear.
- `-Dpolkit=true` — Ask polkit before every privileged request a client makes. The actions
  `wheel` is granted, and those it is not, are listed in
  [polkit, and why the desktop has no authentication agent](../../../../docs/kdos/03-architecture/security-model.md#polkit-and-why-the-desktop-has-no-authentication-agent).
- `-Dpolkit_agent_helper_1=/usr/lib/polkit-1/polkit-agent-helper-1` — The setuid helper `nmcli`
  runs when it acts as a polkit authentication agent itself (`nmcli agent polkit`). The path is
  where the [polkit](#port-polkit) port installs it, compiled in rather than searched for.
- `-Dsuspend_resume=consolekit` — Choose which service's suspend signal NetworkManager listens
  for: `systemd` and `elogind` need their libraries at build time, and `consolekit` needs only the
  D-Bus name `org.freedesktop.ConsoleKit`, which nothing on KDOS owns. It is the only backend that
  builds without those libraries, and it hears nothing. Suspend reaches NetworkManager instead from `kdos-powerd`,
  which calls its `Sleep(true)` before the kernel suspends and `Sleep(false)` after; see
  [What happens around a suspend](../../../../docs/kdos/04-programs/daemons.md#what-happens-around-a-suspend).
- `-Dmodem_manager=true` — Drive mobile broadband modems through
  [modemmanager](#port-modemmanager)'s D-Bus interface, with
  [mobile-broadband-provider-info](#port-mobile-broadband-provider-info) for a carrier's APN.
- `-Dofono=false` — Build no oFono modem support, which upstream marks experimental. oFono is not
  in the tree.
- `-Dbluez5_dun=true` — Support Bluetooth dial-up networking through a phone, built against
  [bluez](#port-bluez)'s headers.
- `-Dteamdctl=false` — Build no support for team devices. It needs `libteamdctl`, which is not in
  the tree.
- `-Dovs=false` — Build no Open vSwitch support. Open vSwitch is not in the tree.
- `-Dclat=true` — Build CLAT (464XLAT), which lets an IPv4-only program reach the IPv4 internet
  over an IPv6-only network through that network's NAT64 gateway. It is a BPF program loaded into
  the kernel, built with [libbpf](#port-libbpf) and [bpftool](#port-bpftool), and is off in every
  connection until its `ipv4.clat` property, or an `ipv4.clat` default in `NetworkManager.conf`,
  turns it on.
- `-Dbpf-compiler=clang` — Compile that BPF program with `clang -target bpf`. Upstream's other
  choice is a `bpf-gcc` cross compiler, which is not in the tree; [clang](#port-clang) is a
  dependency of this port for this one program.
- `-Dppp=true`, `-Dpppd=/usr/sbin/pppd` — Build the PPP plugin, which carries PPPoE and serial
  modem connections, and name where [ppp](#port-ppp) installs `pppd`.
- `-Dconcheck=true` — Build connectivity checking, which fetches a known URL over
  [curl](#port-curl) to tell a working link from a captive portal. It runs only when a
  `[connectivity]` section in `NetworkManager.conf` names a `uri`, and KDOS ships no such section.
- `-Dnm_cloud_setup=false` — Build no `nm-cloud-setup`, the tool that configures networking from
  a cloud provider's metadata service.
- `-Dfirewalld_zone=false` — Install no firewalld zone for shared connections. firewalld is not in
  the tree; the firewall is nftables, and `/etc/nftables.conf` opens what a hotspot needs.
- `-Difupdown=false` — Build no plugin that reads Debian's `/etc/network/interfaces`. Left at
  `auto`, upstream turns it on when `/etc/debian_version` exists on the build machine.
- `-Dnmcli=true`, `-Dnmtui=true` — Build the command line client and the text interface. `nmtui`
  draws with [newt](#port-newt).
- `-Dreadline=libreadline` — Link `nmcli`'s interactive editor with GNU [readline](#port-readline),
  and stop the configure step if it is missing, rather than falling back to libedit.
- `-Dwifi=true` — Build Wi-Fi support, with [wpa_supplicant](#port-wpa_supplicant) as the
  back end.
- `-Diptables=/usr/sbin/iptables`, `-Dip6tables=/usr/sbin/ip6tables`, `-Dnft=/usr/sbin/nft` —
  The firewall commands NetworkManager runs to install a shared connection's NAT, by the paths
  the image installs them at.
- `-Ddhcpcd=/usr/sbin/dhcpcd` — Build support for [dhcpcd](#port-dhcpcd) as an alternative DHCP
  client (`main.dhcp=dhcpcd`), at this path. The default stays the internal client.
- `-Ddnsmasq=/usr/sbin/dnsmasq` — The `dnsmasq` NetworkManager starts, both as the local caching
  resolver that `10-kdos-dns.conf` asks for and as the DHCP and DNS server of a shared hotspot.
  Left empty and not found at configure time, upstream compiles in no path at all.
- `-Dmodprobe=/usr/sbin/modprobe` — The `modprobe` NetworkManager runs to load a kernel module
  before it needs one, such as `ppp_generic` or the netfilter modules a shared connection uses.
- `-Dresolvconf=/usr/sbin/resolvconf` — Write `/etc/resolv.conf` through
  [openresolv](#port-openresolv)'s `resolvconf`, the one writer of that file on KDOS, so
  NetworkManager, `dhcpcd` and `wg-quick` never overwrite each other's entries.
- `-Dnetconfig=no` — Build no support for SUSE's `netconfig`, the other resolv.conf manager
  upstream knows.
- `-Diwd=false` — Build no iwd Wi-Fi back end. iwd is not in the tree.
- `-Dlibpsl=true` — Link [libpsl](#port-libpsl), the public suffix list. When the host name is a
  full domain name, NetworkManager adds its domain part to the DNS search list, and libpsl keeps it
  from adding a public suffix such as `co.uk` there; without it every domain counts as private.
- `-Dnbft=false` — Build no NBFT support, which reads the firmware's table for an NVMe-over-TCP
  boot disk and configures its network. It lives in `nm-initrd-generator`, which nothing in the
  KDOS boot path runs, and with it on the configure step needs [libnvme](#port-libnvme).
- `-Dconfig_dhcp_default=internal` — Make NetworkManager's built-in DHCP client the default. A
  second client on the same link would take a second lease, so `/etc/init.d/30_network.sh` keeps `dhcpcd` down
  while NetworkManager runs; see
  [One DHCP client on the link](../../../../docs/kdos/03-architecture/boot-and-init.md#one-dhcp-client-on-the-link).
- `-Dconfig_logging_backend_default=syslog` — Log to syslog by default, where KDOS's logs are
  kept. It is also what upstream picks when the journal is off.
- `-Dcrypto=gnutls` — Read certificates and private keys (for 802.1X profiles) with
  [gnutls](#port-gnutls) instead of upstream's default, NSS.
- `-Dqt=false` — Build none of upstream's Qt example programs.
- `-Dselinux=false` — Build without SELinux labelling, which needs `libselinux`; KDOS has no
  SELinux.
- `-Dlibaudit=no` — Send no records to the Linux audit daemon, which is not in the tree. Each
  audited operation is still logged by NetworkManager itself, at `info` level in its `AUDIT`
  logging domain.

## Notes
`dhcpcd` and NetworkManager must not both run. `/etc/init.d/30_network.sh` (see
[Services](../../../../docs/kfs/16-wiki/services.md)) stands down when
`/usr/sbin/NetworkManager` is executable and `/etc/service.disabled/networkmanager` is absent, and
the marker alone keeps NetworkManager off. With systemd off the build installs no D-Bus activation
file for `org.freedesktop.NetworkManager`, and the image adds none, so the system bus never starts
NetworkManager: the call `kdos-netagent` makes in every session while the name has no owner is
refused rather than answered by a second, unsupervised NetworkManager; see
[`/etc/service.disabled/<name>`](../../../../docs/kdos/06-reference/configuration.md#etcservicedisabledname).

ModemManager has to be on the bus first. `42_modemmanager.sh` sorts before
`42_networkmanager.sh` and returns once ModemManager holds its name: NetworkManager's WWAN and
Bluetooth DUN devices are ModemManager's modems, and NetworkManager calls the name as it starts.
The call can start nothing, because the modemmanager port deletes the activation file its build
installs; see
[How polkit and ModemManager are started](../../../../docs/kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started).

A file of the same name in `/etc/NetworkManager/conf.d` overrides `10-kdos-dns.conf`, which is
how a machine opts out of the local resolver without editing a packaged file.
