## About

nghttp2 implements HTTP/2 in C: the framing layer and the HPACK header compression, as the shared
library `libnghttp2`. The upstream project also ships programs built on it, the `nghttp` client, the
`nghttpd` server, the `nghttpx` proxy and the `h2load` benchmark. KDOS builds the library alone.

[curl](#port-curl) links it for HTTP/2. [cmake](#port-cmake), [nodejs](#port-nodejs) (through
`--shared-nghttp2`, in place of the copy in its own source tree), [libsoup3](#port-libsoup3) and
[wireshark](#port-wireshark) depend on it too.

## Flags

- `--enable-lib-only` — upstream's shorthand for `--disable-app --disable-examples
  --disable-hpack-tools`: build `libnghttp2` and nothing else, and look for none of the libraries
  the programs need. Without it, configure builds each group of programs when it finds that
  group's libraries: `nghttp`, `nghttpd`, `nghttpx` and `h2load` need zlib, OpenSSL, libev and
  c-ares together, the HPACK tools `inflatehd` and `deflatehd` need [jansson](#port-jansson), and
  the examples, which are never installed, need OpenSSL and [libevent](#port-libevent). libev is
  not a port, so the four programs would be skipped anyway; jansson and libevent are ports of
  `41_system`, a later phase, so a rebuild on a finished tree would install the two HPACK tools.
  With the switch the package is the same whatever configure would find.

## Notes

`make install` writes the manual pages of the four programs whether or not they were built: the
pages are listed in `doc/Makefile.am`'s `man_MANS` unconditionally and ship pre-generated in the
release tarball. `build.sh` therefore deletes `usr/share/man` after the install, so the package does
not carry pages for commands it does not install. The library itself has no manual pages.
