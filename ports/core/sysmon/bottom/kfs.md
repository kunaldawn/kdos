## About

bottom, run as `btm`, is a terminal system monitor with graphs of processor, memory, network and
disk use beside a process table. The package adds a desktop entry, *Resource Monitor*, that opens
it in a terminal from the launcher.

## Flags

- `BTM_GENERATE=true` — Make bottom's `build.rs` write the manual page and the shell completions
  while cargo builds the program, under `target/tmp/bottom/`. Without it the build script writes
  nothing. `build.sh` installs the manual page, `btm.1`, from there; the generated completions are
  not installed.

## Notes

The crates come from the vendor bundle, `bottom-vendor-<version>.tar.xz`, made by `ports/fetch`
with `cargo vendor` and unpacked into the top of the source tree, where cargo finds its
configuration; the build runs `--frozen --offline` (see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring)). cargo installs no
desktop entry, and upstream's `desktop/bottom.desktop` is left in the source tree; the entry the
package carries is the recipe's own. See
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port).
