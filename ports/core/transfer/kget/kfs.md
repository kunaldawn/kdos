## About

KGet is KDE's download manager: a queue of transfers that pause, resume and survive a restart,
split into segments across several connections or mirrors, checked against a checksum or a
signature, and grouped with their own speed limits and destination folders. Besides HTTP and FTP
through KIO it fetches Metalink files and BitTorrent downloads, and a Dolphin service menu sends
links to it. [qbittorrent](#port-qbittorrent) is the torrent client the desktop opens by default;
KGet's torrent support is for downloads queued here.

## Flags

- `-D CMAKE_REQUIRE_FIND_PACKAGE_SQLite3=ON` — Make [sqlite](#port-sqlite) a requirement, for the
  transfer history's SQLite store. Upstream marks it recommended and falls back to an XML file.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON` — The handbook, through
  [kdoctools](#port-kdoctools), which upstream looks for as optional; required, a missing one
  stops the configure instead of leaving the handbook out.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Gpgmepp=ON` — With
  `-D CMAKE_REQUIRE_FIND_PACKAGE_QGpgmeQt6=ON`: make [gpgmepp](#port-gpgmepp) and
  [qgpgme](#port-qgpgme) requirements, for checking a download against its OpenPGP signature.
  Upstream marks them recommended and builds without signature checks when GpgME++ is missing.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_KTorrent6=ON` — Make [libktorrent](#port-libktorrent) a
  requirement. Upstream marks it recommended and leaves the BitTorrent transfer plugin out
  without it.
- `-D CMAKE_DISABLE_FIND_PACKAGE_LibMms=ON` — Never look for libmms, so the MMS stream plugin is
  never built. libmms has had no release since 2014 and is not a port; it is on the list of
  optional dependencies the KDE port defers, each pinned off so that a later port changes this
  build on purpose.
- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The port installs a D-Bus activation file for `org.kde.kget` that names the program by path, with
no `SystemdService=` line, so the session bus starts it directly.

"Shut down when downloads finish" asks the Plasma session's `org.kde.PlasmaShutdown` service, and
does nothing outside that session; a Gear port names no session port in `depends`. Credentials
for a site are kept in [kwallet](#port-kwallet).
