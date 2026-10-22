## About
[cryptography]{.idx} is the Python package of cryptographic recipes and primitives (Fernet,
X.509, the hazmat layer). Its core is
Rust, bound to Python with PyO3 and to OpenSSL through the `openssl` crate, and built by
[maturin](#port-python3-maturin) from the vendor bundle; the compiled module
`cryptography/hazmat/bindings/_rust.abi3.so` links the system [openssl](#port-openssl)
(`libssl.so`, `libcrypto.so`). The rest of its OpenSSL binding is C that
[python3-cffi](#port-python3-cffi) generates from `src/_cffi_src` during the cargo build, and
the build script compiles it into the same module.

## Notes
The Rust crate that maturin builds is `src/rust/Cargo.toml` (`manifest-path` in the
package's `[tool.maturin]`), so the recipe's `vendordir = src/rust` makes `ports/fetch` vendor
from there. `build.sh` unpacks the bundle at the top of the tree all the same, because maturin
runs cargo from the top with `--manifest-path`, and cargo finds `.cargo/config.toml` by walking
up from the directory it runs in, not from the manifest; a configuration beside the manifest is
never read, and every crate then resolves as missing. See
[Where the bundle goes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
