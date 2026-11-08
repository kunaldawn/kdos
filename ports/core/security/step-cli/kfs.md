## About

`step-cli` is Smallstep's command-line tool for a private public-key infrastructure, the program
upstream calls `step`. It creates a certificate authority (`step-cli ca init`), requests, renews
and revokes certificates from a running
[step-ca](#port-step-ca), and inspects and converts keys, certificates and JWTs without one. In
KDOS it is the setup tool and the client of step-ca: one root, trusted once, that speaks ACME so
that a server such as [caddy](#port-caddy) renews its certificates against it with the same
configuration it would use for a public CA.

## Flags

- `export CGO_ENABLED=1` — Build with cgo, which upstream's `Makefile` leaves off unless asked
  (`CGO_OVERRIDE`). Any C code the build compiles then takes the tree's `CGO_CFLAGS` and
  `CGO_LDFLAGS`, which the chroot exports.
- `-X main.Version=$version` — Stamp the release into `step-cli version`. Upstream's `Makefile` also
  stamps `main.BuildTime` with the date; the recipe leaves it unset, or every build would be a
  different binary.

## Notes

**The command is `step-cli`.** Upstream's name, `/usr/bin/step`, belongs to KDE's physics simulator
[step](#port-step), whose menu entry runs it, and kpkg refuses a path two packages own. The
program's own help and error messages still say `step`, because they are compiled in: where they
print `step ca init`, the command to type is `step-cli ca init`.

The Go modules come from the vendor bundle, `step-cli-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`. `build.sh` unpacks it into the
source tree and builds with `-mod=vendor`, offline. The bundle carries its own `sha256 =` line in
the recipe, so a regenerated bundle that resolves differently is refused; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).

The bash and zsh completions are installed from upstream's `autocomplete/` directory, under the
command's name.
