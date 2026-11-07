## About

LibKTorrent is the BitTorrent engine of [ktorrent](#port-ktorrent): torrent and magnet-link
parsing, the peer wire protocol with message stream encryption, the DHT, µTP, trackers over HTTP
and UDP, web seeds, UPnP port mapping, piece selection and disk storage with
preallocation. KTorrent's interface and plugins sit on top of it; nothing else in the tree links
it.

## Flags

- `-D USE_CRYPTO_BACKEND=OpenSSL` — Hashing and the encryption of peer connections through
  [openssl](#port-openssl). Upstream's `Auto` takes OpenSSL 3 when it finds it and falls back to
  libgcrypt quietly; naming the backend stops the configure if OpenSSL is missing, so the library
  never changes backend with what happens to be installed.
- `-D WITH_XFS=OFF` — No XFS-specific preallocation. With it, upstream looks for `xfs/xfs.h` from
  xfsprogs' development headers without saying whether it found it, and uses XFS's own
  reservation ioctl when it does. `posix_fallocate()` already reserves the space on XFS as on
  every other filesystem the kernel supports it on, so the switch only removes a silent
  dependency on which headers are present.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

musl has no separate 64-bit file functions (`fopen64`, `fseeko64`, `mmap64` and the rest), so the
configure's checks for them fail. That is expected: the code falls back to the plain calls, and
`off_t` is 64 bits wide on musl, so files over 2 GiB work.

[boost](#port-boost) is a header-only requirement (the configure asks for Boost's own CMake
package, and µTP uses its `circular_buffer`), and [gmp](#port-gmp) does the Diffie-Hellman arithmetic of the encrypted handshake.
The library opens listening sockets for peers, the DHT and µTP only when an application starts
a torrent session.

The `ktcli` example is built with the library and not installed.
