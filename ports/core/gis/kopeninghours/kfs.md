## About

KOpeningHours parses, validates and evaluates OpenStreetMap `opening_hours` expressions: it
says whether a place is open at a given time, when it next opens or closes, and how an
expression reads in words, with public and school holidays taken from
[kholidays](#port-kholidays). The QML module `org.kde.kopeninghours` offers the same to QML.
[kosmindoormap](#port-kosmindoormap) and KDE Itinerary use it to show whether a station, shop or
venue is open.

## Flags

- `-D BUILD_WITH_QT6=ON` — Build against Qt 6 and KDE Frameworks 6. The project asks for an old
  ECM, and ECM's `QtVersionOption` then picks Qt 5 unless this is passed.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Qml=ON` — Make Qt's QML module a requirement. Upstream looks
  for it as an optional component of Qt and leaves out the `org.kde.kopeninghours` QML module
  without it, which kosmindoormap and Itinerary import. It comes from
  [qt6-qtdeclarative](#port-qt6-qtdeclarative).
- `-D CMAKE_DISABLE_FIND_PACKAGE_Python=ON` — Build no Python bindings. Upstream builds
  `PyKOpeningHours` whenever it finds Python's headers and Boost.Python, both of which this tree
  has, and that would put a Python module into a C++ library's package.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The expression grammar is generated at build time with [flex](#port-flex) and
[bison](#port-bison), which the configure requires.
