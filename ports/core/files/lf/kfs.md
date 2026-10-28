## About
lf is a terminal file manager in the style of ranger, written in Go as a single binary: columns of
parent, current and preview directories, vi-style keys, and commands that are shell snippets. KDOS
seeds a configuration for it in `/etc/skel/.config/lf/` (`lfrc`, and a `preview` script that shows
text through `bat`); see [configuration](../../../../docs/kdos/06-reference/configuration.md#shipped-configuration-for-third-party-software).

## Flags
- `-X main.gVersion=r$version` — Stamps the release into the `gVersion` variable that
  `lf -version` prints. Upstream names its releases `r` and a number, so the stamp carries the `r`
  the recipe's `version` leaves out.

## Notes
`etc/lfcd.sh` is installed to `/usr/share/lf/lfcd.sh`, and `/etc/bash.bashrc` sources it when `lf`
is on the path. It defines `lfcd`, which runs `lf -print-last-dir` and leaves the shell in the
directory `lf` quit in. The `bashrc` line sources the file only when it is readable, so with the
file moved the shell has no `lfcd` and says nothing.

The Go modules come from the vendor bundle, `lf-vendor-<version>.tar.xz`, made by `go mod vendor`
and pinned by its own `sha256 =` line; `go build` uses the `vendor/` directory by default (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).
