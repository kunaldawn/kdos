## About
socat relays bytes between two endpoints of almost any kind: TCP and UDP sockets, UNIX sockets,
TLS connections, files, pipes, ttys and programs it starts. It is built with readline, for its
interactive `READLINE` address, and with OpenSSL, for its `OPENSSL` and `OPENSSL-LISTEN`
addresses.

## Flags
- `--enable-readline` — Include the `READLINE` address type. It is socat's default, but configure
  drops the feature without an error when its probe for a usable readline fails, so `build.sh`
  then checks `config.h` for `WITH_READLINE 1` and stops the build if the feature is missing.
- `--enable-openssl` — Include the TLS address types. Like readline, it is on by default and
  quietly dropped when OpenSSL's headers or libraries are not found; the `grep` for
  `WITH_OPENSSL 1` in `config.h` turns that into a build failure.
- `--disable-libwrap` — Build without TCP wrappers access control (`tcpd.h`, `libwrap`). The tree
  has no TCP wrappers port.

## Patches
- `openssl4.patch` — In `xio-openssl.c`, read an iPAddress subject-alternative-name's bytes and length through `ASN1_STRING_get0_data()` and `ASN1_STRING_length()` instead of the structure's `data` and `length` fields: OpenSSL 4 makes `ASN1_STRING` opaque, and the accessors exist in every OpenSSL from 1.1.0.
