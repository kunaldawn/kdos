## About
[libzip]{.idx} reads, creates and modifies ZIP archives, with the `zipcmp`, `zipmerge` and
`ziptool` commands. [ark](#port-ark), the emulators, the CAD and 3D programs,
[imagemagick](#port-imagemagick) and [libsigrok](#port-libsigrok) read and write their ZIP files
through it. Besides deflate it reads and writes the bzip2, xz and zstd methods, and AES-encrypted
entries through OpenSSL.

## Flags
- `-DENABLE_OPENSSL=ON` — with `-DENABLE_GNUTLS=OFF`, `-DENABLE_MBEDTLS=OFF` and
  `-DENABLE_COMMONCRYPTO=OFF`: take AES encryption from [openssl](#port-openssl) alone. libzip
  prefers OpenSSL over GnuTLS and mbed TLS when it finds several, so the two `OFF` switches
  change which library is used only when OpenSSL is missing; they keep CMake from probing for
  them. CommonCrypto is Apple's.
- `-DENABLE_BZIP2=ON` — with `-DENABLE_LZMA=ON` and `-DENABLE_ZSTD=ON`: the bzip2, xz and zstd
  compression methods, each upstream's default, named.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_OpenSSL=ON` — and the same switch for `BZip2`, `LibLZMA` and
  `zstd`: turn each of those `find_package` calls into a required one. libzip answers a missing
  library with a warning and builds without the method, so without these a library missing at
  build time would give a libzip that cannot open such an archive.
- `-DBUILD_DOC=ON` — with `-DDOCUMENTATION_FORMAT=man`: install the manual pages for the library
  and the commands, from the preformatted `man` sources in the release. Left to itself, libzip
  picks the format from the tools it finds (`mdoc` when `mandoc` or `groff` is there, `man` when
  `nroff` is, `html` otherwise), so the installed pages would depend on what the build root holds.
- `-DBUILD_TOOLS=ON` — build `zipcmp`, `zipmerge` and `ziptool`, upstream's default, named.
- `-DBUILD_EXAMPLES=OFF` — with `-DBUILD_REGRESS=OFF`: skip the example programs and the
  regression tests, which upstream builds by default.

## Notes
[musl-fts](#port-musl-fts) is in `depends` because `zipcmp` walks directories with `fts_open` and
`fts_read`, which musl does not provide; the installed `zipcmp` links `libfts.so.0`.
