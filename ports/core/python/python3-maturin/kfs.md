## About
[maturin]{.idx} is the build backend for Python packages written in Rust with PyO3 or cffi
bindings, and for Rust programs shipped as Python distributions. pip calls it through PEP 517 to
build [python3-cryptography](#port-python3-cryptography),
[python3-pydantic-core](#port-python3-pydantic-core), [python3-rpds-py](#port-python3-rpds-py),
[python3-jellyfish](#port-python3-jellyfish), [python3-orjson](#port-python3-orjson) and
[ruff](#port-ruff). It is itself a Rust program, so it is built from its vendor bundle.

maturin's source distribution builds itself with setuptools and
[python3-setuptools-rust](#port-python3-setuptools-rust): its `pyproject.toml` names
`build-backend = "bootstrap"` with `backend-path = ["maturin"]`, and its `setup.py` runs cargo
to compile the `maturin` binary into the wheel.

## Flags
- `MATURIN_SETUP_ARGS` — The cargo arguments `setup.py` builds the binary with. Unset, they are
  `--no-default-features`, a minimal build; set, the variable replaces that list whole. The
  value names upstream's default features less `xwin`, the cross-linker for Windows' MSVC
  target: `cli-completion` (shell completions), `scaffolding` (`maturin new` and `init`),
  `upload` with `rustls` for its TLS, `sbom` (a software bill of materials in the wheel),
  `auditwheel` (repairing a wheel's shared-library references) and `zig`, which lets `maturin build --zig` link through the [zig](#port-zig)
  port at run time.
