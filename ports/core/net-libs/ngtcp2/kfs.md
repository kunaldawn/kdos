## About

ngtcp2 implements QUIC, the UDP-based transport under HTTP/3, in C. The package has two parts:
`libngtcp2`, the protocol engine, which leaves the TLS handshake to a separate library, and a crypto
helper library that connects it to one particular TLS implementation. KDOS builds the helper for
OpenSSL, `libngtcp2_crypto_ossl`, which uses the QUIC TLS interface OpenSSL itself provides.

Its one consumer in the tree is [curl](#port-curl), which builds its HTTP/3 support from this and
[nghttp3](#port-nghttp3) and requires the `ngtcp2_crypto_ossl` module to be found by `pkg-config`,
or its configure stops.

## Flags

- `--enable-lib-only` — build `libngtcp2` and the crypto helper, and not the example client and
  server. With it, configure also skips every check that only the examples use: libnghttp3, libev,
  jemalloc and the two brotli libraries.
- `--with-openssl` — build the crypto helper for OpenSSL. Configure looks for OpenSSL with
  `pkg-config` and then for the API it needs: first `SSL_provide_quic_data`, the interface of the
  quictls fork, then `SSL_set_quic_tls_cbs`, OpenSSL's own. With the second it builds
  `libngtcp2_crypto_ossl`. With neither, configure would quietly drop OpenSSL support, and the
  switch turns that into an error: `openssl was requested (--with-openssl) but not found`.
- `--without-gnutls` — no crypto helper for GnuTLS. `--without-boringssl`, `--without-picotls` and
  `--without-wolfssl` do the same for those three libraries. Each is already off by default; the
  switches say on the command line that the OpenSSL helper is the only one. GnuTLS is a port of a
  later phase, `41_system`, and the other three are not ports.
- `--without-libnghttp3` — with `--without-libev`, `--without-jemalloc`, `--without-libbrotlienc`
  and `--without-libbrotlidec`: the optional libraries of the examples. Under `--enable-lib-only`
  configure never looks for them, so these switches change nothing in the build; they record that
  the examples' dependencies are not wanted.
- `--disable-cryptotest` — skip the crypto helper's test program. It is a `check_PROGRAMS` target,
  built only by `make check`, which `build.sh` does not run.
