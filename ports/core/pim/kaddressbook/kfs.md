## About

KAddressBook is KDE's contact manager. It edits the contacts and contact groups that Akonadi holds,
in any address book a [kdepim-runtime](#port-kdepim-runtime) resource serves (a local vCard file or
folder, CardDAV, Google), searches LDAP directories through [kldap](#port-kldap), and shows and
prints contacts with the themes of [grantleetheme](#port-grantleetheme). It runs on its own or as
a page of [kontact](#port-kontact). The port also installs `KPim6AddressbookImportExport`, the
library [kdepim-addons](#port-kdepim-addons)' vCard, CSV, LDIF, LDAP and GMX import and export
plugins build against, which makes it a library port as well as an application.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg
  removes every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — the handbook is built only when
  [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the configure instead
  of quietly dropping the handbook.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6PimCommonActivities=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: an address book can be tied to Plasma
  activities and shown only in them. Upstream searches for [pimcommon](#port-pimcommon)'s
  `PimCommonActivities` library quietly, and for [plasma-activities](#port-plasma-activities) only
  once that library is found, so either missing turns the feature off without a word. Both are
  required, and the configure stops when pimcommon is built without activities.
- `-D CMAKE_DISABLE_FIND_PACKAGE_KF6UserFeedback=ON` — no KUserFeedback, through which
  KAddressBook would submit usage statistics. It is not a port, and refusing the search keeps a
  stray copy in the build root from turning telemetry on.

## Notes

[akonadi-search](#port-akonadi-search) is `REQUIRED` on every Unix, with no switch; it indexes
contacts for the search field.

[kdepim-runtime](#port-kdepim-runtime) is in `depends` for run time only: without its resources
Akonadi has no address book to show.

The Wayland app id is `org.kde.kaddressbook`, which `main.cpp` sets as the desktop file name; it
is the name of the entry upstream installs, which carries no `StartupWMClass`, so the task manager
matches the window to it with no change. The two other entries, which view a contact or a
contact group and import vCard and LDIF files, are `NoDisplay=true` and exist for their
`MimeType=` lines.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes
`/usr/share/doc/HTML/<lang>/` from the package.
