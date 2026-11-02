## About

KPkPass reads Apple Wallet passes: the `.pkpass` archive of a boarding pass, an event ticket or a
loyalty card, and the `.pkpasses` bundle that holds several. It unpacks the archive with KArchive,
parses `pass.json` with its localised strings, and exposes the barcode, the fields and the images
to C++ and to QML. [kitinerary](#port-kitinerary), Itinerary and the KMail pass plugins in
[kdepim-addons](#port-kdepim-addons) read tickets through it.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds no interface translations, which the package-wide
  English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
  KPkPass ships no catalogues of its own, so the switch changes nothing in the package; every KDE
  port passes it.

## Notes

The two MIME types, `application/vnd.apple.pkpass` and `application/vnd.apple.pkpasses`, are
installed by upstream only when [shared-mime-info](#port-shared-mime-info) is older than 2.2 and
2.5, the releases that took each type into its own database. The tree's shared-mime-info is newer
than both, so the package ships no MIME XML and the types come from shared-mime-info.

The QML module `org.kde.pkpass` needs Qt Qml, which is why
[qt6-qtdeclarative](#port-qt6-qtdeclarative) is in `depends`.
