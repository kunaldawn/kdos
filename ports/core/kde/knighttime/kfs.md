## About

KNightTime decides when the day ends and the night begins. Its daemon, `knighttimed`, works the
schedule out from sunrise and sunset at the machine's location, or from fixed times, and serves
it on the session bus as `org.kde.NightTime`; the `KNightTime` library is the client side, which
subscribes to that schedule. [kwin](#port-kwin)'s Night Light follows it to warm the screen
colour, and Plasma's automatic switch between a light and a dark theme follows it too. The
settings are `~/.config/knighttimerc`.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Compiles none of the translation catalogues. kpkg removes
  every non-English catalogue from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The daemon is started by D-Bus activation. The install writes
`/usr/share/dbus-1/services/org.kde.NightTime.service`, whose `Exec=` line names
`/usr/lib/libexec/knighttimed` and which the session bus runs the first time a client asks for the
name; its `SystemdService=` line is read only by a bus that systemd starts. The install also puts
the systemd user unit that line names, `plasma-knighttimed.service`, under `/usr/lib/systemd`;
`build.sh` removes the directory, since KDOS has no systemd (see
[No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)).

Sunrise and sunset come from [kholidays](#port-kholidays)'s `SunEvents`. The location is either
typed in, or, with automatic location on (the default), asked of Qt Positioning's default source
through [qt6-qtpositioning](#port-qt6-qtpositioning), whose GeoClue plugin asks GeoClue on
behalf of the desktop id `org.kde.knighttimed`; the hidden desktop entry the port installs under
that name is the identity GeoClue authorises. When no position source answers, the daemon falls
back to the fixed times, 06:00 and 18:00 with a 30-minute transition unless they are changed, and
it keeps the last position it was given in `~/.local/state/knighttimestaterc`.
