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
  ([No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--enable-obex` — Build `obexd`, the OBEX service for file transfer and phonebook access. It
  needs [libical](#port-libical)'s `libicalvcal`. Its phonebook stays the default dummy back end;
  the other one needs evolution-data-server.
- `--enable-cups` — Install the CUPS backend for Bluetooth printers. `configure` installs it only
  when it can read CUPS's `serverbin` directory from pkg-config, and says nothing when it cannot,
  so [cups](#port-cups) is in `depends` to make the result the same in every build order.
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
