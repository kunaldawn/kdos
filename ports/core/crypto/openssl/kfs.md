## About

OpenSSL is the TLS library and general cryptography library most of the tree links: curl, git,
python3's `ssl` and `hashlib`, openssh, wpa_supplicant, cryptsetup, libarchive, krb5, nodejs and
rust among them. Its `openssl` program is used by the build as well: the [linux](#port-linux) port
generates the kernel's module-signing key with `openssl req` when the build has none (see
[Module signing](../../../../docs/kdos/03-architecture/security-model.md#module-signing)), and
[wireless-regdb](#port-wireless-regdb) checks, with `openssl smime -verify`, that the regulatory
database it builds from `db.txt` matches upstream's signature.

This port is OpenSSL 4, whose libraries are `libssl.so.4` and `libcrypto.so.4`, and every port
builds against it. A program that cannot use OpenSSL 4 loads the 3.5 runtime from the separate
[openssl3](#port-openssl3) slot instead; see
[A second version beside the first](../../../../docs/kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).
The certificate store it reads is described in
[TLS trust anchors](../../../../docs/kdos/03-architecture/security-model.md#tls-trust-anchors).

## Flags

- `export CFLAGS="${CFLAGS/-O2/-O3}"` — Compile at `-O3`. OpenSSL's own Linux configurations
  build at `-O3`, and an exported `CFLAGS` replaces their flags whole, so the tree's `-O2` would
  otherwise win. The substitution raises the level inside the tree's flags and keeps the rest of
  them (see [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `--openssldir=/etc/ssl` — Read `openssl.cnf` and the default certificate locations from
  `/etc/ssl`, the directory where `ca-certificates` writes `/etc/ssl/cert.pem`. Every program
  linked against OpenSSL then trusts the system's roots with no setting of its own.
- `enable-ec_nistp_64_gcc_128` — Build the optimised implementations of the NIST P-224, P-256, P-384
  and P-521 curves that use the compiler's 128-bit integer type. They are off by default; upstream's
  `INSTALL.md` supports them only on a little-endian processor that tolerates misaligned access,
  with a compiler that has `__uint128_t`, which x86-64 with GCC is.
- `enable-camellia`, `enable-seed`, `enable-rfc3779`, `enable-argon2` — The Camellia and SEED
  ciphers, the RFC 3779 certificate extensions for IP addresses and AS numbers, and the Argon2
  key-derivation function. All four are on by default in OpenSSL 4, so naming them restates the
  default.
- `enable-ktls` — Build kernel TLS support, off by default: once a TLS session is set up, libssl
  can hand the record encryption to the kernel's `tls` module, so `sendfile` and `splice` work on a
  TLS socket.
- `no-mdc2`, `no-ec2m`, `no-sm2`, `no-sm4` — Leave out the MDC-2 hash, elliptic curves over binary
  fields, and the SM2 signature and SM4 cipher algorithms. `openssl list -disabled` on the built
  tree lists them, beside the ones upstream leaves out by default.
- `shared` — Build `libssl` and `libcrypto` as shared libraries as well as the static archives,
  which are built either way and ship.
- `threads` — Build with thread support, so the libraries may be used from several threads at
  once. It is the default on Linux.
- `zlib` — Build zlib compression and decompression into libcrypto, linked against
  [zlib](#port-zlib), which is in `depends`. Upstream's default is off.
- `MANSUFFIX=ssl` — Install the manual pages with an `ssl` suffix on the section, as
  `passwd.1ssl` and `rand.1ssl`. OpenSSL's command pages have generic names, and the suffix keeps
  them from colliding with another package's page of the same name.
- `install_sw` — Install the programs, libraries, headers and pkg-config files.
- `install_ssldirs` — Create `/etc/ssl` with its `certs`, `private` and `misc` directories, and
  install `openssl.cnf`, `ct_log_list.cnf` and the `CA.pl` and `tsget` scripts.
- `install_man_docs` — Install the manual pages. `make install` would also run
  `install_html_docs`; the HTML manual is left out, and the tree's prune removes HTML under
  `/usr/share/doc` from every package in any case.

## Notes

The legacy provider is a loadable module, `/usr/lib/ossl-modules/legacy.so`. It belongs to this
port only: the `openssl3` slot is built `no-module` and carries its legacy provider inside
`libcrypto.so.3`, so a process holding the 3.5 library never loads a module built for OpenSSL 4.
