## About

restic makes deduplicated, encrypted backups into a repository, on a local disk or one of the
remote backends it speaks, such as SFTP or its own REST server, and restores from it.
`restic mount` shows the snapshots as a filesystem through FUSE, running fuse's setuid
`fusermount` to mount, which is why [fuse](#port-fuse) is in `depends`.

## Flags

- `CGO_ENABLED=0` — Build without cgo. The binary then links no C library and needs no shared
  library at run time. It is still a position-independent executable, from the `-buildmode=pie` in
  the tree's `GOFLAGS`, and asks for musl's loader, `/lib/ld-musl-x86_64.so.1`, so it starts only
  where that file exists.
- `-X main.version=$version` — Upstream's `build.go` sets the same variable. This release declares
  no `main.version`: `restic version` reports the constant in `internal/global`, which is the
  source's own version, and the Go linker ignores an `-X` that names no variable.

## Notes

The modules come from the vendor bundle, `restic-vendor-<version>.tar.xz`, made by `ports/fetch`
with `go mod vendor` and unpacked into the top of the source tree; `go build -mod=vendor` builds
from it alone (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

The manual pages and the completions are written by the binary just built, `restic generate`, so
they always describe the commands of the version installed. `generate` creates neither the
directory `--man` writes into nor the one above a completion file, which is why `build.sh` makes
all four directories first.
