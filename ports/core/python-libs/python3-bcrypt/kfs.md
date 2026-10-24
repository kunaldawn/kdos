## About
[bcrypt]{.idx} is the bcrypt password hash for Python, implemented in Rust and built with
setuptools and [python3-setuptools-rust](#port-python3-setuptools-rust) from the vendor bundle.
[spyder](#port-spyder) depends on it.

## Notes
The Rust crate is `src/_bcrypt/Cargo.toml`, the `path` in the package's
`[[tool.setuptools-rust.ext-modules]]`, with its own `Cargo.lock` beside it, so the recipe's
`vendordir = src/_bcrypt` makes `ports/fetch` vendor there. `build.sh` unpacks the bundle at
the top of the tree, where pip, and through it setuptools-rust, runs cargo, so the bundle's
`.cargo/config.toml` is found by cargo's walk up from that directory; see
[Where the bundle goes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
