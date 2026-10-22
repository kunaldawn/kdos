## About
PostgreSQL is the relational database server, with its client `psql`, its administration tools
(`initdb`, `pg_dump`, `pg_basebackup` and the rest), the contrib modules, and `libpq`, the client
library that [gdal](#port-gdal), [qgis](#port-qgis), [libreoffice](#port-libreoffice),
[python3-psycopg2](#port-python3-psycopg2) and the QPSQL driver of [qt6-qtbase](#port-qt6-qtbase)
connect through. On KDOS it runs as one shared server under the `postgres` account, started by the
service `76_postgresql` once a cluster exists; see
[Services](../../../../docs/kdos/02-user-guide/administration.md#services).

## Flags
- `--with-icu` — Build ICU collations, through [icu](#port-icu). musl's `strcoll()` is
  `strcmp()`, so a collation taken from the C library sorts text by byte value; an ICU collation
  sorts it by the rules of a language. configure's default is the same, and with it a missing ICU
  stops configure.
- `--with-openssl` — TLS for client connections and the server, and the `pgcrypto` module's
  cryptography, from [openssl](#port-openssl).
- `--with-readline` — Line editing and history in `psql`, with [readline](#port-readline).
- `--with-libxml`, `--with-libxslt` — SQL/XML support from [libxml2](#port-libxml2), and the
  `xml2` contrib module's XSL transformations from [libxslt](#port-libxslt).
- `--with-lz4`, `--with-zstd` — LZ4 and Zstandard compression: LZ4 for TOAST values, and both
  for full-page images in the write-ahead log and for `pg_basebackup`.
- `--with-python`, `--with-perl` — The PL/Python and PL/Perl server-side languages, with
  [python3](#port-python3) and [perl](#port-perl).
- `--without-tcl` — No PL/Tcl.
- `--with-pam`, `--with-gssapi`, `--with-ldap` — The `pam`, `gss` and `ldap` authentication
  methods of `pg_hba.conf`, through [pam](#port-pam), [krb5](#port-krb5) and
  [openldap](#port-openldap); LDAP also serves `libpq`'s lookup of connection parameters.
- `--with-libcurl` — `libpq`'s OAuth 2.0 client flows, over [curl](#port-curl).
- `--with-liburing` — Asynchronous I/O through io_uring, with [liburing](#port-liburing), for
  the server's `io_method = io_uring`.
- `--with-uuid=e2fs` — Build the `uuid-ossp` contrib module on the e2fsprogs UUID API, which
  [util-linux](#port-util-linux)'s `libuuid` provides.
- `--with-system-tzdata=/usr/share/zoneinfo` — Use the system's time zone database from
  [tzdata](#port-tzdata) rather than install the copy in PostgreSQL's source, so one update of
  the zone rules reaches the database too.
- `--without-llvm` — No LLVM JIT compilation of queries; configure's default is the same.
- `--disable-rpath` — Do not mark the programs with a search path for `/usr/lib`, the default
  search path already.

## Notes
`make world-bin` builds the server, the clients and every contrib module but no documentation, and
`make install-world-bin` installs the same. The manual pages are built and installed on their own
from `doc/src/sgml` with [libxslt](#port-libxslt)'s `xsltproc` and the DocBook stylesheets and DTD
in `depends`.

`build.sh` writes the service script `/etc/init.d/76_postgresql.sh`. It skips itself until the
`postgres` account exists and `/var/lib/postgres/data` holds a cluster (`sudo -u postgres initdb -D
/var/lib/postgres/data` makes one), then supervises `postgres -D /var/lib/postgres/data` run as
`postgres` through `setpriv`. It looks the account up with `getent passwd postgres`.

`postinstall.sh` creates the `postgres` group and account with `/var/lib/postgres` as its home,
and gives that directory to it at mode `0700`. The package carries the directory at that mode
already, but `kpkgadd` creates every directory `0755`, so the hook sets it again; see
[postinstall.sh](../../../../docs/kdos/05-developer/writing-ports.md#postinstallsh).
