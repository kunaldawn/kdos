## About
packaging is the PyPA's implementation of the interoperability specifications for Python
packages: version numbers and their ordering (PEP 440), requirement strings and environment
markers (PEP 508), wheel tags and core metadata. Build backends and installers use it to read
and compare what a project declares.

In KDOS it is part of the Python packaging chain in `30_foundation`.
[python3-vcs-versioning](#port-python3-vcs-versioning) and
[python3-setuptools-scm](#port-python3-setuptools-scm) need it, and so does
[python3-hatchling](#port-python3-hatchling). Outside that chain, [python3-meson-python](#port-python3-meson-python),
[python3-scikit-build-core](#port-python3-scikit-build-core) and programs such as
[matplotlib](#port-matplotlib), [inkscape](#port-inkscape) and [freecad](#port-freecad)
depend on it.

## Flags
- `--no-build-isolation` — Build with the backend already installed in the build root rather
  than one pip would fetch into a temporary environment. packaging builds with flit_core
  (`requires = ["flit_core >=3.12"]`), which is why [python3-flit-core](#port-python3-flit-core)
  is in `depends`.
