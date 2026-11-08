## About

akonadi-contacts keeps address books in [akonadi](#port-akonadi). `KPim6AkonadiContactCore`
holds the contact and contact-group models and jobs; `KPim6AkonadiContactWidgets` holds the
contact editor, the contact viewer, which renders a vCard through a
[grantleetheme](#port-grantleetheme) template, and the address-book selection widgets. The two
serializer plugins store a vCard and a contact group as Akonadi items. KAddressBook, KMail,
Merkuro and [akonadi-calendar](#port-akonadi-calendar) link it.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6Mime=ON` — Take KMime from its Frameworks package. The
  configure asks for `KF6Mime` first and falls back to the PIM-era `KPim6Mime`; required, the
  fallback can never be taken. [kmime](#port-kmime) installs `KF6Mime`.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

`KF6TextUtils` comes from [ktextaddons](#port-ktextaddons), a KDE library released on its own
schedule, not from Frameworks. The contact editor's QR code of a contact is drawn by
[prison](#port-prison).
