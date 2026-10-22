## About

This is a second OpenSSL, the 3.5 long-term line, installed beside the main
[openssl](#port-openssl) port as a [slot]{.idx term="version slot"}. It installs two libraries and its licence, `libssl.so.3` and
`libcrypto.so.3`: no headers, no `libssl.so` link, no pkg-config file, no `openssl` program and no
manual pages, so no port can build against it by accident, and nothing it installs collides with a
path of OpenSSL 4. It is there for code that cannot use OpenSSL 4: a binary built elsewhere against
OpenSSL 3, which the loader otherwise refuses for want of `libssl.so.3`, and the Qt 5 ports.
`qt5-qtbase`, `qca-qt5` and `mumble` carry the same 3.5 tarball, generate its headers in their own
builds and link these two libraries by path. Why, and the rule that keeps OpenSSL 4 out of every
Qt 5 process, are in
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

The configuration is the [openssl](#port-openssl) port's, switch for switch, with `no-module` and
`no-tests` added; that page explains the switches the two share. `qt5-qtbase`, `qca-qt5` and
`mumble` each pin the release in an `_openssl` helper of their own, so a new release here is a new
release in those three recipes too, or their headers describe a different OpenSSL from the library
they load.

## Flags

- `export CFLAGS="${CFLAGS/-O2/-O3}"` — Compile at OpenSSL's own `-O3`, as the main port does.
- `--openssldir=/etc/ssl` — Read the same `openssl.cnf` and certificate store as OpenSSL 4, so a
  program on either library trusts the same roots.
- `no-module` — Build no loadable modules. The legacy provider, which OpenSSL 4 installs as
  `/usr/lib/ossl-modules/legacy.so`, is compiled into `libcrypto.so.3` instead, so this library
  never loads a module built for the other version.
- `no-tests` — Build no test programs; the port runs none and installs only the libraries.
- `build_libs` — Build only `libcrypto` and `libssl`, skipping the `openssl` program and the
  manuals, which the main port supplies.

## Notes

`series = 3.5` in the recipe keeps `ports/update` offering only 3.5 releases for this port, so
the slot stays on the long-term line. A binary that needs a symbol added in OpenSSL 3.6 or later
still fails to load against it.
