## About
[cbindgen]{.idx} goes the other way from [bindgen](#port-bindgen): it reads Rust source and writes
the C and C++ headers through which C code calls a Rust library. It is a build tool for the ports
that mix the two languages: [firefox-esr](#port-firefox-esr), [librewolf](#port-librewolf) and
[thunderbird](#port-thunderbird) generate their headers for Gecko's Rust components with it, and
[mesa](#port-mesa) for its Rust code.

It is a Rust program, built with cargo from its vendor bundle, the second `sha256 =` line. The
bundle carries the crates under `vendor/`, the `Cargo.lock` they were resolved from and a
`.cargo/config.toml` that points cargo's crates.io source at `vendor/`, so `--frozen --offline`
builds it with no network (see
[Vendoring](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#vendoring)).

## Flags
- `-C target-feature=-crt-static` — Links `cbindgen` against musl dynamically instead of
  statically, which Rust's musl target does by default; every cargo build in the tree sets it (see
  [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)).
