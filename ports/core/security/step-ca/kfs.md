## About

step-ca is Smallstep's private certificate authority: an online CA that issues X.509 and SSH
certificates over its own API and over ACME, so that any ACME client, [caddy](#port-caddy) among
them, can obtain and renew certificates from it on a network with no route to a public CA. It is
set up and driven with [step-cli](#port-step-cli)'s `step` command, and the package adds a service
script, `89_step-ca`, that runs it as root from `/etc/step-ca`; the
[Services](../../../../docs/kdos/02-user-guide/administration.md#services) table says how to set
it up.

## Flags

- `export CGO_ENABLED=1` — Build with cgo. step-ca's PKCS#11 and YubiKey PIV key stores are
  compiled only with cgo (their files carry `//go:build cgo`); without it they compile out
  silently, and a `kms` block in `ca.json` naming either fails only when the CA starts. The YubiKey
  store talks to the key through [pcsc-lite](#port-pcsc-lite), which is in `depends`.
- `-X main.Version=$version` — Stamp the release into `step-ca version`. Upstream's `Makefile` also
  stamps `main.BuildTime`; the recipe leaves it unset, so two builds of one release are the same
  binary.

## Notes

The Go modules come from the vendor bundle, `step-ca-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`; `build.sh` unpacks it and
builds with `-mod=vendor`, offline. See
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

`build.sh` writes `/etc/init.d/89_step-ca.sh`. It sets `STEPPATH=/etc/step-ca` and skips itself,
with a `[SKIP]` line, until both `config/ca.json` and `password.txt` exist there: the first is
what `sudo STEPPATH=/etc/step-ca step ca init` writes, and the second holds the CA key's password,
which step-ca reads with `--password-file` instead of asking for it at a console. Once both exist
it starts the CA under the service supervisor, so a crashed CA is restarted. Other machines reach
it only after the firewall's `https` name is opened, when the CA listens on 443 (see
[Opening a service](../../../../docs/kdos/02-user-guide/administration.md#opening-a-service)).
