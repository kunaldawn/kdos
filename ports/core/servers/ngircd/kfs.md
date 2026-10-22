## About

ngIRCd is a small IRC server, enough for a room of machines on one network to chat with no
internet behind them. The package installs the server, its manual pages and the sample
configuration, and an init script, `79_ngircd`, that runs it under the supervisor as its own
`ngircd` account. Nothing runs until the administrator writes `/etc/ngircd.conf`: the script is
skipped until that file exists and is not empty (see
[Services](../../../../docs/kdos/02-user-guide/administration.md#services)). The server listens on
TCP 6667, and on 6697 for TLS once the configuration names it; the `irc`
[firewall](../../../../docs/kdos/04-programs/daemons.md#firewall) name opens both to other
machines.

## Flags

- `--with-openssl` — Build TLS for client and server links against OpenSSL, the
  [openssl](#port-openssl) port in `depends`.
- `--without-gnutls` — The other TLS library ngIRCd can use. configure builds neither unless asked
  and refuses both at once.
- `--with-zlib` — Compress server-to-server links with zlib. configure would detect it on its
  own; naming it makes a missing library a failed configure.
- `--with-iconv` — Build the `CHARCONV` command, which upstream builds only when asked for, with which a client names its character set and
  the server converts that client's messages to and from UTF-8. On musl, `iconv` is part of the C
  library.
- `--without-ident` — Leave out IDENT (RFC 1413) lookups of connecting users. The library it needs,
  libident, is not a port.
- `--without-tcp-wrappers` — Leave out `hosts.allow`/`hosts.deny` checks through libwrap, which is
  not a port.
- `--without-pam` — Leave out authenticating clients through PAM, which upstream leaves off by
  default.
- `--with-syslog`, `--with-epoll`, `--enable-ipv6` — Logging through syslog, the epoll event
  interface and IPv6. configure detects all three by default; naming them keeps the build from
  depending on what it finds.

## Notes

`make install` copies the sample configuration to `/etc/ngircd.conf` when none is there, and
`build.sh` deletes that copy from the package. The init script starts the server whenever that
file exists, so shipping it would run an IRC server on every machine that installs the package.
The sample stays at `/usr/share/doc/ngircd/sample-ngircd.conf`, and copying it into place is the
decision to run one.

The init script starts `ngircd -n -f /etc/ngircd.conf` through `setpriv`, which drops to the
`ngircd` account before the server starts: `-n` keeps it in the foreground where the supervisor
watches it, and `ServerUID` in the configuration is not needed. Neither port needs privilege to
bind. The account is made by `postinstall.sh` under `PKG_ROOT`, the root the package is being
installed into, so an install into another tree, such as the inactive A/B slot, gets the account
in that tree.
