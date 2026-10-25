## About

QCA, the Qt Cryptographic Architecture, gives Qt programs one API for hashing, ciphers, TLS,
certificates, OpenPGP and SASL, and does the work in provider plugins over real crypto
libraries. This port is the Qt 5 build, `libqca-qt5`, for the Qt 5 programs in the tree
([quassel](#port-quassel)); the Qt 6 build is the [qca](#port-qca) port, from the same tarball and
with the same `group = qca`, so the version checker offers the two only as one bump.

The `ossl` provider is the reason the build is long. A Qt 5 process may load only OpenSSL 3, never
OpenSSL 4, so this port carries the OpenSSL 3 release that the [openssl3](#port-openssl3) slot
is built from, configures it with the slot's own switches to generate its headers, and compiles
the provider against those headers and the slot's `libssl.so.3` and `libcrypto.so.3`. The rule
and the reason are in
[A second version beside the first](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## Flags

- `make build_generated` — In the OpenSSL tree, generate the headers that `./config` leaves as
  templates (`opensslv.h`, `configuration.h` and the rest) and build nothing else. The switches
  passed to `./config` are the openssl3 port's, switch for switch, so the headers describe the
  library they are compiled against; that port's page explains each.
- `-D BUILD_WITH_QT6=OFF`, `-D QCA_SUFFIX=qt5` — Build against Qt 5 and name the library, its
  pkg-config file and its CMake package with the `qt5` suffix (`libqca-qt5`, `qca2-qt5.pc`,
  `Qca-qt5`), so
  nothing collides with the Qt 6 build of the [qca](#port-qca) port.
- `-D BUILD_TOOLS=OFF` — Skip the command-line tools, `qcatool-qt5` and `mozcerts-qt5`; the
  [qca](#port-qca) port builds none either.
- `-D BUILD_PLUGINS="ossl;gnupg;cyrus-sasl;logger;softstore"` — The providers to build: OpenSSL
  (`ossl`), OpenPGP through [gnupg](#port-gnupg), SASL through [cyrus-sasl](#port-cyrus-sasl), the
  logger and the software key store. Upstream's `auto` would build every provider whose library
  it finds; the NSS, PKCS #11, Botan and libgcrypt providers are not named.
- `-D QCA_FEATURE_INSTALL_DIR=/usr/lib/qt5/mkspecs/features` — Install `crypto.prf`, the qmake
  feature file a `CONFIG += crypto` project uses, into Qt 5's own mkspecs directory.
- `-D OPENSSL_INCLUDE_DIR="$_ossl/include"`, `-D OPENSSL_SSL_LIBRARY=/usr/lib/libssl.so.3`,
  `-D OPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.so.3` — Point CMake's OpenSSL search at the
  generated OpenSSL 3 headers and the slot's two libraries by path, instead of the OpenSSL 4 headers and
  `libssl.so` it would otherwise find.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON` — Do not look for Doxygen, so no API documentation is
  generated even when it is in the build root.

## Notes

`_openssl` in the recipe pins the OpenSSL release and must equal the [openssl3](#port-openssl3)
port's `version`, as `qt5-qtbase`'s and `mumble`'s do; otherwise the provider is compiled
against headers of one release and loads the library of another.

The three lines after the install read the `ossl` provider's `NEEDED` entries and stop the build
unless it links `libssl.so.3` and neither `libssl.so.4` nor `libcrypto.so.4`, so a provider that
picked up OpenSSL 4 cannot ship into a Qt 5 process.
