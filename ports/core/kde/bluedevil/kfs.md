## About

BlueDevil is Bluetooth for Plasma. A module of the KDE daemon, `kded6`, registers the pairing
agent with `bluetoothd`, restores each adapter's power state at login and answers incoming file
pushes; the system tray applet lists and connects devices; the Bluetooth page of System Settings
(`kcm_bluetooth`) pairs, trusts and forgets them; and the pairing wizard, the file sender
(`bluedevil-sendfile`, the Share menu's Bluetooth target) and the `bluetooth:/` and `obexftp:/`
KIO workers complete the set. Everything goes through BlueZ's D-Bus API by way of
[bluez-qt](#port-bluez-qt).

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional and skips without a word.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Kirigami=ON` — Upstream looks for Kirigami only as a
  run-time dependency, and a build without it succeeds; the settings page and the applet are
  Kirigami QML and show nothing without it. Required, its absence stops the configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

[bluez](#port-bluez), [kded](#port-kded), [kirigami](#port-kirigami) and
[kdeclarative](#port-kdeclarative) are in `depends` for run time; the applet's device entry copies
an address to the clipboard through kdeclarative's `org.kde.kquickcontrolsaddons` QML module.
The daemon is `bluetoothd` from bluez; file transfer is `obexd`, also from bluez, which the
session bus starts on the first OBEX call through the `Exec=` line of `org.bluez.obex.service`.
The kded module is a plugin that `kded6` loads at login, so without
kded nothing pairs and no adapter is powered on.

BlueZ's D-Bus policy lets any local user drive the adapter, so no polkit rule is involved in
pairing or connecting.

The install puts `bluedevil-mime.xml` under `/usr/share/mime/packages` and asks
`update-mime-database` to run only when no `DESTDIR` is set; kpkg rebuilds the MIME database when
the package is installed (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)).
