## About
[Crubit]{.idx} is Google's tool set for calls between C++ and Rust. This port builds one program
from it, `cc_bindings_from_rs`, which reads a compiled Rust crate and writes the C++ headers
through which C++ code calls it, and installs the C++ support library those headers include, with
Crubit's GN rules and licence, under `/usr/share/crubit`. [chromium](#port-chromium) is its one
consumer: its `build.sh` links the program into `third_party/rust-toolchain/bin` and
`/usr/share/crubit` into `third_party/rust-toolchain/lib/third_party/crubit`, the Rust toolchain
directory Chromium's build reads, beside `rustc` and `cargo`.

The source is a commit, not a release, and Crubit's own build is Bazel. The `cargo/` tree in it
holds generated `Cargo.toml` files, so cargo can build the tool from its vendor bundle, the second
`sha256 =` line. The bundle carries `vendor/`, `Cargo.lock` and `.cargo/config.toml`; it is
unpacked at the top of the tree and cargo runs from there, which is where it finds that
configuration (see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes)).

## Flags
- `-C target-feature=-crt-static` — link against musl dynamically rather than statically, the rule
  for every cargo build in the tree (see
  [rust](../../../../docs/kdos/05-developer/writing-ports.md#rust)). Here a static link is not even
  possible: the program links the compiler's own `librustc_driver-<hash>.so`.
- `RUSTC_BOOTSTRAP=1` — let the stable compiler accept the nightly-only `#![feature(...)]`
  attributes the program's source declares, `rustc_private` and `never_type`. `rustc_private` is
  what gives it the compiler's internal crates, through which it reads a crate's types.
- `--bin cc_bindings_from_rs` — build that one binary target and nothing else of the manifest.
- `--manifest-path` — the generated manifest of the tool,
  `cargo/cc_bindings_from_rs/cc_bindings_from_rs/Cargo.toml`. The `Cargo.toml` at the top of the
  tree is a workspace manifest with no package of its own; cargo still reads it, as the workspace
  this manifest belongs to, for the shared dependency versions.

## Notes
`cc_bindings_from_rs` runs only with the compiler library it was linked against: its `NEEDED`
entries name `librustc_driver-<hash>.so`, whose hash identifies one build of the
[rust](#port-rust) port. A new version of `rust` leaves the installed program unable to start
until this port is rebuilt against it.

The commit in `_commit` is meant to be the `CRUBIT_REVISION` that Chromium's
`tools/rust/update_rust.py` names. The headers the tool writes include the support library
installed beside it, and Chromium's build is written against that revision of both, so a version
change of the `chromium` port reads that line from the new release and moves `_commit` with it.
