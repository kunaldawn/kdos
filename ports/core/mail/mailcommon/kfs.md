## About

MailCommon is the mail layer KMail and Merkuro's mail view share above Akonadi: the folder tree
and its collection settings, favourite folders, expiry, the mail filters with their filter
manager, filter actions and import from other clients' filter files, the search pattern editor,
and folder and message utilities. [kmail](#port-kmail), Merkuro, the import wizard and the mbox
importer link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D OPTION_USE_PLASMA_ACTIVITIES=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: build the activity support, which shows in
  the folder tree and folder pickers only the accounts assigned to the current Plasma activity. Upstream turns the option on only
  when [pimcommon](#port-pimcommon) was built with its activities library and then looks for
  [plasma-activities](#port-plasma-activities) as optional; requiring both stops the configure
  instead of dropping the feature quietly.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The filter actions that encrypt and decrypt use GpgME's C++ and Qt bindings,
[gpgmepp](#port-gpgmepp) and [qgpgme](#port-qgpgme), 2.0 or newer, which the configure requires;
they run [gnupg](#port-gnupg)'s `gpg` at run time. The "play a sound" action uses Qt Multimedia
from [qt6-qtmultimedia](#port-qt6-qtmultimedia), also required.

Every KPim library it finds, [akonadi](#port-akonadi), [akonadi-mime](#port-akonadi-mime),
[akonadi-contacts](#port-akonadi-contacts), [libkdepim](#port-libkdepim),
[mailimporter](#port-mailimporter), [kmailtransport](#port-kmailtransport),
[messagelib](#port-messagelib), [pimcommon](#port-pimcommon) and
[kidentitymanagement](#port-kidentitymanagement), must be at least PIM 6.8.2, the version of Gear
26.08.2, so the PIM ports move together.
