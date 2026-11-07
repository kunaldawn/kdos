## About

PimCommon is the shared layer under KDE's PIM applications. `KPim6PimCommon` carries the
plugin interfaces KMail, the composer and the sieve editor load their extensions through, the
custom-tools and share menus, the template lists, and the autocorrection settings; `KPim6PimCommonAkonadi` adds what
needs Akonadi: recipient completion from address books, LDAP and recent addresses, and the folder
and ACL dialogs; `KPim6PimCommonActivities` ties identities and transports to
Plasma activities. KMail, Akregator, KAddressBook, the sieve editor and the import and export
tools all link it.

## Flags

- `-D BUILD_DESIGNERPLUGIN=OFF` — Build no Qt Designer plugin for the widgets. Upstream builds one
  by default, and the configure then requires Qt's `UiPlugin` module from
  [qt6-qttools](#port-qt6-qttools), which this port's `depends` does not name.
- `-D OPTION_USE_PLASMA_ACTIVITIES=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_PlasmaActivities=ON`: build `KPim6PimCommonActivities` against
  [plasma-activities](#port-plasma-activities), and stop the configure if it is missing. Upstream
  looks for it as optional and drops the library with only a note; [mailcommon](#port-mailcommon)
  builds its activity support only when this library exists, so both ports pin it the same way.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

On Linux the configure requires `KPim6AkonadiSearch` from [akonadi-search](#port-akonadi-search)
unless `FORCE_DISABLE_AKONADI_SEARCH` is set, which this recipe leaves off: the search-backed
address completion is what the composer's recipient field uses. Every KPim library it finds,
[akonadi](#port-akonadi), [akonadi-contacts](#port-akonadi-contacts), [kimap](#port-kimap),
[kldap](#port-kldap) and [libkdepim](#port-libkdepim), must be at least PIM 6.8.2, the version of
Gear 26.08.2, so the PIM ports move together.

The text editors, autocorrection and their widgets come from [ktextaddons](#port-ktextaddons), a
KDE library released outside Gear; the configure requires 2.0.2 or newer.

[purpose](#port-purpose) provides the share menu, and [knewstuff](#port-knewstuff) the download
of shared templates in the template lists.
