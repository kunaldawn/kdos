## About
The [Go]{.idx} toolchain: the `go` command, `gofmt`, the compiler, linker and assembler under
`pkg/tool`, and the standard library as source. Many ports are Go programs built with it, among
them [podman](#port-podman), [buildah](#port-buildah) and [skopeo](#port-skopeo),
[syncthing](#port-syncthing), [restic](#port-restic), [rclone](#port-rclone), [gh](#port-gh),
[git-lfs](#port-git-lfs), [fzf](#port-fzf), [caddy](#port-caddy), [esbuild](#port-esbuild) and the
language server [gopls](#port-gopls), and [chromium](#port-chromium) needs it to build.
How a Go port is written, and which flags every Go build in the tree gets, is in
[go](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#go).

Go's compiler is written in Go, so the build starts from upstream's prebuilt `linux-amd64`
release, the second `source =` line, which unpacks beside the source as `$SRC_ROOT/go`. That
release is a [bootstrap seed]{.idx}: `make.bash` compiles the new toolchain with it, and nothing of
the seed is installed. The tree's list of seeds is in
[Bootstrap seeds](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/why-kdos.md#bootstrap-seeds).

The toolchain is installed whole into `/usr/lib/go`, which is its `GOROOT`, with `go` and `gofmt`
linked into `/usr/bin`. The `go` command finds its `GOROOT` from the path of its own executable,
so the links in `/usr/bin` reach the tree under `/usr/lib/go`.

## Flags
- `GOROOT_BOOTSTRAP` — The Go tree `make.bash` compiles the new toolchain with: the unpacked
  bootstrap release.
- `GOROOT_FINAL` — This release of Go does not read the variable: neither `make.bash` nor
  `cmd/dist` refers to it. The installed toolchain locates `/usr/lib/go` from the path of its own
  binary, as described above.
- `GOOS=linux` — The operating system the toolchain is built for and targets by default.
- `GOARCH=amd64` — The architecture the toolchain is built for and targets by default. `GOAMD64`
  stays unset, so the default instruction level is upstream's `v1`.
- `CGO_ENABLED=1` — Compiles cgo into the toolchain and records cgo as on in the installed
  toolchain's defaults (`cmd/dist` writes the value into `DefaultCGO_ENABLED`). A Go build on the
  system then uses cgo unless it sets `CGO_ENABLED=0`, which most Go ports in the tree do; a
  program that binds a C library leaves it on.
- `GO_LDFLAGS=-buildmode=pie` — Extra arguments for the Go linker when `make.bash` links the
  commands. The tree exports `-buildmode=pie` to every Go build in `GOFLAGS`, but `make.bash`
  clears `GOFLAGS`, so this is how `go`, `gofmt` and the tools under `pkg/tool` become
  position-independent executables like every other program in the tree (see
  [The release flags](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-release-flags)).

## Patches
- `go-env-toolchain-local.patch` — Changes `GOTOOLCHAIN=auto` to `GOTOOLCHAIN=local` in upstream's `go.env`, the file of defaults the `go` command reads from its `GOROOT`, and rewrites the comment above it to match. With `auto`, a `go.mod` that names a newer Go than the installed one makes `go` download and run that toolchain; with `local`, it uses the installed toolchain only, and such a module fails to build. The module proxy and checksum database lines are left as upstream has them.

## Notes
`cp -a bin pkg src lib api` does not copy the files at the top of the source tree, so `go.env`
and `VERSION` are installed by name after it. Without `go.env` in `GOROOT`, the patch above would
not reach the installed system.

The source archive carries object files that upstream compiled and that no step here rebuilds:
the race detector's runtime under `src/runtime/race` and the BoringCrypto module under
`src/crypto/internal/boring/syso`. The `find ... -delete` removes them from the package, so
`go build -race` and `GOEXPERIMENT=boringcrypto` fail to link on KDOS; the rule this follows is in
[What is built from source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
`pkg/obj` is `make.bash`'s scratch directory and is removed as well, and so is every `testdata`
directory under `src`, which only `go test std` reads.
