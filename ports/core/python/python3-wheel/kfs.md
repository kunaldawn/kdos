## About
The reference implementation of the Python [wheel]{.idx} format: the `wheel` package and
command, which unpack, repack, retag and inspect `.whl` files and convert eggs and Windows
installers to wheels. It is a build dependency of many ports. [qemu](#port-qemu) is one: its
`pythondeps.toml` lists `wheel` in its `tooling` group of Python dependencies, beside
`qemu.qmp`.

## Notes
The install runs with pip's build isolation on. wheel builds with flit_core
(`requires = ["flit_core >=3.11,<5"]`), and pip installs it into the temporary build environment
from the vendor bundle (`--no-index --find-links=vendor`); flit_core needs nothing else to build
itself. The bundle also carries `packaging`, wheel's one run-time dependency, which neither the
build environment nor the package takes from it: `--no-deps` leaves it out, and
[python3-packaging](#port-python3-packaging) in `depends` provides it. Nothing from the bundle is
installed into the package.
