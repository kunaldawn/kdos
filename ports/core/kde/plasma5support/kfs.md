## About

Plasma 5 Support carries the data engines and services of Plasma 5's libplasma into Plasma 6, for
the widgets that still read them: time and time zones, battery and power management, devices
and hotplug, places, the keyboard's lock keys, the mouse position, running applications,
activities, executables, geolocation, favicons and the weather providers. A widget reads an
engine through the `org.kde.plasma.plasma5support` QML module. [plasma-workspace](#port-plasma-workspace),
[plasma-desktop](#port-plasma-desktop) and [kdeplasma-addons](#port-kdeplasma-addons) use it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the translations of the engines' messages,
  which the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D BUILD_QCH=OFF` — No API documentation in Qt Help format; no port builds QCH.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KSysGuard=ON` — The device-notifications engine, which names
  the programs that keep a device busy when it cannot be unmounted, takes their names from
  [libksysguard](#port-libksysguard). Upstream builds the engine only when that library is found;
  required, its absence stops the configure.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6NetworkManagerQt=ON` — The geolocation engine and its
  network-address provider, through [networkmanager-qt](#port-networkmanager-qt). Built only when
  found, so it is required.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON` — The activities engine, through
  [plasma-activities](#port-plasma-activities). Built only when found, so it is required.

## Notes

The device-notifications engine finds the processes holding a device by running `lsof -t` on it,
so [lsof](#port-lsof) is in `depends` for run time; without it the notification names no
program.

The geolocation engine also builds a GPS provider whenever `pkg-config` finds `libgps`, a test
with no switch, so [gpsd](#port-gpsd) is in `depends` and the provider is always built; it reads
positions from a running `gpsd`.

Two engines reach the network, and only when a widget asks them to: the geolocation provider
that locates the machine from the networks it can see asks a web service, and the weather engine
fetches forecasts from the providers it ships (the BBC, Environment Canada, NOAA, DWD and
wetter.com).
