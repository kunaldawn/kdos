## About
libssh is a C library implementing the SSH protocol, client and server, with SFTP and SCP. It is
not OpenSSH's code. [wireshark](#port-wireshark) uses it for remote capture over SSH,
[remmina](#port-remmina) for SSH sessions and tunnels, [konsole](#port-konsole) and
[nbdkit](#port-nbdkit) for their SSH support. Cryptography comes from [openssl](#port-openssl).

## Flags
- `-DWITH_GSSAPI=ON` — Build GSSAPI (Kerberos) user authentication against [krb5](#port-krb5).
  This and `-DWITH_ZLIB=ON` (compression), `-DWITH_SFTP=ON`, `-DWITH_SERVER=ON` (the server-side
  API), `-DWITH_GEX=ON` (Diffie-Hellman group exchange), `-DWITH_PCAP=ON` (writing a session's
  packets to a pcap file for debugging), `-DWITH_EXEC=ON` and `-DWITH_SYMBOL_VERSIONING=ON` are
  upstream's defaults.
- `-DWITH_EXEC=ON` — Let libssh run commands named in an SSH configuration file or in options:
  `Match exec`, `ProxyCommand` and OpenSSH-style `ProxyJump`.
- `-DWITH_GCRYPT=OFF` — Use OpenSSL as the crypto backend, not libgcrypt; `-DWITH_MBEDTLS=OFF`
  likewise rules out mbed TLS.
- `-DWITH_NACL=OFF` — Do not look for libnacl for curve25519; upstream's default is to use it when
  found. The tree has no libnacl, and the curve25519 key exchange is then served by OpenSSL.
- `-DWITH_FIDO2=ON` — Support FIDO2/U2F security-key SSH keys (`sk-ecdsa`, `sk-ed25519`) through
  [libfido2](#port-libfido2). Upstream leaves it off.
- `-DWITH_PKCS11_URI=OFF` — Accept no `pkcs11:` URIs for keys held on smart cards and HSMs, and
  with `-DWITH_PKCS11_PROVIDER=OFF` use no OpenSSL PKCS#11 provider. Both are upstream's
  defaults.
- `-DWITH_INSECURE_NONE=OFF` — Leave out the `none` cipher and MAC, which send a session
  unencrypted and unauthenticated. `-DWITH_BLOWFISH_CIPHER=OFF` leaves out Blowfish-CBC. Both are
  upstream's defaults.
- `-DWITH_EXAMPLES=OFF` — Build none of the example programs, which upstream builds by default.
- `-DWITH_INTERNAL_DOC=OFF` — Generate no Doxygen documentation of the internals.
- `-DCLIENT_TESTING=OFF` — Build none of the test suites: with `-DUNIT_TESTING=OFF`,
  `-DSERVER_TESTING=OFF`, `-DGSSAPI_TESTING=OFF`, `-DFUZZ_TESTING=OFF` and
  `-DWITH_BENCHMARKS=OFF`. The client and server suites need OpenSSH and dropbear to run against,
  and the fuzzers turn the `none` cipher on.
