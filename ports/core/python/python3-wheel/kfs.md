## About
The reference implementation of the Python [wheel]{.idx} format: the `wheel` package and
command, which pack, unpack, inspect and retag `.whl` files. It is a build dependency of many
ports. [qemu](#port-qemu) is one: its `pythondeps.toml` lists `wheel` in its `tooling` group of
Python dependencies, beside `qemu.qmp`.

## Notes
The install runs with pip's build isolation on. wheel builds with flit_core, and the vendor
bundle carries `flit_core` and `packaging`, from which pip sets up the temporary build
environment (`--no-index --find-links=vendor`); nothing from the bundle is installed into the
package.
