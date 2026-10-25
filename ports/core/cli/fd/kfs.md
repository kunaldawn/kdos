## About
fd finds files by name, a simpler and faster `find` for the common case: a regular expression or
glob against the name, a parallel walk of the tree, and `.gitignore` rules and hidden files
honoured by default. The port installs `fd`, its manual page and completions for bash, zsh and
fish.

## Flags
- `--features use-jemalloc` — Make jemalloc, compiled from the vendored `tikv-jemalloc-sys`
  crate, the program's allocator, in place of musl's `malloc`, which takes one global lock for
  every allocation while fd's parallel walker allocates from many threads. The feature is added to
  upstream's defaults, which build the completion generator.

## Notes
fd is built from a vendor bundle, `fd-vendor-<version>.tar.xz`, made by `ports/fetch` with
`cargo vendor`; see [Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring).
The jemalloc inside the binary is that crate's copy, not the [jemalloc](#port-jemalloc) port's
library.
