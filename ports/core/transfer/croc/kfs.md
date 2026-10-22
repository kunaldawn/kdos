## About

croc sends a file or a directory from one machine to another, the receiver typing a code phrase
the sender shows. On KDOS `/usr/bin/croc` is a small shell wrapper that runs the real binary with
`--local`, which keeps the transfer to direct connections on the local network: croc otherwise
falls back to its public relay on the internet, without saying so, whenever the direct path does
not come up. The unwrapped binary is installed as `croc-relay`, for a transfer that is meant to go
through the relay.

## Flags

- `-tags croc_no_tailcat` — croc's relay-only build. It leaves out the Tailcat transport, a
  WireGuard path that falls back to Tailscale's DERP relay servers, and `croc ssh`, which is built
  on it and has no `--local` of its own; with them goes the vendored Tailscale code they pull in.
  `croc ssh` then answers that it is not supported in this build.
- `ts_omit_webclient`, `ts_omit_debugeventbus` — Tailscale's own feature tags, in the same `-tags`
  list. They leave out Tailscale's web client and its event-bus debug page, which embed web assets
  that arrive minified, such as `htmx.min.js.gz`. With `croc_no_tailcat` nothing in the build
  imports either; the tags keep them out even if croc reaches that code by another route.
- `-X main.Version=$version` — Names a variable croc's `main` package does not declare: the version
  `croc --version` prints is the constant in `src/version`, the source's own, and the Go linker
  ignores an `-X` that names no variable.
- `CGO_ENABLED=0` — Build without cgo, as upstream's own makefile does.

## Notes

The modules come from the vendor bundle, `croc-vendor-<version>.tar.xz`, made by `ports/fetch` with
`go mod vendor` and used with `-mod=vendor` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The binary is built as
`croc.bin` and installed under its other name; the manual page, `croc.1`, describes the command as
upstream ships it.
