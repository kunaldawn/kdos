## About

nghttp2 implements HTTP/2 in C: the framing layer and the HPACK header compression, as the shared
library `libnghttp2`. The upstream project also ships programs built on it, the `nghttp` client, the
`nghttpd` server, the `nghttpx` proxy and the `h2load` benchmark. KDOS builds the library alone.

[curl](#port-curl) links it for HTTP/2. [cmake](#port-cmake), [nodejs](#port-nodejs) (through
`--shared-nghttp2`, in place of the copy in its own source tree), [libsoup3](#port-libsoup3) and
[wireshark](#port-wireshark) depend on it too.

## Flags

- `--enable-lib-only` — upstream's shorthand for `--disable-app --disable-examples
  --disable-hpack-tools`: build `libnghttp2` and nothing else. Upstream's configure builds the
  programs only when it finds zlib, OpenSSL, libev and c-ares together; libev is not a port in this
  tree, so without the switch they would be skipped anyway, and with it the result does not depend
  on what configure happens to find.

## Notes

`make install` writes the manual pages of the four programs whether or not they were built: the
pages are listed in `doc/Makefile.am`'s `man_MANS` unconditionally and ship pre-generated in the
release tarball. `build.sh` therefore deletes `usr/share/man` after the install, so the package does
not carry pages for commands it does not install. The library itself has no manual pages.
