## About

KontactInterface is the KDE PIM library through which an application becomes a page of
[kontact](#port-kontact): the `KontactInterface::Plugin` class each application implements as a
KPart, the summary widgets on Kontact's start page, and `PimUniqueApplication`, which makes a
second launch of KMail, KOrganizer or KAddressBook raise the one already running, standalone or
inside Kontact. [kmail](#port-kmail), [korganizer](#port-korganizer),
[kaddressbook](#port-kaddressbook), [akregator](#port-akregator), [zanshin](#port-zanshin) and
[kontact](#port-kontact) link it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.

## Notes

`find_package(X11 REQUIRED)` runs on every Unix but macOS, with no switch, so
[libX11](#port-libX11) is a requirement of the configure. The X11 code passes a startup id from
one instance to the other, and it runs only when `KWindowSystem::isPlatformX11()` says the
program is an X11 client; a Wayland client raises the running instance through
[kwindowsystem](#port-kwindowsystem) instead. With X11 found the library also links Qt's private
`GuiPrivate` target, so it is rebuilt with every [qt6-qtbase](#port-qt6-qtbase) release like any
other user of Qt's private headers.
