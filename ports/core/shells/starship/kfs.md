## About

Starship draws the shell prompt. The system `bash.bashrc` runs `starship init bash` whenever
`starship` is installed and falls back to a plain prompt otherwise; the prompt reads
`~/.config/starship.toml`, whose palette block `kdos theme` rewrites with the active colour scheme
(see [Theming](../../../../docs/kdos/02-user-guide/theming.md#what-changes-when)). The copy a new
account gets from `/etc/skel` gives starship's `container` module a format, so a shell inside a box shows
the container's name in the prompt.

## Notes

The crates come from the vendor bundle, `starship-vendor-<version>.tar.xz`, which `ports/fetch`
makes with `cargo vendor` and which carries the `vendor/` directory, the `.cargo/config.toml` that
points cargo at it, and the `Cargo.lock` it was resolved against (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). `build.sh` unpacks it
into the top of the source tree, where cargo runs and finds the configuration, and builds with
`--frozen --offline`, so a crate missing from the bundle stops the build instead of being fetched.
The `RUSTFLAGS` line is the tree's rule for every Rust port: `-crt-static` links the program against
the system's shared musl rather than statically (see
[rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)).
