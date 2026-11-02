## About

The Import Wizard moves another mail client's data into KDE PIM: mail folders, accounts and
identities, filters, address books, calendars and settings. It finds the client's profile on its
own or takes a folder by hand. Its own plugins read Thunderbird, Icedove, SeaMonkey, Evolution 3,
Claws Mail, Sylpheed, Balsa and Trojitá; [kdepim-addons](#port-kdepim-addons) adds more through the
`KPim6ImportWizard` library this port installs. [kmail](#port-kmail) offers it on first start and
under its Tools menu.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.

## Notes

[qtkeychain](#port-qtkeychain) is found without `REQUIRED` and marked `TYPE REQUIRED`, and the
feature summary stops the configure when it is missing; imported account passwords go through it
to the Secret Service.

The Wayland app id is `org.kde.akonadiimportwizard`, which `main.cpp` sets as the desktop file
name; it is the name of the entry upstream installs, which carries no `StartupWMClass`, so the
task manager matches the window to it with no change.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
