## About

cantools reads CAN bus database files (DBC, KCD, SYM, ARXML) and uses them to encode and decode
CAN frames as named signals. On KDOS it is the step after [can-utils](#port-can-utils):
`candump` prints raw frames, and `cantools decode` turns that output into signal names and values
against a database; `cantools generate_c_source` writes the C a microcontroller decodes the same
frames with, and `cantools plot` draws signals over time with [matplotlib](#port-matplotlib).

## Notes

The bundle `python3-cantools-vendor-<version>.tar.xz` holds the source distributions of the
modules only cantools uses, and of its build requirements: `argparse-addons`, `bitstruct`,
`crccheck`, `python-can` (with its own dependency `wrapt`) and `textparser`, and setuptools,
`setuptools_scm` and the backends they need. `ports/fetch` resolves the runtime set from the
package's own metadata (`pyrequirements = no`: not from a `requirements.txt`), so the bundle also
holds copies of `packaging` and `typing_extensions`, which are installed from their ports instead;
see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).

`pip3 install` names every distribution it installs and passes `--no-deps`, so the list in
`build.sh` is the whole closure; a module the bundle holds but the list leaves out is not
installed. The build is not given `--no-build-isolation`: pip sets up each package's build
requirements from the bundle alone (`--no-index --find-links=vendor`), which is why setuptools
and `setuptools_scm` are in it although both are also ports.
