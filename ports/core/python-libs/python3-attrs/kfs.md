## About
[attrs]{.idx} writes the boilerplate of a Python class from declared attributes: `__init__`,
`__repr__`, comparison and hashing, with validators and converters on each attribute.
[python3-jsonschema](#port-python3-jsonschema), [python3-referencing](#port-python3-referencing),
[deluge](#port-deluge), [vdirsyncer](#port-vdirsyncer) and [spyder](#port-spyder) are among the
ports that import it.

## Notes
attrs builds with hatchling, hatch-vcs and hatch-fancy-pypi-readme. The first two are ports in
`depends`; the third, a hatchling plugin that assembles the long description, is not, and it
is the one entry of the recipe's `pypackages`. `build.sh` installs it from the bundle into
`$SRC_ROOT/backends` and puts that directory on `PYTHONPATH` for the build, so the plugin is
used and nothing of it lands in the build root or the package; see
[The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys). The other sdists in the bundle are the
plugin's own build requirements, which this build does not use, because both installs run with
`--no-build-isolation`.
