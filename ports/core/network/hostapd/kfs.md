## About
hostapd turns a wireless card into an access point, and carries an EAP and RADIUS server for
802.1X networks. On KDOS it makes a machine the access point of a network of its own, one that
stays up across reboots, with [dnsmasq](#port-dnsmasq) beside it for addresses and names;
`kdos-net`'s hotspot, through NetworkManager, is the quicker way to share a connection. The package installs
`hostapd`, `hostapd_cli`, their manual pages and `/etc/hostapd/hostapd.conf.example`.
`/etc/init.d/46_hostapd.sh`, which the base filesystem ships rather than this package, starts the
daemon only once `/etc/hostapd/hostapd.conf` exists; setting one up is
[An access point with hostapd](../../../../docs/kdos/02-user-guide/administration.md#an-access-point-with-hostapd).

## Flags
- `CONFIG_DRIVER_NL80211=y` — Drive the card through nl80211 with [libnl](#port-libnl) 3
  (`CONFIG_LIBNL32=y`); `CONFIG_DRIVER_WIRED=y` adds an 802.1X authenticator for wired ports.
- `CONFIG_ACS=y` — Automatic channel selection: with `channel=acs_survey` hostapd surveys the
  channels and picks the least busy one.
- `CONFIG_IEEE80211AC=y` — Wi-Fi 5 access point modes; `CONFIG_IEEE80211AX=y` and
  `CONFIG_IEEE80211BE=y` add Wi-Fi 6 and 7, and `CONFIG_IEEE80211R=y` fast roaming. The other
  802.11 features on the list are the same kind of switch: `CONFIG_WPS=y`, `CONFIG_DPP=y` with
  `CONFIG_DPP2=y` (Easy Connect), `CONFIG_RSN_PREAUTH=y`, `CONFIG_WNM=y`, `CONFIG_MBO=y`,
  `CONFIG_OCV=y` and `CONFIG_PASN=y`.
- `CONFIG_SAE=y` — WPA3-Personal. `CONFIG_SAE_PK=y` adds SAE-PK, with which a client checks the
  access point's public key, and `CONFIG_OWE=y` adds Opportunistic Wireless Encryption for open
  networks.
- `CONFIG_FULL_DYNAMIC_VLAN=y` — Let hostapd create the VLAN and bridge interfaces a per-station
  VLAN assignment needs, and with `CONFIG_VLAN_NETLINK=y` do it through netlink instead of
  `ioctl()`.
- `CONFIG_EAP=y` — The integrated EAP server, with each method its own switch (MD5, TLS, PEAP,
  MSCHAPv2, GTC, TTLS, PWD, FAST, TEAP, SIM, AKA, AKA'), `CONFIG_ERP=y` for EAP
  re-authentication and `CONFIG_PKCS12=y` for `.p12` keys.
- `CONFIG_RADIUS_SERVER=y` — Offer the EAP server to other access points over RADIUS;
  `CONFIG_IPV6=y` lets RADIUS run over IPv6. Upstream's sample configuration leaves the server
  out.
- `CONFIG_SQLITE=y` — Let `eap_user_file` and the EAP-SIM database be SQLite databases, through
  [sqlite](#port-sqlite).
- `CONFIG_TLS=openssl` — Use [OpenSSL](#port-openssl) for TLS and cryptography.
- `CONFIG_INTERNAL_LIBTOMMATH=y` — Selects hostapd's own minimal LibTomMath for big-number
  arithmetic. The makefile reads it only when its internal crypto is chosen, so with
  `CONFIG_TLS=openssl` it has no effect.
- `CONFIG_GETRANDOM=y` — Seed hostapd's random pool with `getrandom(2)` instead of reading
  `/dev/random`; musl provides the call.
- `CONFIG_ELOOP=eloop` — hostapd's own event loop, on `epoll` (`CONFIG_ELOOP_EPOLL=y`).
- `CONFIG_DEBUG_SYSLOG=y` — Allow `-s` to send the log to syslog, which is how the init script
  runs it, supervised in the foreground with no terminal to log to.
- `CONFIG_WPA_CLI_EDIT=y` — Line editing and history in `hostapd_cli`, through hostapd's own
  small line editor; without it `hostapd_cli` reads plain lines.

## Notes
`build.sh` writes the whole `.config`, so upstream's `defconfig` does not apply and anything not
on the list is left out. The package ships the sample configuration under the name
`hostapd.conf.example`, never `hostapd.conf`, because the init script treats the presence of
`hostapd.conf` as the administrator's decision to run an access point.

NetworkManager must be told to leave the card alone, or it and hostapd both drive it; the
administration page gives the `unmanaged-devices` line.
