## About

KWeatherCore fetches weather for a place: forecasts from the Norwegian Meteorological Institute's
`api.met.no`, place search and time zones from `secure.geonames.org`, and weather alerts from the
national CAP feeds. It caches forecasts, marks each hour day or night from the sun's position, and
exposes all of it to QML as `org.kde.weathercore`. [kweather](#port-kweather) is built on it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

Every forecast and place lookup is a request to a third-party service; nothing is fetched until
an application asks for a place. The current position comes through Qt Positioning, from
[qt6-qtpositioning](#port-qt6-qtpositioning), and the day and night split uses KHolidays'
`SunEvents` from [kholidays](#port-kholidays).
