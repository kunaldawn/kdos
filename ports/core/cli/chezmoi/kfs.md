## About
chezmoi manages dotfiles across machines: it keeps the wanted state of your home directory's
configuration files in a source directory, usually a Git repository, and applies it, with
templates for what differs between machines and encrypted files for secrets.

## Flags
- `-tags noupgrade` — Build without the `chezmoi upgrade` command, which asks the GitHub API for
  the newest release, downloads it and installs it the way it judges chezmoi was installed.
  Upstream's page for the command warns that a package manager's build may leave it out.
- `-X main.version=$version`, `-X main.builtBy=kdos` — Stamp the release and the builder into the
  binary; `chezmoi --version` prints both, as `built by kdos`.

## Notes
chezmoi is a pure Go program, built with cgo off from a vendor bundle,
`chezmoi-vendor-<version>.tar.xz`, which `ports/fetch` makes with `go mod vendor`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
