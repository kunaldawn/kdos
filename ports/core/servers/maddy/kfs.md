## About

Maddy is a mail server in one Go program: SMTP for receiving and sending, message submission, and
IMAP for reading, with its own account database and mail storage. It replaces the usual set of an
MTA, an MDA and an IMAP server for a small installation, such as a LAN with no mail provider. The
package adds a service script, `78_maddy`, which runs it under the supervisor as the `maddy`
account; other machines reach it once the firewall's `mail` name is on (see
[Opening a service](../../../../docs/kdos/02-user-guide/administration.md#opening-a-service)).

## Flags

- `export CGO_ENABLED=1`, `-tags libsqlite3` — Build with cgo for one library: the `libsqlite3`
  tag makes the go-sqlite3 driver link the [sqlite](#port-sqlite) port instead of compiling the
  copy of SQLite's C source it vendors, so the credentials and IMAP databases use the library every
  other program here does. The `libpam` tag, which would build the PAM authentication module, is
  not given; accounts live in Maddy's own credentials table and are managed with `maddy creds`.
- `-X github.com/foxcpp/maddy.Version=$version` — Stamp the release into the binary's version string,
  as upstream's own `build.sh` does.

## Notes

The Go modules come from the vendor bundle, `maddy-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`; `build.sh` unpacks it and
builds with `-mod=vendor`, offline. See
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring). The manual page is
rendered from `docs/man/maddy.1.scd` with [scdoc](#port-scdoc).

`/etc/maddy/maddy.conf` is upstream's configuration as it ships. The host name, the domain and the
certificate paths at its top are the part every installation edits.

Maddy never drops privileges itself. The service script makes `/run/maddy`, its runtime
directory, which the account cannot create under `/run`, and starts it through `setpriv` as the
`maddy` account with one capability, `net_bind_service`, so that it can bind ports 25, 143, 465,
587 and 993.

The service skips itself until the `maddy` account exists and `/var/lib/maddy/credentials.db` is
non-empty. `maddy creds create <address>` writes the first account into `credentials.db` and
`maddy imap-acct create <address>` its mailbox into `imapsql.db`; before that there is no mailbox
to deliver to and nobody to let in. Both commands must run as the `maddy` account: a database
created as root belongs to root, and the daemon can then read it but never write to it.

`postinstall.sh` creates the account and group and gives it `/var/lib/maddy` with mode 750, set
there because `kpkgadd` creates every directory 0755, whatever mode it was packaged with. It
writes under `PKG_ROOT` and reads the ids from that root's own `passwd`.
