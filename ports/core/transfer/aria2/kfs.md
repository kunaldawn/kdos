## About

aria2 is a command-line download utility for HTTP(S), FTP, SFTP, BitTorrent and Metalink that
splits one download over several connections. Its JSON-RPC interface is how
[kiwix-desktop](#port-kiwix-desktop) downloads ZIM archives from the online catalogue:
[libkiwix](#port-libkiwix) starts `aria2c` as a child with RPC enabled and drives it.

## Flags

- `--with-gnutls` — With `--without-openssl`, `--with-libgcrypt`, `--without-libnettle` and
  `--without-libgmp`: [gnutls](#port-gnutls) is the TLS library and
  [libgcrypt](#port-libgcrypt) the crypto, for the BitTorrent message digests, its
  Diffie-Hellman handshake and Metalink checksums. configure otherwise takes whichever it finds
  first, and aria2 1.37's OpenSSL and nettle code compiles against neither OpenSSL 4, which makes
  the certificate name types opaque and const, nor nettle 4, whose digest functions take no
  length.
- `--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt` — The trust store HTTPS is verified
  against, the bundle [ca-certificates](#port-ca-certificates) installs.
- `--with-libxml2` — With `--without-libexpat`: Metalink and XML-RPC through
  [libxml2](#port-libxml2).
- `--with-sqlite3` — Reads Firefox and Chromium cookie stores (`--load-cookies`) through
  [sqlite](#port-sqlite).
- `--with-libcares` — Asynchronous DNS through [c-ares](#port-c-ares).
- `--with-libssh2` — SFTP through [libssh2](#port-libssh2).
- `--disable-nls` — No message catalogues; the image is English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
