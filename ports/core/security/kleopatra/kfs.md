## About

Kleopatra is KDE's certificate manager and graphical front end to [GnuPG](#port-gnupg). It
creates, imports, exports and certifies OpenPGP and S/MIME keys, signs and encrypts files and
folders, decrypts and verifies them, and works on the clipboard. It talks to GnuPG through
[gpgmepp](#port-gpgmepp) and [qgpgme](#port-qgpgme), with most of its key-handling widgets in
[libkleo](#port-libkleo). The port also builds KWatchGnuPG, a viewer for the logs of the GnuPG
daemons.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`. kpkg removes
  every non-English catalogue from the package anyway (see
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so this only saves build time.
- `-D USE_UNITY_CMAKE_SUPPORT=OFF` — compile each source file on its own instead of in CMake
  "unity" batches, which upstream offers to speed up compilation. This is upstream's default.
- `-D USE_DBUS=ON` — build the parts that use D-Bus: talking between instances, so that a file
  opened from outside goes to the Kleopatra already running, and the presentation-related bits
  upstream lists. On by default on Linux; named so it stays on.
- `-D DISABLE_KWATCHGNUPG=OFF` — build KWatchGnuPG. This is upstream's default.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — Kleopatra's and KWatchGnuPG's handbooks are
  built only when [kdoctools](#port-kdoctools) is found; required, a missing kdoctools stops the
  configure instead of quietly dropping the handbooks (see
  [a graphical application](../../../../docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6IdentityManagementCore=ON` — with
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6MailTransport=ON` and
  `-D CMAKE_REQUIRE_FIND_PACKAGE_KPim6AkonadiMime=ON`: require
  [kidentitymanagement](#port-kidentitymanagement), [kmailtransport](#port-kmailtransport) and
  [akonadi-mime](#port-akonadi-mime), the three KDE PIM libraries behind "Publish at Mail
  Provider", which sends an OpenPGP key to the mail provider's Web Key Service from the user's
  mail identity. Upstream builds the command only when all three are found and otherwise leaves
  it out without an error.

## Notes

`build.sh` writes its own `org.kde.kleopatra.desktop` and `org.kde.kwatchgnupg.desktop` over
upstream's, which have no `StartupWMClass`. KAboutData sets each program's desktop file name from
the `kde.org` domain, so the Wayland app ids are `org.kde.kleopatra` and `org.kde.kwatchgnupg`,
not the program names, and the replacement entries name those ids. They also add search keywords
and the `Security` category. Upstream's main entry claims no MIME type; opening key and
certificate files is left to upstream's `kleopatra_import.desktop`, which is installed unchanged.

"Publish at Mail Provider" queues the request through Akonadi's outbox, on the mail identity and
transport KMail sets up, so it sends only once a KMail account exists and the Akonadi server runs.
Without them Kleopatra's other ways of sending a key, a keyserver upload and an exported file,
are unaffected.
