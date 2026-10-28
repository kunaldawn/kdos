## About

micro is a terminal text editor with the keys of a desktop editor: `Ctrl-S` saves, `Ctrl-Q` quits,
`Ctrl-C` and `Ctrl-V` copy and paste, and the mouse selects, alongside syntax highlighting, multiple
cursors, split panes and Lua plugins. It is a single statically linked Go program, built here with
upstream's `make build` from a vendored module bundle. The port installs it with its manual page and
a desktop entry, *Editor (micro)*, which opens it in a terminal and claims plain text and common
source-file types.

## Flags

- `VERSION=$version` — The version compiled into the program, which `micro --version` prints.
  Upstream's makefile computes it with `tools/build-version.go`, which asks `git describe` and
  answers `0.0.0-unknown` when the source is not a git checkout.
- `HASH=` — The commit hash compiled in beside it, which the makefile takes from `git rev-parse`;
  there is no repository, so it is set empty.

## Notes

- **`make build` is two steps.** It runs `go generate ./runtime`, which writes the header files of
  the syntax definitions under `runtime/syntax` (the program embeds that directory, the colour
  schemes, the help pages and the plugins with `go:embed`), then `build-quick`, which is `go build
  -trimpath -ldflags "-s -w …" ./cmd/micro`. The makefile passes `-s -w` itself, which the tree
  requires of every Go build (see
  [go](../../../../docs/kdos/05-developer/writing-ports.md#go)). It
  names no `-mod=vendor`: Go uses the `vendor/` directory the bundle unpacks on its own, because
  `go.mod` declares a Go version from 1.14 on.
- **The build date is the source date.** The makefile's `DATE` comes from `tools/build-date.go`,
  which uses `SOURCE_DATE_EPOCH` when it is set, as it is for every build here, and the current time
  otherwise.
- **The desktop entry is the recipe's own.** `build.sh` writes `micro.desktop`; upstream's
  `assets/packaging/micro.desktop` is not installed.
