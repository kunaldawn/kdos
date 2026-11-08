## About

KCalendarCore is the KDE Frameworks data model of the iCalendar standard (RFC 5545): events,
to-dos, journals, recurrence rules, alarms and time zones, read from and written to `.ics` files
and vCalendar through [libical](#port-libical). It has a QML plugin for Qt Quick programs.
The calendar parts of KDE PIM ([korganizer](#port-korganizer), [merkuro](#port-merkuro),
[kalarm](#port-kalarm)) are built on it, and [digikam](#port-digikam) links it.

## Flags

- `-D BUILD_PYTHON_BINDINGS=OFF` — Build no Python bindings. Upstream's default is on, and the
  configure then requires Shiboken6 and PySide6, which this port's `depends` does not name.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language. The switch also skips upstream's `en`
  catalogue, which holds only English plural forms, so a count reads as the source writes it,
  with the `(s)` left in.

## Notes

libical is a requirement of the configure with no switch, and the QML plugin, on by default
(`USE_QML`), requires Qt Qml from [qt6-qtdeclarative](#port-qt6-qtdeclarative).
