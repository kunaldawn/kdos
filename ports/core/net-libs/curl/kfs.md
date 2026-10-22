## About

curl is the `curl` command and `libcurl`, the library behind it: URL transfers over HTTP(S) in all
three versions, FTP, SCP and SFTP, LDAP, SMTP, IMAP and the rest of its protocols. [git](#port-git),
in the same phase, links it for HTTP remotes; [rust](#port-rust)'s cargo and
[cargo-c](#port-cargo-c), in `31_compilers`, link it too; and many later ports depend on it.

KDOS's own tools run the `curl` command rather than linking the library, always through an argument
vector and never a shell. `kdos update fetch` downloads the signed ports tree with it and takes
curl's exit codes for an unresolved name, a refused connection and a timeout to mean the machine is
offline; `kpkg` downloads each source with it during `kdos update apply`; and `kdos-fetch-static`
downloads with it too. The build's own `ports/fetch` and `ports/publish` run on the build host and
use the host's curl, not this one.

Almost every switch in `build.sh` names a feature, and the library behind it is in the recipe's
`depends`, so it is installed before curl builds. Left to itself, configure looks for each optional
library and builds without the feature when it finds none; named, each makes a missing library an error instead. libidn2 is the exception: configure only
warns about it whatever the switches say. The Notes describe the two checks `build.sh` adds after
configure.

## Flags

- `--with-openssl` — build the TLS layer on [openssl](#port-openssl). curl's configure stops unless
  a TLS backend is chosen or TLS is explicitly turned off. It is also the backend HTTP/3 needs here:
  configure requires the ngtcp2 crypto helper for the TLS library curl is built on, and the
  [ngtcp2](#port-ngtcp2) port builds only the OpenSSL one, `ngtcp2_crypto_ossl`.
- `--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt` — the CA bundle compiled in as the default
  for verifying servers. The path is a link to `/etc/ssl/cert.pem`, the file
  `update-ca-certificates` writes from the Mozilla roots and any local ones, so curl trusts what
  every other OpenSSL program on the system trusts; see [TLS trust
  anchors](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#tls-trust-anchors).
- `--with-nghttp2` — HTTP/2, through [nghttp2](#port-nghttp2).
- `--with-ngtcp2` — with `--with-nghttp3`, HTTP/3 over QUIC: [ngtcp2](#port-ngtcp2) is the QUIC
  transport and [nghttp3](#port-nghttp3) the HTTP/3 layer. Configure stops if either is missing, or
  if ngtcp2 has no crypto helper for the TLS backend chosen.
- `--with-nghttp3` — see `--with-ngtcp2`.
- `--with-libidn2` — international domain names: a host name in Unicode is converted to its ASCII
  form with [libidn2](#port-libidn2) before it is resolved. Configure does not stop when libidn2 is
  missing even with this switch; it prints "Cannot find libidn2" and builds without it.
- `--with-libpsl` — cookie domains are checked against the Public Suffix List with
  [libpsl](#port-libpsl), so a server cannot set a cookie for a whole suffix such as `co.uk`.
- `--with-libssh2` — the `scp://` and `sftp://` protocols, through [libssh2](#port-libssh2).
- `--with-zstd` — accept `zstd`-compressed responses (`Content-Encoding: zstd`), through
  [zstd](#port-zstd).
- `--with-brotli` — accept Brotli-compressed responses (`Content-Encoding: br`), through the
  [brotli](#port-brotli) decoder.
- `--enable-ldap` — `ldap://` URLs. Left to itself, configure looks for the LDAP libraries and, when
  it does not find them, prints a warning and builds without LDAP; named, the switch makes it stop
  with `could not detect the LDAP libraries` instead. The libraries are
  [openldap](#port-openldap)'s, which `30_foundation`'s list installs "for curl's ldap:// URLs and
  dirmngr's directory lookups".
- `--enable-ldaps` — `ldaps://`, LDAP over TLS. It is turned off with LDAP, and with OpenLDAP it
  also needs curl's TLS layer.
- `--enable-threaded-resolver` — resolve host names on a thread of their own, so a transfer can time
  out or be cancelled while `getaddrinfo` is still waiting. It is configure's default on Linux when
  c-ares is not used, and naming it makes configure stop if no thread library is found. curl is not
  built against [c-ares](#port-c-ares).
- `--with-zsh-functions-dir=/usr/share/zsh/site-functions` — install the `_curl` completion where
  [zsh](#port-zsh) loads completions from. Configure generates it with a Perl script, and skips it,
  with a warning, when Perl is missing; [perl](#port-perl) is in the recipe's `depends`.
- `--without-fish-functions-dir` — install no fish completion. That is configure's default already;
  fish is not a port in this tree.

## Notes

Two checks run between `configure` and `make`. Each greps `lib/curl_config.h`, the header configure
writes, and stops the build unless it defines a feature:

- `USE_OPENLDAP 1`: configure found OpenLDAP's `ldap_init_fd` and built LDAP support on OpenLDAP's
  own code path. When it finds an LDAP library without that function, configure reports "enabled
  (ancient OpenLDAP)", defines nothing, and carries on; this check stops the build there instead.
- `HAVE_BROTLI 1`: the Brotli decoder was found. `--with-brotli` already stops configure when it is
  not, so this check is a second guard for the same feature.

When either fails, the message names the library: "curl: OpenLDAP not found at configure" or "curl:
brotli not found at configure". Look in `config.log` for why the library's check failed, not in
`make`'s output.
