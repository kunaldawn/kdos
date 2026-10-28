## About
trove-classifiers is the canonical list of the classifier strings PyPI accepts in a package's
metadata, such as `License :: OSI Approved :: MIT License`, as a Python module, with a
`trove-classifiers` command that prints it.

In KDOS it is in `30_foundation` because [python3-hatchling](#port-python3-hatchling) checks a
project's declared classifiers against this list when it builds the project's metadata.

## Flags
- `--no-build-isolation` — Build with what the build root already holds rather than with
  requirements pip would fetch into a temporary environment. trove-classifiers builds with
  setuptools and takes its version through calver (`requires = ["setuptools", "calver"]`), so
  [python3-calver](#port-python3-calver) is in `depends`.

## Notes
`setup.py` passes `use_calver="%Y.%m.%d.%H"`, but the source distribution carries `PKG-INFO`, and
calver reads the version from that first, so the installed version is the release's, not the
date of the build (see [python3-calver](#port-python3-calver)).

hatchling stops with ``Unknown classifier in field `project.classifiers`: `` when a project it
builds declares a classifier this list does not contain, other than a `Private ::` one, unless
`HATCH_METADATA_CLASSIFIERS_NO_VERIFY` is set in the environment. A project that uses a
classifier newer than this version of the list therefore needs this port bumped, or the variable
set in its `build.sh`.
