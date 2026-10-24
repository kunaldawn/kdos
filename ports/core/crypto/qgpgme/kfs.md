## About

QGpgME is the Qt binding to GPGME, built on [gpgmepp](#port-gpgmepp): job classes that run
GnuPG operations asynchronously and report through Qt signals. It is released separately from
GPGME. In KDOS it serves [kleopatra](#port-kleopatra), the certificate manager, and
[libkleo](#port-libkleo), KDE's shared library of key-management classes and widgets.

## Flags

- `-DBUILD_WITH_QT5=OFF`, `-DBUILD_WITH_QT6=ON` — Upstream builds a Qt 5 and a Qt 6 library by
  default; only the Qt 6 one (`libqgpgmeqt6`) is built, on [qt6-qtbase](#port-qt6-qtbase).
- `-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON` — Do not look for Doxygen, so no API documentation is
  generated even when it is in the build root.

## Patches

- `release-tarball-version.patch` — The same change as [gpgmepp](#port-gpgmepp)'s patch of the same name, to the same `cmake/modules/G10GetFullVersion.cmake`: a build with no `.git` keeps the plain release version instead of `<version>-unknown`. In this project the full version reaches only the dist and release targets and Doxygen's project number.
