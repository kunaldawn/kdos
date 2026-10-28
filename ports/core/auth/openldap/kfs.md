## About
OpenLDAP is the reference implementation of LDAP, the directory protocol. KDOS builds only its
client half: the libraries `libldap` and `liblber`, the `ldapsearch` family of command-line tools
and the client configuration file `/etc/openldap/ldap.conf`. There is no `slapd` server and no
load balancer.

The `30_foundation` list names the library for two consumers: [curl](#port-curl), whose
`ldap://` and `ldaps://` URLs go through `libldap`, and [gnupg](#port-gnupg), whose `dirmngr`
looks keys and certificates up in LDAP directories. [postgresql](#port-postgresql),
[nfs-utils](#port-nfs-utils) and [libreoffice](#port-libreoffice) link it too.

The recipe is held to one upstream release line by its `series` key, so a version check never
offers a release from a later line (see
[Filtering](../../../../docs/kdos/05-developer/writing-ports.md#filtering)).

## Flags
- `--enable-dynamic` — Link the `ldap*` programs against the shared `libldap` and `liblber`.
  At configure's default, `auto`, they are linked with the libraries built in statically, even
  though the shared libraries are built and installed.
- `--disable-slapd` — Build no `slapd`, the LDAP server, and none of its tools (`slapadd`,
  `slapcat` and the rest). The image serves no directory.
- `--with-tls=openssl` — Do TLS (`ldaps://` and StartTLS) with OpenSSL rather than GnuTLS. Named
  this way, configure stops when OpenSSL is missing; at `auto` it would try GnuTLS next and, finding
  neither, build with no TLS and only a warning.
- `--with-cyrus-sasl` — Authenticate binds with SASL through [cyrus-sasl](#port-cyrus-sasl).
- `--without-systemd` — No systemd service notification. KDOS has no systemd
  ([No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)),
  and the notification is only for the servers; with `--disable-slapd` configure ignores the
  switch anyway.
- `--without-fetch` — Do not read URLs through FreeBSD's `fetch(3)` library, which the tree does not
  carry.

## Notes
`make depend` runs before `make`, as OpenLDAP's `INSTALL` prescribes: it writes the header
dependencies into each directory's Makefile.

The manual pages are installed by directory, whether or not the server is built, so `make install`
puts `slapd(8)`, `lloadd(8)`, `slapd.conf(5)` and the other server pages on the system even with
`--disable-slapd`. `build.sh` removes every page in `man8`, which holds only server pages, and the
`slap*` and `lloadd*` pages of `man5`, so `man` does not describe programs the package does not
install.
