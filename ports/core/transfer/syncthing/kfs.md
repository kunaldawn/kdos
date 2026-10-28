## About

Syncthing keeps folders the same on several machines, peer to peer. The package ships it with a
starting configuration that keeps it on the local network: the machines find each other by local
discovery; global discovery and relays, which reach servers on the internet, are off, and so is
NAT traversal, which asks the router by UPnP or NAT-PMP to forward a port to the machine. The
launcher entry *File Sync* runs `kdos-syncthing`, a wrapper the recipe writes, in a terminal; the
web interface is on `127.0.0.1:8384`, and closing the terminal stops the sync.

## Flags

- `EXTRA_LDFLAGS=-s` — Upstream's `build.go` appends this variable to the linker flags it writes,
  which already hold `-w`. With it the binary has neither the symbol table nor the DWARF data,
  the pair the tree asks of every Go binary.
- `-no-upgrade` — `build.go`'s switch for the `noupgrade` build tag, which removes the built-in
  upgrade check and self-upgrade.
- `-version "v$version"` — The version string compiled in. Without it `build.go` looks for a
  `VERSION` variable, a `RELEASE` file or a git tag, and the release tarball has none of them, so
  the binary would call itself `unknown-dev`.
- `CGO_ENABLED=0` — Build without cgo, the tree's usual setting for a Go port.

## Notes

The modules come from the vendor bundle, `syncthing-vendor-<version>.tar.xz`, made by `ports/fetch`
with `go mod vendor`; `go run -mod=vendor build.go` runs upstream's build script against it (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). Only the manual pages
named `syncthing*` are installed: the tarball's `stdiscosrv.1` and `strelaysrv.1` describe the
discovery and relay servers, which are not built.

`/usr/share/kdos/syncthing-offline.xml` is a whole configuration with only an `<options>` block.
Syncthing reads a `config.xml` over its own defaults, so every key it does not name keeps
upstream's value, and the device's identity and keys are made on first start. Besides the three
network switches it declines usage reporting (`urAccepted` of `-1`) and sets the automatic upgrade
interval to zero. Its `version="52"` is the configuration schema this release writes, so no
migration runs over the file; a version bump that moves Syncthing's `CurrentVersion` should move
it too.

`kdos-syncthing` looks for an existing configuration where Syncthing itself does: `$STHOMEDIR`,
`$STCONFDIR`, `$XDG_CONFIG_HOME/syncthing` and `~/.config/syncthing` when a `config.xml` is there,
otherwise `$XDG_STATE_HOME/syncthing` or `~/.local/state/syncthing`. It copies the shipped file
there only when no `config.xml` exists, so a configuration already made is never touched, and then
runs `syncthing serve`.
