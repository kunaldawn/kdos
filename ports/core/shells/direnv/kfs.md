## About

direnv loads and unloads environment variables as the shell enters and leaves a directory: a
`.envrc` in a project directory is run by bash, once it has been allowed with `direnv allow`, and
the variables it exports are removed again when the shell leaves the directory. The port installs
the `direnv` binary, its manual pages and upstream's fish hook in
`/usr/share/fish/vendor_conf.d/direnv.fish`, which turns it on in every fish shell. The system
`bash.bashrc` does not hook it, so a bash user adds `eval "$(direnv hook bash)"` to their own
`~/.bashrc`.

## Flags

- `CGO_ENABLED=0` — Build without cgo, so the binary needs no shared library. It is still a
  position-independent executable, from the `-buildmode=pie` in the tree's `GOFLAGS`, and names
  musl's loader as its interpreter. direnv is pure Go; this is the tree's usual setting for a Go
  port (see
  [go](../../../../docs/kdos/05-developer/writing-ports.md#go)).
- `GO_LDFLAGS="-s -w"` — The variable upstream's `GNUmakefile` turns into the `-ldflags` of its
  `go build`. `-s -w` leave out the symbol table and the DWARF data, which kpkg does not strip from
  a Go binary. It is passed to the `install` run as well: upstream's `install` target depends on
  `all`, so a binary make rebuilds there is built with the same flags.
- `PREFIX=/usr` — Upstream's makefile installs under `/usr/local` unless told otherwise.

## Notes

The Go modules come from the vendor bundle, `direnv-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `go mod vendor` and which `build.sh` unpacks into the top of the source tree (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). The makefile's
`go build` names no `-mod=vendor`: with `vendor/modules.txt` present and a `go.mod` that asks for Go
1.14 or later, the go command builds from `vendor/` by default, and the build has no network to
reach a module proxy in any case.
