## About

KD SOAP is KDAB's SOAP library for Qt: `libkdsoap-qt6` for clients, `libkdsoap-server-qt6` for
servers, and `kdwsdl2cpp-qt6`, which turns a WSDL description into C++ client and server classes
at build time. [kdsoap-ws-discovery-client](#port-kdsoap-ws-discovery-client) is generated with it,
and through that the smb worker of kio-extras finds Windows and Samba hosts on the local network.
Consumers find it with `find_package(KDSoap-qt6)`.

## Flags

- `-D CMAKE_INSTALL_LIBDIR=lib` — KD SOAP is a plain CMake project; its `KDInstallLocation`
  module includes `GNUInstallDirs` and installs to `CMAKE_INSTALL_LIBDIR`.
- `-D KDE_INSTALL_USE_QT_SYS_PATHS=ON` — Read by the copy of ECM's `ECMGeneratePriFile` that KD
  SOAP carries: it puts the qmake `.pri` files in Qt's own `mkspecs/modules` directory instead
  of a `mkspecs/` at the top of the prefix.
- `-D KDSoap_QT6=ON` — Build against Qt 6, which names every library, header directory and
  CMake package with a `-qt6` suffix. On is upstream's default; the recipe names it, because the
  Qt 5 build installs under the same names without the suffix and is not what any consumer
  here asks for.
- `-D KDSoap_STATIC=OFF` — Shared libraries, upstream's default.
- `-D KDSoap_EXAMPLES=OFF` — Build none of the example programs, which upstream builds by
  default.
- `-D KDSoap_TESTS=OFF` — Build no tests. KD SOAP reads this switch and not `BUILD_TESTING`.
- `-D KDSoap_DOCS=OFF` — Build no API documentation, which needs Doxygen.

## Notes

`kdwsdl2cpp` is compiled with libkode, KDAB's code-generation library, which the release carries
in `kdwsdl2cpp/libkode/`. libkode is LGPL-2.0-or-later and the libraries MIT, so `license` names
both; upstream states that the code `kdwsdl2cpp` generates may carry any licence. The install also copies upstream's
`README.md` and `LICENSES/` into `/usr/share/doc/KDSoap-qt6`.
