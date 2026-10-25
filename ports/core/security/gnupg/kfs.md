## About
[GnuPG]{.idx} is the GNU implementation of OpenPGP, and of S/MIME through its X.509 support: `gpg`
encrypts and signs, `gpg-agent` holds the secret keys and asks for passphrases through
[pinentry](#port-pinentry), `dirmngr` fetches keys and certificate revocation lists, and
`scdaemon` talks to smart cards. In KDOS it is the engine behind [gpgme](#port-gpgme), and so
behind Kleopatra, `notmuch` and podman's image signatures; [pass](#port-pass) runs `gpg`
directly.

## Flags
- `--libexecdir=/usr/lib/$name` — install the helper programs the other GnuPG programs start,
  such as `scdaemon`, `keyboxd`, `dirmngr_ldap` and `gpg-check-pattern`, in `/usr/lib/gnupg`.
- `--enable-ldap` — build `dirmngr_ldap`, which `dirmngr` runs for `ldap://` keyservers and for
  the certificates and revocation lists an X.509 directory serves, against
  [openldap](#port-openldap). configure answers a missing `libldap` with a warning and a
  `dirmngr` without LDAP, so `build.sh` checks `config.h` for `USE_LDAP` after configure and
  stops the build when it is not defined.
- `--disable-ntbtls` — with `--enable-gnutls`: `dirmngr`'s TLS, for `hkps://` keyservers and
  HTTPS fetches, is [GnuTLS](#port-gnutls). configure prefers NTBTLS, GnuPG's own TLS library,
  whenever it finds it, and falls back to GnuTLS; ntbtls is not a port.
- `--with-tss=intel` — build `tpm2daemon`, which keeps a key in the TPM, against the Intel TPM
  software stack, [tpm2-tss](#port-tpm2-tss), named rather than picked from the two configure
  probes for.
- `--enable-card-support` — with `--enable-ccid-driver`: build `scdaemon`, and its own CCID
  driver, which reaches a USB card reader directly through [libusb](#port-libusb). Both are
  upstream's default, named.
- `--enable-sqlite` — keep the TOFU (trust on first use) trust model, which stores what it has
  seen in an [SQLite](#port-sqlite) database. Upstream's default, named.
- `--with-readline` — with `--with-zlib` and `--with-bzip2`: line editing in `gpg`'s interactive
  prompts, and the ZLIB and BZIP2 compression algorithms of OpenPGP messages.
- `--disable-tests` — run none of the regression tests in `make check`.

## Notes
`import-ecdh-key.patch` is upstream's own fix (it carries its header). Without it, importing an
unprotected ECDH key builds its private key from the wrong public parameter, so a Curve25519
encryption subkey is stored under a keygrip nothing asks for, and every decryption with that subkey
fails with "No secret key".

pinentry's dialog is Qt, so `gnupg` builds in `43_toolkits`, and so does everything that reaches
it through `gpgme`; see
[Build phases are named, banded and closed](../../../../docs/kdos/01-philosophy/decisions.md#build-phases-are-named-banded-and-closed).
