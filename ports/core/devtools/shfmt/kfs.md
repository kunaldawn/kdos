## About
[shfmt]{.idx} parses POSIX shell, bash, mksh, zsh and Bats scripts and prints them back in one consistent
format; `shfmt -d` shows the difference instead, and a script it cannot parse is reported with
the line and column. It comes from the `mvdan/sh` module, a Go shell parser, and is built with
cgo off from its vendor bundle. `testing/preflight.sh` checks every `build.sh` in the tree
with `bash -n`, which finds syntax errors and nothing else; shfmt parses each script into a
syntax tree, so it also reports what a syntax check passes, such as inconsistent formatting.

## Flags
- `-X main.version=v$version` — Meant to stamp the version into the program, but `cmd/shfmt`
  has no `main.version` variable, and the Go linker ignores an `-X` for a symbol that does not
  exist. `shfmt --version` prints the main module's version from the build information instead,
  which for a build from a source tree is `(devel)`.

## Notes
The manual page is written in scdoc, as `cmd/shfmt/shfmt.1.scd`, and [scdoc](#port-scdoc), in
`depends`, renders it to `shfmt.1` at install time.
