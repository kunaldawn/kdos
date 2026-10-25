## About
fzf is a fuzzy finder: it reads a list of lines, lets you narrow it by typing, and prints the
lines you pick, so any command's output becomes a menu. The port installs `fzf`, the `fzf-tmux`
wrapper, both manual pages, and the bash and zsh key bindings and completion scripts under
`/usr/share/fzf`, which a shell's start-up file sources to get `Ctrl-R` history search,
`Ctrl-T` file insertion and `**` completion.

## Flags
- `-X main.version=$version` — Stamp the release into the binary, so `fzf --version` reports it.
  The source carries only the major and minor version.

## Notes
fzf is a pure Go program built with cgo off from a vendor bundle, `fzf-vendor-<version>.tar.xz`,
which `ports/fetch` makes with `go mod vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring) and the
[go build shape](../../../../docs/kdos/05-developer/writing-ports.md#go). The `go build` line
passes no `-mod=vendor`: when a `vendor/` directory is present and `go.mod` declares Go 1.14 or
later, the go command builds from it by default.
