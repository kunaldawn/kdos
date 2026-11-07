## About

KWeather shows weather forecasts for a list of saved places: current conditions, an hourly and a
daily forecast, and charts, with animated backgrounds. It is a Kirigami application over
[kweathercore](#port-kweathercore), which fetches the forecasts from the Norwegian Meteorological
Institute's api.met.no and looks places up in GeoNames. It also installs a Plasma widget and a
KRunner plugin that answer the same forecasts.

## Flags

- `-D BUILD_PLASMOID=ON` — The Plasma widget, `org.kde.plasma.kweather_1x4`, linked against
  [libplasma](#port-libplasma), which is then a requirement. On by default upstream; named so the
  widget cannot drop out.
- `-D BUILD_KRUNNER=ON` — The KRunner plugin, which answers "weather" queries in the launcher
  search, linked against [krunner](#port-krunner). On by default upstream; named for the same
  reason.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The widget and the runner are plugins loaded by the Plasma shell and by KRunner, which come with
the session; neither is named in `depends`, because a port built before the session cannot depend
on it (see [Which phase lists a port](../../../../docs/kdos/05-developer/writing-ports.md#which-phase-lists-a-port)).
The holiday markers come from the `org.kde.kholidays` QML module of
[kholidays](#port-kholidays), which the configure only reports. Every forecast and every place
search is a network request.
