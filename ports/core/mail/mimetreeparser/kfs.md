## About

mimetreeparser parses a mail message into its tree of MIME parts, decrypts and verifies the
OpenPGP and S/MIME parts through [libkleo](#port-libkleo) and [gpgmepp](#port-gpgmepp), and
renders the result. It builds three libraries: the core parser, a widgets viewer that
[kleopatra](#port-kleopatra) shows mail files in, and the QML module `org.kde.pim.mimetreeparser`,
the mail view of Merkuro Mail.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D USE_UNITY_CMAKE_SUPPORT=OFF` — Compiles each source file on its own instead of in CMake
  unity batches. Upstream's default, named.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Quick=ON` — Requires Qt Quick from
  [qt6-qtdeclarative](#port-qt6-qtdeclarative) for the QML module. Upstream builds it only when Qt
  Quick is found; with it, [kio](#port-kio) and [kservice](#port-kservice) are required too.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Qt6Widgets=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6WidgetsAddons=ON`
  — Require Qt Widgets and [kwidgetsaddons](#port-kwidgetsaddons) for the widgets viewer, which
  upstream builds only when both are found and which Kleopatra requires.

## Notes

The QML module's itinerary view loads [kirigami](#port-kirigami) and
[kirigami-addons](#port-kirigami-addons) at run time; the configure only lists them, which is why
both are in `depends`.
