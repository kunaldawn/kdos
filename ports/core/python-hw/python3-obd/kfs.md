## About
[python-OBD]{.idx} (`obd`) reads live sensor data and diagnostic trouble codes from a vehicle's
OBD-II port, and clears the codes, through an ELM327-type adapter on a serial port (a USB
adapter appears as one); see [The device groups and the udev rules](../../../../docs/kdos/02-user-guide/administration.md#the-device-groups-and-the-udev-rules).
Numeric sensor values are returned as pint quantities carrying their unit, so `import obd`
imports pint.

## Notes
pint and its two helpers, flexcache and flexparser, have no other consumer in the tree. They are
the vendor bundle's `pypackages`, with pint pinned there to a release within the
`pint==0.24.*` that obd declares, and installed by name into this package with `--no-deps`.
Their own runtime dependencies, platformdirs and typing-extensions, are ports in `depends`, and
so are their build requirements, setuptools and setuptools-scm (setuptools also builds obd), so
the build runs with `--no-build-isolation` against the installed ports. The three also name
`wheel` as a build requirement, which this setuptools does not need: it builds the wheel itself.
The bundle carries sdists of setuptools, setuptools_scm, wheel and their own requirements, which
`ports/fetch` collects as the build requirements; with build isolation off, pip uses none of them.

The bundle is unpacked into `$SRC_ROOT/vendor`, beside the source tree and not inside it. obd
keeps its package at the top of the tree and lists no packages, so setuptools discovers them by
itself and would take a `vendor` directory there for a second top-level package and refuse to
build; see [Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
