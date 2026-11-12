## About

Merkuro is KDE's convergent personal information suite: three Kirigami applications, Merkuro
Calendar, Merkuro Contacts and Merkuro Mail, over the same Akonadi calendars, address books and
mail folders that [korganizer](#port-korganizer), [kaddressbook](#port-kaddressbook) and
[kmail](#port-kmail) use, and a Plasma widget that lists contacts and shows a contact's details and
QR code. Mail is read with [mimetreeparser](#port-mimetreeparser) and written with
[messagelib](#port-messagelib)'s composer; calendar maps come from
[qt6-qtlocation](#port-qt6-qtlocation).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Plasma=ON` — the contacts widget for the Plasma panel and
  desktop. `src/contacts/applet` builds it only when it finds [libplasma](#port-libplasma), with no
  `REQUIRED` and no message when it does not; required, a missing libplasma stops the configure.

## Notes

Upstream asks for every KDE PIM package at 6.8.2 or newer, the PIM version of Gear 26.08.2, so
[libkleo](#port-libkleo), [kmbox](#port-kmbox) and [mimetreeparser](#port-mimetreeparser) must be
at 26.08.2 like the rest of the PIM set, or the configure stops on the version check.

The QML the three applications load at run time imports modules the configure never checks:
`org.kde.kirigami`, `org.kde.kirigamiaddons.*`, `org.kde.kitemmodels`, `org.kde.prison` (the QR
codes), `org.kde.contacts` from [kcontacts](#port-kcontacts), `org.kde.kquickcontrolsaddons` from
[kdeclarative](#port-kdeclarative), `Qt5Compat.GraphicalEffects` from
[qt6-qt5compat](#port-qt6-qt5compat), `QtLocation` and `QtPositioning`, and the `org.kde.desktop`
style from [qqc2-desktop-style](#port-qqc2-desktop-style). Each is in `depends`, `QtPositioning`
through qt6-qtlocation's own. Four come from PIM libraries: `org.kde.akonadi` from
[akonadi](#port-akonadi) and `org.kde.akonadi.mime` from [akonadi-mime](#port-akonadi-mime), which
both always build their QML modules; `org.kde.kidentitymanagement` from
[kidentitymanagement](#port-kidentitymanagement), whose `KPim6IdentityManagementQuick` the
configure requires; and `org.kde.pim.mimetreeparser` from [mimetreeparser](#port-mimetreeparser),
which builds its QML module only when it finds Qt Quick, something no configure here checks.
Merkuro Mail's message view does not load when mimetreeparser is built without it.

The widget is a Plasma applet, `org.kde.merkuro.contact.applet`, and imports `org.kde.plasma.*`
and `org.kde.ksvg` ([ksvg](#port-ksvg)); it runs in the Plasma shell, which loads it, and not in
any of the three applications. Its metadata turns it on by default in the system tray, and
loading it starts Akonadi, so every KDOS desk's layout script leaves it out of the tray (see
[Plasma shell](../../../../docs/kdos/04-programs/plasma-shell.md)); it is built and installed, and
a user who adds it in the tray's settings gets it as upstream ships it, Akonadi with it.

[kdepim-runtime](#port-kdepim-runtime) is in `depends` for run time only: without its resources
Akonadi has no calendar, address book or mail folder to show.

Each application's Wayland app id is the desktop file name `KAboutData` derives from its component
name (`merkuro.calendar`, `merkuro.contact`, `merkuro.mail`), which is the name of its entry:
`org.kde.merkuro.calendar`, `org.kde.merkuro.contact` and `org.kde.merkuro.mail`. None of the
entries carries a `StartupWMClass`, so the task manager matches each window with no change. A
fourth entry, `org.kde.merkuro.desktop`, starts Merkuro Calendar and is `NoDisplay=true`, because
`MERKURO_FLATPAK`, which would make it a visible launcher for all three, is not set. The calendar's
entry claims `text/calendar`, as Thunderbird's and KOrganizer's do; the default is chosen in
`mimeapps.list`.
