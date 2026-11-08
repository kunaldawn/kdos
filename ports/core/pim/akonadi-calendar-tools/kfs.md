## About

Two command-line programs for the calendars Akonadi holds. `konsolekalendar` prints, adds,
changes and deletes events, and prints a calendar as text, short text or CSV; `calendarjanitor`
scans calendars for incidences with broken fields and reports them, and can fix them, delete old
alarms or back a calendar up to an iCalendar file.
Both talk to the Akonadi server from [akonadi](#port-akonadi) and read the same calendars as
[korganizer](#port-korganizer) and [merkuro](#port-merkuro).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the KonsoleKalendar handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.

## Notes

Neither program starts a calendar of its own: with no Akonadi calendar resource configured, both
find nothing. The resources (a local iCalendar file or folder, CalDAV, Google) are agents in
[kdepim-runtime](#port-kdepim-runtime), which this port does not name; the `kde-gear-pim` meta
installs it.

Upstream installs `konsolekalendar.desktop` with `NoDisplay=true` and an `Exec=` that runs
`kdialog` to say the program is command-line only. The entry is hidden from every menu and claims
no MIME type, so nothing launches it, and `kdialog` is not in `depends`.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
