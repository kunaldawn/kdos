## About

KI18n is KDE's translation library: it wraps gettext with `i18n()` and its relatives, handles
plural forms and the translation scripting of KDE catalogues, and its locale-data library names
countries and country subdivisions from the [iso-codes](#port-iso-codes) data (`KCountry`,
`KCountrySubdivision`) and finds the time zone of a location and the country of a time zone
(`KTimeZone`). It also carries the CMake macros,
`ki18n_install()` and `ki18n_wrap_ui()`, that KDE projects call. Nearly every KDE port in the tree
depends on it.

## Notes

[iso-codes](#port-iso-codes) is a run-time dependency: the locale-data library reads its JSON files
under `/usr/share/iso-codes` and keeps compact caches of them in `~/.cache/org.kde.ki18n/iso-codes/`.
Without it a program gets no country or subdivision names, and nothing fails at build time.

[python3](#port-python3) is required by the configure, which writes the interpreter's path into
the installed `KF6I18nMacros.cmake` as the fallback when a later build finds no `python3` on its
own. A KDE project that installs translation scripts runs `ts-pmap-compile.py` with that
interpreter.
