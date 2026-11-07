## About
[KDE Connect]{.idx} pairs this machine with a phone that runs the KDE Connect app, over the LAN or
Bluetooth: it shares files and the clipboard, shows the phone's notifications, controls media
players either way, and reads and sends text messages. The port installs the daemon and its
plugins, the `kdeconnect-app` and `kdeconnect-sms` windows, `kdeconnect-indicator` for the panel
tray, and the `kdeconnect-cli` command. Browsing the phone's files mounts them with
[sshfs](#port-sshfs).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_INSTALL_LIBEXECDIR=lib` — point the libexec directory at `/usr/lib`. This release
  installs nothing there: `kdeconnectd`, the windows and the command all go to `/usr/bin`, so the
  switch changes nothing in the package.
- `-D WITH_X11=OFF` — leave out the X11 path. Its mousepad plugin needs libfakekey, which is not a
  port, so the plugin is built for Wayland alone, on [libei](#port-libei). That path asks the
  RemoteDesktop portal for an input connection, which
  [xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde) answers in the Plasma session with an EIS
  connection to KWin.
- `-D WITH_PULSEAUDIO=ON` — build the plugins that reach the sound server through
  [pulseaudio-qt](#port-pulseaudio-qt) and PipeWire's PulseAudio service: `pausemusic`, which
  pauses or mutes playback during a phone call, `findthisdevice`, which lets the phone make the
  machine play an alarm sound, and `systemvolume`, which lets the phone set the machine's volume.
  It is upstream's default, named.
- `-D BLUETOOTH_ENABLED=ON` — build the Bluetooth link provider on QtBluetooth from
  [qt6-qtconnectivity](#port-qt6-qtconnectivity), which reaches `bluetoothd` over D-Bus. The LAN
  provider is built either way. It is upstream's default, named.
- `-D LOOPBACK_ENABLED=OFF` — no loopback link provider, which connects the daemon to itself.
  It is upstream's default.
- `-D INSTALL_UFW_APPLICATION_RULE=OFF` — install no application rule for the ufw firewall into
  `/etc/ufw/applications.d`; ufw is not a port.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6ModemManagerQt=ON` — stop the configure if
  [modemmanager-qt](#port-modemmanager-qt) is missing. Upstream builds the ModemManager telephony
  plugin only when it finds the library, and otherwise drops it without an error.

## Notes
`build.sh` renders upstream's SVG logo, `icons/app/sc-apps-kdeconnect.svg`, to PNG icons at 48,
64, 128 and 256 pixels with `rsvg-convert` from [librsvg](#port-librsvg), so the `kdeconnect`
icon resolves in hicolor at those sizes whatever icon theme is chosen.

The three desktop entries are KDOS's own, written over upstream's. KAboutData names each window's
Wayland app id from the reversed organisation domain and the component name, and each entry's
`StartupWMClass` repeats it, so the task manager can match the window to its entry. The
indicator's entry carries upstream's `NotShowIn=KDE;`: under Plasma the KDE Connect plasmoid is
the tray icon, and the indicator would add a second one.

The virtual-monitor plugin, which lends the phone or tablet to this machine as an extra screen,
runs `krdpserver` from [krdp](#port-krdp) when it is asked to. The build does not check for it,
so krdp is in `depends` to put the server on every machine that has KDE Connect.

The digitizer plugin, which turns a tablet's pen into a drawing tablet here, installs
`40-kdeconnect-uinput.rules`, tagging `/dev/uinput` `uaccess` so the user of the active session
may create the virtual pen device. It goes into the `udevdir` that `udev.pc` from
[eudev](#port-eudev) names, `/lib/udev`; without `udev.pc` the configure falls back to
`/usr/lib/udev`, which is why eudev is in `depends`. The package carries the rule as
`lib/udev/rules.d/`, which is `/usr/lib/udev/rules.d` on the merged-`/usr` root.
