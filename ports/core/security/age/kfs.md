## About

age is a small file encryption tool with short explicit keys and no configuration options. An age recipient
is a short line (`age1…`) that can be read out over a phone, and a file can be encrypted to
several recipients, to SSH public keys, or to a passphrase. The package holds `age`,
`age-keygen`, `age-inspect`, which prints an encrypted file's recipient types and size breakdown
without decrypting it, and `age-plugin-batchpass`, a plugin for passphrase encryption that takes
the passphrase from the environment rather than a terminal; each comes with its manual page.

## Flags

- `-X main.Version=v$version` — Stamp the release into `--version`. The variable overrides the
  module version Go records, which a build from a tarball without git history does not have, and
  upstream's comment on it asks for the form `v1.2.3`, hence the `v`.

## Notes

The Go modules come from the vendor bundle, `age-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` because the recipe says `vendoring = go`; `build.sh` unpacks it and
builds with `-mod=vendor`, offline. See
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring).

The loop builds each command from `./cmd/<name>` and installs it with its page from
`doc/<name>.1`. A command added upstream reaches the package only when its name is added to the
loop, and the build fails if a listed command has no page.
