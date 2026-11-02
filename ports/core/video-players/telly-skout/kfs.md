## About

Telly Skout is a TV guide: it lists what is on for a chosen set of channels, from the German TV
Spielfilm site or from any XMLTV file or URL, and shows each programme's details. It is a
Kirigami application; the fetched listings are kept in an SQLite database through Qt SQL.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Crash=ON` — Crash handling through [kcrash](#port-kcrash),
  which upstream asks for as an optional component and leaves out without it; required, a missing
  one stops the configure.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

On Linux Kirigami is a QML module the window imports, not something the configure looks for, and
the desktop style is a `TYPE RUNTIME` package; [kirigami](#port-kirigami) and
[qqc2-desktop-style](#port-qqc2-desktop-style) are in `depends` so both are installed. The
built-in source covers German channels only; elsewhere an XMLTV source has to be configured. The
test and benchmark programs are compiled whatever `BUILD_TESTING` says and are not installed.
