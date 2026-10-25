## About
wpa_supplicant authenticates a machine to a wireless network, and to a wired one that uses
802.1X: it runs the WPA/WPA2/WPA3 key handshakes and the EAP methods enterprise networks ask for.
On KDOS it is the Wi-Fi back end of [NetworkManager](#port-networkmanager), which drives it over
D-Bus; the bus starts it on first use (see
[How polkit and ModemManager are started](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#how-polkit-and-modemmanager-are-started)
for what that activation depends on). `wpa_cli` and `wpa_passphrase` are installed beside it.

## Flags
- `CONFIG_DRIVER_NL80211=y` — Talk to wireless drivers through the kernel's nl80211 netlink
  interface, which every current Linux wireless driver serves, using [libnl](#port-libnl) 3
  (`CONFIG_LIBNL32=y`).
- `CONFIG_DRIVER_WIRED=y` — Also run 802.1X on wired Ethernet ports, and with
  `CONFIG_DRIVER_MACSEC_LINUX=y` and `CONFIG_MACSEC=y` set up IEEE 802.1AE MACsec through the
  kernel's `macsec` driver.
- `CONFIG_BACKEND=file` — Keep networks in a text configuration file (`wpa_supplicant.conf`).
  The other back ends are `winreg`, the Windows registry, and `none`.
- `CONFIG_CTRL_IFACE=y` — The UNIX-socket control interface `wpa_cli` uses.
- `CONFIG_CTRL_IFACE_DBUS_NEW=y` — The `fi.w1.wpa_supplicant1` D-Bus interface, which is how
  NetworkManager controls it; `CONFIG_CTRL_IFACE_DBUS_INTRO=y` adds introspection data to it.
- `CONFIG_DEBUG_FILE=y` — Allow `-f` to write the debug log to a file, and with
  `CONFIG_DEBUG_SYSLOG=y` `-s` to send it to syslog.
- `CONFIG_TLS=openssl` — Use [OpenSSL](#port-openssl) for TLS-based EAP methods and for
  cryptography, upstream's default stated outright.
- `CONFIG_EAP_MD5=y` — The EAP methods, each its own switch: MD5, MSCHAPv2, TLS, PEAP, TTLS, GTC,
  OTP, LEAP, SIM, PSK, AKA, AKA', FAST, PWD, TEAP, IKEv2, SAKE, GPSK (with its SHA-256 suite),
  PAX, EKE and TNC, with `CONFIG_IEEE8021X_EAPOL=y` for the EAPOL framing they ride on. AKA, AKA',
  TEAP and EKE are off in upstream's sample configuration.
- `CONFIG_PCSC=y` — Read the SIM or USIM over PC/SC through [pcsc-lite](#port-pcsc-lite), which
  EAP-SIM and EAP-AKA need; `CONFIG_SMARTCARD=y` lets a TLS private key live on a smart card, and
  `CONFIG_PKCS12=y` reads keys and certificates from `.p12`/`.pfx` files.
- `CONFIG_SAE=y` — WPA3-Personal (SAE), with `CONFIG_SAE_PK=y` for SAE public keys,
  `CONFIG_OWE=y` for Opportunistic Wireless Encryption on open networks, and
  `CONFIG_SUITEB192=y` for WPA3-Enterprise 192-bit mode.
- `CONFIG_WEP=y` — Keep support for WEP networks. Upstream builds without it unless asked,
  and calls WEP insecure and due to be removed; with it off, a WEP network cannot be joined at
  all.
- `CONFIG_AP=y` — Let a network block run the card as an access point (`mode=2`);
  `CONFIG_P2P=y` adds Wi-Fi Direct, `CONFIG_MESH=y` 802.11s mesh, `CONFIG_IBSS_RSN=y` encrypted
  ad-hoc networks, and `CONFIG_TDLS=y` direct links between stations.
- `CONFIG_IEEE80211R=y` — Fast roaming between access points of one network. The other 802.11
  amendments on the list are the same kind of switch: `CONFIG_IEEE80211AC=y`,
  `CONFIG_IEEE80211AX=y` and `CONFIG_IEEE80211BE=y` (Wi-Fi 5, 6 and 7 station features),
  `CONFIG_WNM=y`, `CONFIG_MBO=y`, `CONFIG_OCV=y`, `CONFIG_PASN=y`, `CONFIG_FILS=y`,
  `CONFIG_HS20=y` with `CONFIG_INTERWORKING=y` (Hotspot 2.0), `CONFIG_DPP=y` with
  `CONFIG_DPP2=y` (Easy Connect), and `CONFIG_WPS=y` (Wi-Fi Protected Setup).
- `CONFIG_BGSCAN_SIMPLE=y` — The two background-scan modules, `simple` and
  `learn` (`CONFIG_BGSCAN_LEARN=y`), which look for a better access point while connected.
- `CONFIG_WPA_CLI_EDIT=y` — wpa_supplicant's own limited line editing and history in `wpa_cli`,
  in place of GNU Readline (`CONFIG_READLINE`). Upstream's sample configuration notes that
  Readline is GPL-licensed, so the BSD license may not apply to a `wpa_cli` linked with it.
- `CONFIG_ELOOP_POLL=y` — Run the event loop on `poll` instead of `select`, so a descriptor
  numbered past `FD_SETSIZE` can still be watched. Not `epoll`: the D-Bus interface registers its
  bus socket three times, for reading, writing and exceptions, and `epoll` refuses the second
  `EPOLL_CTL_ADD` of one descriptor with `EEXIST`. The interface is then never answered, and
  NetworkManager reports every Wi-Fi device `unavailable`.

## Notes
`build.sh` writes the whole `.config`; nothing from upstream's `defconfig` applies, so a feature
not on the list is not built.

`make` is given `BINDIR=/usr/sbin` because it substitutes that path into the D-Bus service file
it generates, `fi.w1.wpa_supplicant1.service`, whose `Exec=` line the bus runs. Left at the
default it would name `/usr/local/sbin/wpa_supplicant`, and activation would fail. Every file is
installed by hand with `install`; of the files the makefile generates, the systemd units are the
ones left out.
