## About

qBittorrent is a BitTorrent client on Qt 6 and [libtorrent-rasterbar](#port-libtorrent-rasterbar),
with a built-in web interface for remote control. The port builds the source twice: once as the
desktop client, `qbittorrent`, and once with no GUI as `qbittorrent-nox`, which runs on a machine
with no display and is driven through its web interface.

## Flags

- `-D GUI=$_gui` — `build.sh` loops over `ON` and `OFF`, configuring into `build-gui-ON` and
  `build-gui-OFF`. With `GUI=ON` the build is the desktop client; with `GUI=OFF` it is
  `qbittorrent-nox`. The whole install is taken from the GUI build, and only the `qbittorrent-nox`
  binary and its manual page are added from the other.
- `-D WEBUI=ON` — the built-in HTTP server for remote control, in both builds. This is upstream's
  default, and it is `qbittorrent-nox`'s only interface.
- `-D STACKTRACE=OFF` — no crash handler that prints a backtrace to the terminal and into a
  dialog. On by default upstream.
- `-D TESTING=OFF` — no internal test suite, upstream's default.
- `-D SYSTEMD=OFF` — do not install the systemd service file for `qbittorrent-nox`; KDOS has no
  systemd (see [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).

## Patches

- `geoip-off-by-default.patch` — In `src/base/preferences.cpp`, changes the default of `Preferences/Connection/ResolvePeerCountries` from `true` to `false`. With it on, qBittorrent downloads DB-IP's free country database from `download.db-ip.com` and fetches it again each month to show each peer's country. With the patch a fresh profile makes no such request; the setting can still be turned on in the preferences.

## Notes

`build.sh` deletes every directory under `/usr/share/man` that is not a `man<N>` section, which
removes upstream's translated manual pages; kpkg's
[size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)
removes the same directories from every package. The recipe writes its own desktop entry: qBittorrent
sets its desktop file name to `org.qbittorrent.qBittorrent`, which is the Wayland app id the
entry's `StartupWMClass` names, where upstream's entry carries `qbittorrent`. The entry claims
`application/x-bittorrent` and `magnet:` links; [Transmission](#port-transmission)'s entry claims
neither.
