## About
[pydantic-core]{.idx} is the validation and serialisation engine of pydantic, written in Rust
and built by [maturin](#port-python3-maturin) from its vendor bundle. pydantic itself, the pure
Python half, is not a port: [ocrmypdf](#port-ocrmypdf) carries it in its own vendor bundle and
depends on this port for the compiled part.

## Notes
pydantic pins pydantic-core to one exact version (`pydantic-core==<version>` in its
`pyproject.toml`), and `import pydantic` raises `SystemError` when the installed one is any
other. `series` holds this port at the
version that the pydantic in ocrmypdf's `pypackages` names, so a bump of either is a bump of
both; see [Filtering](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#filtering).
