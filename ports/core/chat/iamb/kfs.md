## About
iamb is a Matrix chat client for the terminal, with vim's keys: rooms, spaces and threads,
end-to-end encryption, reactions and edits, and image previews (as pictures in a terminal that
can show them, as coloured blocks in one that cannot), all through the Rust Matrix SDK. The port
installs the program, its two manual pages, `iamb(1)` and the configuration page `iamb(5)`, and a desktop entry
that opens it in a terminal as *Matrix*.

## Notes
iamb is a Rust program built from a vendor bundle, `iamb-vendor-<version>.tar.xz`, which
`ports/fetch` makes with `cargo vendor` and the recipe pins by its `sha256`; see
[Vendoring](../../../../docs/kdos/05-developer/writing-ports.md#vendoring) and the
[rust build shape](../../../../docs/kdos/05-developer/writing-ports.md#rust).

The Matrix SDK keeps its state in SQLite through `libsqlite3-sys`, which the tree's environment
points at the system library. `build.sh` checks that the binary NEEDs `libsqlite3.so.0` after
`cargo build`, so a crate that falls back to compiling its own SQLite stops the build instead of
shipping a private copy.

TLS has no such switch. The SDK uses rustls with its default provider, AWS-LC, which `aws-lc-sys`
compiles into the binary. That copy has a row in `src/system/kdos-tools/secdb/bundled.txt`, so
`kdos cve` still sees its version; see
[Bundled copies](../../../../docs/kdos/03-architecture/packaging.md#bundled-copies). A version bump
that moves `aws-lc-sys` changes that row too.
