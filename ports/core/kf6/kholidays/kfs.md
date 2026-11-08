## About

KHolidays answers which days are public holidays in a region, from the holiday plan files it
carries for most countries and their subdivisions, and computes seasons, sunrise and sunset, moon
phases and the astronomical events of a date. It has a QML module, `org.kde.kholidays`. The Plasma
clock's calendar shows holidays through it, and [korganizer](#port-korganizer),
[merkuro](#port-merkuro) and [kalarm](#port-kalarm) use it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files. The strings in the source
  are English, and the package-wide English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove every catalogue in another language.

## Notes

The holiday plan files are written in a small language whose parser the build generates with
[flex](#port-flex) and [bison](#port-bison) (3.3.2 or newer); both are requirements of the
configure. The QML module needs Qt Qml from [qt6-qtdeclarative](#port-qt6-qtdeclarative), which
the configure requires as well.
