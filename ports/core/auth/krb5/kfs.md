## About

MIT [Kerberos]{.idx} 5 provides the libraries (`libkrb5`, `libgssapi_krb5` and the rest) and the
client tools — `kinit`, `klist`, `kdestroy`, `kpasswd`, `kvno` — with which a machine gets and
uses tickets from a realm's key distribution centre. KDOS needs it as a client and nothing more:
`kinit` gets a ticket, and `cifs.upcall` from `cifs-utils` hands it to the kernel's cifs module so
that a `sec=krb5` mount reaches a file server that will not take a password. The server half —
the KDC, `kadmind`, the database utilities — is built, because there is no switch to leave it
out, and then removed from the package. See also
[Kerberos is built and has never been given a ticket](../../../../docs/kdos/06-reference/known-gaps.md#kerberos-is-built-and-has-never-been-given-a-ticket).

## Flags

- `--sbindir=/usr/sbin`, `--runstatedir=/run` — The server programs install to `/usr/sbin`, and
  `build.sh` removes them from there; `libkrb5` also compiles the directory in as the value of
  `%{SBINDIR}` in a `krb5.conf` path. The run-time directory compiled in is `/run`. `make install`
  creates that directory in the package, and the
  [package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint) refuses
  `run` in a port's payload, so `build.sh` removes it again.
- `--disable-rpath` — Every library lands in `/usr/lib`, and an rpath naming the build tree is
  one the package manager cannot rewrite.
- `--disable-nls` — krb5 calls `dgettext` directly. With NLS left on, `configure` finds
  `libintl.h` from the `gettext` port and then looks for `dgettext` by linking, finds it in musl's
  C library and adds no `-lintl`; but the `gettext` port's `libintl.h` renames the call to
  `libintl_dgettext`, which only `libintl` defines, and the first shared library fails with
  `undefined reference to libintl_dgettext` (see
  [Gettext on musl](../../../../docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl)).
  What is given up is translated messages from the Kerberos libraries.
- `--enable-dns-for-realm` — Find a realm's KDCs from the DNS SRV records a domain publishes,
  instead of needing every realm written out in `krb5.conf`; that is the shape a `sec=krb5` mount
  is used in.
- `--with-crypto-impl=openssl` — Take AES, SHA-2 and the rest from OpenSSL rather than krb5's
  built-in code; a second implementation on the same disk is a second set of advisories to track.
- `--with-tls-impl=openssl` — Build the TLS module that reaches a KDC through an MS-KKDCP HTTPS
  proxy.
- `--with-spake-openssl` — SPAKE pre-authentication with libcrypto's NIST elliptic-curve groups.
- `--enable-pkinit` — PKINIT, `kinit` with a certificate or a smart card.
- `--with-keyutils` — The kernel-keyring credential cache, `KEYRING:`, which a `krb5.conf` may
  name instead of a file; `keyutils` is a dependency for it.
- `--with-system-et` — Use `compile_et` and `libcom_err` from `e2fsprogs` instead of building and
  installing krb5's own copy.
- `--without-system-verto` — Build and install the bundled `libverto`, the event-loop abstraction
  the KDC, `kadmind`, `libkrad` and the OTP pre-authentication plug-in link. There is no libverto
  port; left alone, `configure` would take a system libverto whenever `pkg-config` found one and
  the bundled copy otherwise, so the switch fixes the choice.
- `--without-ldap`, `--without-lmdb` — The LDAP and LMDB database back ends, which serve the KDC
  this package does not ship.
- `--without-readline`, `--without-libedit` — The line editor they would give is `kadmin`'s, and
  `kadmin` administers a realm this system does not serve.

## Patches

- `openssl4.patch` — Ports the PKINIT and KKDCP TLS plug-ins to OpenSSL 4, where `ASN1_STRING` is opaque and several getters return `const`. `k5tls` checks the proxy's certificate with `X509_check_host()` and `X509_check_ip_asc()` instead of walking subjectAltName and the common name by hand, and PKINIT reads every ASN.1 string through `ASN1_STRING_get0_data()` and `ASN1_STRING_length()`, both present in every OpenSSL from 1.1.0 on.

## Notes

- **The TLS module, SPAKE, PKINIT and the keyring are named, not probed.** Left to `configure`,
  each is dropped without an error when its probe misses.
- **The configure script is in `src`.** Everything above it in the tarball is documentation, so
  `build.sh` changes directory after the patch, which is written against the top.
- **What is removed after the install.** The server programs (`krb5kdc`, `kadmind`,
  `kadmin.local`, `kdb5_util`, `kprop`, `kpropd`, `kproplog`) and their manual pages; the
  tutorial servers and their clients (`sserver`/`sclient`, `gss-server`/`gss-client`,
  `sim_server`/`sim_client`, `uuserver`/`uuclient`); `krb5-send-pr`, which mails a bug report
  through a mail transport nothing configures; and `ksu`. A partial `make` in a tree with this
  many inter-library dependencies fails in places that have nothing to do with the KDC, so the
  whole tree is built and these are taken back out.
- **`ksu` would be a setuid-root binary nothing needs.** It is `su` for a Kerberos principal, on a
  machine whose accounts are local and whose privilege escalation is `sudo`; see
  [setuid binaries](../../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).
- **The `request-key` rule.** `build.sh` installs `/etc/request-key.d/cifs.spnego.conf`, which
  tells `request-key` to run `cifs.upcall` when the kernel's cifs module asks for a ticket.
  `keyutils` owns that directory and ships it empty; without the rule the mount fails with
  `Required key not available`, and nothing says which key. There is no rule for `cifs.idmap`:
  it needs winbind's client library, which `samba` here is built without.
