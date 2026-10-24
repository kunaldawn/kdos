## About
[python-OBD]{.idx} (`obd`) reads live sensor data and diagnostic trouble codes from a vehicle's
OBD-II port, and clears the codes, through an ELM327-type adapter, which is a USB serial device;
see [The device groups and the udev rules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).
Every decoded value is a pint quantity with its unit, so `import obd` imports pint.

## Notes
pint and its two helpers, flexcache and flexparser, have no other consumer in the tree. They are
the vendor bundle's `pypackages`, with pint pinned there to a release within the
`pint==0.24.*` that obd declares, and installed by name into this package with `--no-deps`. Their own runtime
dependencies, platformdirs and typing-extensions, and the build backends are ports in `depends`,
which is why the build runs with `--no-build-isolation`.

The bundle is unpacked into `$SRC_ROOT/vendor`, beside the source tree and not inside it. obd
keeps its package at the top of the tree and lists no packages, so setuptools discovers them by
itself and would take a `vendor` directory there for a second top-level package and refuse to
build; see [Where the bundle goes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
