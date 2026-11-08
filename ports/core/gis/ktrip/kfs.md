## About

KTrip is a public-transport journey planner: it finds connections between two stops and shows a
stop's departures, with the operator's real-time data where it has any. The queries and the
operator list come from [kpublictransport](#port-kpublictransport), and the interface is
Kirigami.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every search is a request to a transport operator's online service, made when the user searches;
nothing is fetched in the background.

The configure requires the QML modules as well as the libraries: `ecm_find_qmlmodule(... REQUIRED)`
stops it unless `org.kde.coreaddons`, `org.kde.kirigami`, the kirigami-addons `dateandtime` and
`formcard` modules and `org.kde.kpublictransport` with its `.ui` module are installed. It also
requires KF6 QQC2DesktopStyle on every platform but Android, so
[qqc2-desktop-style](#port-qqc2-desktop-style) is a build dependency here. Alpine passes
`-DKF_IGNORE_PLATFORM_CHECK=ON` for this module; the project includes no platform check, so the
recipe does not.

`main.cpp` sets the desktop file name `org.kde.ktrip`, so the Wayland app_id is the name of
upstream's entry.
