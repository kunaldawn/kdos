## About

Kontact is KDE's personal information suite: one window with a side bar that switches between
KMail, KOrganizer's calendar, to-do list and journal, KAddressBook, Akregator and Zanshin, each
embedded as a KPart through [kontactinterface](#port-kontactinterface), and a start page that
summarises them. The port is only the shell and its settings; every page is a plugin the
application itself installs: [kmail](#port-kmail) (mail and the summary page),
[korganizer](#port-korganizer) (calendar, to-dos, journal, special dates),
[kaddressbook](#port-kaddressbook), [akregator](#port-akregator) and [zanshin](#port-zanshin).
The introduction page shown before any part is chosen is a
[qt6-qtwebengine](#port-qt6-qtwebengine) view of a [grantleetheme](#port-grantleetheme) template.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.

## Notes

None of the applications whose pages Kontact shows is in `depends`: each is a separate choice of
what to install, and Kontact with none of them installed has no page to show. The `kde-gear-pim`
meta installs all of them.

The Wayland app id is `org.kde.kontact`, which `main.cpp` sets as the desktop file name; it is the
name of the entry upstream installs, which carries no `StartupWMClass`, so the task manager
matches the window to it with no change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
