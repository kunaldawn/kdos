## About
eza lists files, as `ls` does, with colours by file type, a long view with headers, a tree view,
icons, extended attributes, and each file's Git status in the long view. The port installs `eza`,
its manual pages and completions for bash, zsh and fish.

## Flags
- `-M source=v$version` — Set the `source` field of each manual page's header, where `man` shows
  the version. The Markdown pages carry the placeholder `$version` in their title line, which
  upstream's `justfile` replaces with `sed` before converting with pandoc; the recipe converts
  with [lowdown](#port-lowdown) (`-s -Tman`) and gives it the version as metadata instead.

## Notes
eza is built from a vendor bundle, `eza-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
The Git status comes from upstream's default `git` feature through the `git2` crate, which the
tree's environment links against the system [libgit2](#port-libgit2). `build.sh` checks the binary
NEEDs `libgit2`, so a crate that compiles its bundled copy instead stops the build.
