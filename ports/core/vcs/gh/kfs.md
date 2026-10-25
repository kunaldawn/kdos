## About
[gh]{.idx} is GitHub's command-line client: pull requests, issues, releases, Actions runs and
repositories from a terminal, and `gh auth`, which signs in to GitHub and can act as git's
credential helper for it. It is a Go program built with cgo off from its vendor bundle, the second
`sha256 =` line, which `ports/fetch` generates with `go mod vendor` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

Upstream builds it through `make`, whose targets run `script/build.go`; `bin/gh` builds the
program and `manpages` writes the manual pages under `share/man/man1`, which the port installs.

## Flags
- `GO_LDFLAGS="-s -w"` — extra linker flags for `script/build.go`, which puts them after the
  `-X` flags it writes itself into the one `-ldflags` it passes to `go build`. This is how `-s -w`
  reaches a Go link that a makefile runs (see
  [go](../../../../docs/kdos/05-developer/writing-ports.md#go)).
- `GH_VERSION=$version` — the version `script/build.go` stamps into `internal/build.Version`, which
  `gh --version` prints. Unset, it asks `git describe --tags`, and a release archive is no git
  repository.
- `HOME="$PWD/.home"` — together with `GH_CONFIG_DIR`, points the configuration gh reads at a
  directory inside the build tree while `bin/gh completion` writes the bash, zsh and fish
  completions, so generating them reads and writes nothing of the builder's own.

## Notes
The date `gh --version` prints is not the day of the build: `script/build.go` takes it from
`SOURCE_DATE_EPOCH`, which the tree pins (see
[Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages)).
