## About

kidentitymanagement holds the identities a person sends mail as: the sender name and address, the
organisation, the signature, the reply-to and BCC addresses, the OpenPGP and S/MIME keys to sign
and encrypt with, and the folders and transport each identity uses. `KPim6IdentityManagementCore`
stores them, `…Widgets` is the identity editor KMail shows, and `…Quick` is the QML module
Merkuro's mail and calendar pages use. KMail, [kcalutils](#port-kcalutils),
[calendarsupport](#port-calendarsupport) and the message libraries link it.

## Flags

- `-D BUILD_QUICK_SUPPORT=ON` — Build the QML module `org.kde.pim.identitymanagement`, which
  makes [kirigami-addons](#port-kirigami-addons) and Qt Quick requirements of the configure. It is
  upstream's default; passing it keeps Merkuro's identity pages from being dropped by a change of
  default.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The signature editor is built on [kpimtextedit](#port-kpimtextedit) and on `KF6TextCustomEditor`
from [ktextaddons](#port-ktextaddons), a KDE library released on its own schedule, not from
Frameworks. The QML module imports [kirigami](#port-kirigami) at run time.
