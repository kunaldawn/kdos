## About

BlueZ is the Linux Bluetooth stack in user space: `bluetoothd`, the system daemon that manages
adapters and devices over D-Bus; `obexd`, the file-transfer and phonebook service on the session
bus; `bluetoothctl` and the other tools; and `libbluetooth`. [pipewire](#port-pipewire)'s
Bluetooth audio, [networkmanager](#port-networkmanager)'s Bluetooth networking and the other ports
that name it talk to the daemon or link the library. `fs/etc/init.d/60_bluetooth.sh` starts
`bluetoothd` at boot.

## Flags

- `--libexecdir=/usr/lib` — Install the daemons under `/usr/lib/bluetooth`. `build.sh` adds
  `/usr/sbin/bluetoothd` as a link to the daemon.
- `--enable-library` — Install `libbluetooth` and its headers, which upstream does not install by
  default and which the ports that link BlueZ need.
- `--disable-systemd` — Install no systemd unit files. KDOS has no systemd
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--enable-obex` — Build `obexd`, the OBEX service for file transfer and phonebook access, which
  is also upstream's default. It links [libical](#port-libical)'s `libicalvcal`. Its phonebook
  stays the default `dummy` back end; the other two, `ebook` and `tracker`, need
  evolution-data-server and Tracker.
- `--enable-cups` — Build `bluetooth`, the CUPS backend for Bluetooth printers, which is also
  upstream's default. It links no CUPS library. It is installed into the `backend` directory under
  the `cups_serverbin` that pkg-config reads from [cups](#port-cups), or under `/usr/lib/cups` when
  that is not found; both are `/usr/lib/cups/backend` here.
- `--enable-midi` — Build Bluetooth Low Energy MIDI support, which links
  [alsa-lib](#port-alsa-lib).
- `--enable-deprecated` — Install the tools upstream marks deprecated, among them `hciattach`,
  `rfcomm` and `hcitool`. UART-attached adapters and serial links over Bluetooth need them.
- `--enable-sixaxis` — Build the plugin that pairs a PlayStation controller when it is plugged in
  by USB.
- `--enable-hid2hci` — Build `hid2hci`, which switches adapters that start in HID
  mode into normal Bluetooth (HCI) mode.
- `--disable-mesh`, `--disable-btpclient` — Build neither the Bluetooth Mesh daemon nor the BTP
  test client. Both need ELL, the Embedded Linux Library.
- `--disable-external-ell` — Do not look for an installed ELL; nothing built here uses it.
