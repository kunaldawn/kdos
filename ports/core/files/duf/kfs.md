## About
duf lists mounted filesystems with their size, use and free space, in the manner of `df`, as
coloured tables grouped into local, network, FUSE and special devices. It is a single Go binary with
no C dependencies. The interactive shell aliases it as `dff` (`/etc/bash.bashrc`).

## Flags
- `-X main.Version=$version` — Stamps the release into the `Version` variable `duf --version`
  prints. Left empty, duf takes the main module's version from the Go build information; a build
  from a source tree records it as `(devel)`, and duf then prints `(built from source)`.

## Notes
The Go modules come from the vendor bundle, `duf-vendor-<version>.tar.xz`, which `ports/fetch`
writes with `go mod vendor` and pins with its own `sha256 =` line. `build.sh` passes no
`-mod=vendor`: when a `vendor/` directory with its `modules.txt` sits beside a `go.mod` whose `go`
line is 1.14 or later, `go build` uses it by default. See
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
