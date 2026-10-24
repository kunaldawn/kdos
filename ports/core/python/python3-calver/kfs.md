## About
calver is a setuptools extension that sets a package's version to a calendar date. A project
passes `use_calver=True`, or a `strftime` format, to `setup()` and lists calver among its build
requirements.

In KDOS its one use in the packaging chain is
[python3-trove-classifiers](#port-python3-trove-classifiers), whose `setup.py` passes
`use_calver="%Y.%m.%d.%H"`. jupyterlab, spyder, python3-nbconvert and python3-lsp-server name
it in `depends` as well.

## Flags
- `--no-build-isolation` — Build with the setuptools already installed in the build root rather
  than one pip would fetch into a temporary environment. calver's own build needs only
  setuptools (`requires = ["setuptools>=77.0.1"]`), named in `depends`.

## Notes
calver takes the version from the `PKG-INFO` file in the current directory when there is one,
and only otherwise from the date. That date is `SOURCE_DATE_EPOCH` when it is set, and the clock when
it is not; the build environment, `script/env/common.env`, exports a fixed `SOURCE_DATE_EPOCH`
for every port. A port built from a PyPI source
distribution, which carries `PKG-INFO`, therefore gets the release's own version whatever day it
is built.
