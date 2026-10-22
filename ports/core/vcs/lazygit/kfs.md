## About
[lazygit]{.idx} is a terminal interface to git: it stages files and single lines, commits,
branches, rebases interactively and resolves conflicts from panels driven by the keyboard, running
the `git` command underneath, which is why [git](#port-git) is in `depends`. Where
[tig](#port-tig) reads a repository, lazygit acts on one.

It is a Go program built with cgo off from its vendor bundle, the second `sha256 =` line, which
`ports/fetch` generates with `go mod vendor` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `-X main.version=$version` — set the `version` variable of lazygit's `main` package at link
  time, which `lazygit --version` prints. Without it lazygit falls back to the commit Go records
  from version control, and `-buildvcs=false` in the exported `GOFLAGS` records none, so it would
  report `unversioned`.
  It shares one `-ldflags` string with `-s -w`, because a second `-ldflags` would replace the
  first (see [go](../../../../docs/kdos/05-developer/writing-ports.md#go)).

## Notes
The release archive already carries a `vendor/` tree, and the bundle is unpacked over it. `go
build` is given no `-mod=vendor`: when a `vendor/modules.txt` is present and `go.mod` declares a Go
version of 1.14 or later, as lazygit's does, the go command builds from `vendor/` by default.

`build.sh` writes the package's own desktop entry, `lazygit.desktop`, named *Git* in the menu, with
`Terminal=true`, so the launcher opens lazygit in the session's terminal (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
