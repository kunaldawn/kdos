## About

plasma-nm is Plasma's front end to [NetworkManager](#port-networkmanager): the network applet in the
panel, the connection editor, the Wi-Fi, Ethernet, mobile broadband and VPN pages of System
Settings, and the kded module that asks for secrets and reports connection changes. It talks to
NetworkManager through [networkmanager-qt](#port-networkmanager-qt) and to ModemManager through
[modemmanager-qt](#port-modemmanager-qt), and keeps the passwords it is asked to remember in the
Secret Service through [qtkeychain](#port-qtkeychain).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_OPENCONNECT=OFF` — No OpenConnect VPN editor. The plugin links
  [openconnect](#port-openconnect) and Qt WebEngine for its sign-in page, but it only prepares
  connections for NetworkManager's OpenConnect service plugin, which is not a port, so every
  connection it saved would fail to start. Upstream turns it on whenever `openconnect` is found.

## Notes

The mobile broadband wizard reads the provider database of
[mobile-broadband-provider-info](#port-mobile-broadband-provider-info), whose path the configure
takes from that package's pkg-config file. The lookup is optional upstream and has no switch: when
the file is missing the path is compiled in empty and the wizard lists no providers. The `grep`
after the configure stops the build unless the cache records the module as found.

The other VPN editors (OpenVPN, L2TP, PPTP, SSTP, strongSwan, Libreswan, vpnc, Fortinet SSL,
iodine, SSH) are always built. Each only writes a connection for its NetworkManager plugin, and of
those plugins only [networkmanager-openvpn](#port-networkmanager-openvpn) is a port. WireGuard is
part of NetworkManager itself, and its editor is part of the connection editor rather than a
plugin.

[kirigami](#port-kirigami), [kirigami-addons](#port-kirigami-addons),
[kquickcharts](#port-kquickcharts), [kdeclarative](#port-kdeclarative) and
[prison](#port-prison) are QML modules the applet and the settings pages load at run time; the
configure checks only for `org.kde.prison` and `org.kde.kirigamiaddons.formcard`. Changing a
system-wide connection is authorised by polkit (see
[Why `settings.modify.system` is granted](../../../../docs/kdos/03-architecture/security-model.md#why-settingsmodifysystem-is-granted)).
