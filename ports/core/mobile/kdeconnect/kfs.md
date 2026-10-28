## About
[KDE Connect]{.idx} pairs this machine with a phone that runs the KDE Connect app, over the LAN or
Bluetooth: it shares files and the clipboard, shows the phone's notifications, controls media
players either way, and reads and sends text messages. The port installs the daemon and its
plugins, the `kdeconnect-app` and `kdeconnect-sms` windows, `kdeconnect-indicator` for the panel
tray, and the `kdeconnect-cli` command. Browsing the phone's files mounts them with
[sshfs](#port-sshfs).

## Flags
- `-D CMAKE_INSTALL_LIBEXECDIR=lib` — point the libexec directory at `/usr/lib`. This release
  installs nothing there: `kdeconnectd`, the windows and the command all go to `/usr/bin`, so the
  switch changes nothing in the package.
- `-D WITH_X11=OFF` — leave out the X11 path. Its mousepad plugin needs libfakekey, which is not a
  port, so the plugin is built for Wayland alone, on [libei](#port-libei). That path asks the
  RemoteDesktop portal for an input connection, which no backend on KDOS answers, so the phone
  cannot drive the pointer or keyboard; see
  [A phone cannot drive the pointer or keyboard through KDE Connect](../../../../docs/kdos/06-reference/known-gaps.md#a-phone-cannot-drive-the-pointer-or-keyboard-through-kde-connect).
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
64, 128 and 256 pixels with `rsvg-convert` from [librsvg](#port-librsvg), because the panel draws
only PNG icons.

The three desktop entries are KDOS's own, written over upstream's. KAboutData names each window's
Wayland app id from the reversed organisation domain and the component name, and each entry's
`StartupWMClass` repeats it, so the panel can match the window to its entry (see
[`kdos appid`](../../../../docs/kdos/04-programs/kdos-command.md#kdos-appid)).
