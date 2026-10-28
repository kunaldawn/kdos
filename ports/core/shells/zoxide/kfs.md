## About

zoxide is a `cd` that remembers which directories are used and how recently, and jumps to the best
match for a few letters. The system `bash.bashrc` runs `zoxide init bash --cmd j`, so the jump
command is `j` (see
[Shell environment](../../../../docs/kdos/06-reference/configuration.md#shell-environment)), and
the file chooser's *Recent directories* group reads the same database through `zoxide`
(see [kdos-shell](../../../../docs/kdos/04-programs/kdos-shell.md#places)).

## Notes

The crates come from the vendor bundle, `zoxide-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor` and unpacked into the top of the source tree, where cargo finds its
configuration; the build runs `--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). cargo builds only
the binary, so `build.sh` copies the manual pages and the bash, zsh and fish completions from the
source tree to the places each shell reads them.
