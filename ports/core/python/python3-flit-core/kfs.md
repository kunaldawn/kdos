## About
flit_core is the [build backend]{.idx} of Flit: the code pip calls, through the PEP 517
interface, to turn a Python project's source tree into a wheel. It needs nothing outside the
Python standard library; for an interpreter without `tomllib` it carries a TOML reader, tomli, in
its own `vendor` directory. It can therefore be built and installed with no other backend
present.

That makes it the first backend in the chain `30_foundation` builds after
[python3-setuptools](#port-python3-setuptools) and [python3-pip](#port-python3-pip).
[python3-packaging](#port-python3-packaging) and [python3-pathspec](#port-python3-pathspec)
build with it, and both are part of what [python3-hatchling](#port-python3-hatchling) needs.
Later phases build many more `python3-*` ports with it, among them python3-click,
python3-idna, python3-markdown-it-py and python3-typing-extensions.

## Flags
- `--no-build-isolation` — Run the build backend in the build root's own Python environment
  instead of in a temporary one that pip fills with the declared build requirements. Here the
  build needs nothing: flit_core's `pyproject.toml` declares no build requirement and sets
  `backend-path = ["."]`, so the backend that builds the wheel is the copy of flit_core in the
  source being packaged.

## Notes
A source distribution may cap its backend's major version below this port's.
[python3-docutils](#port-python3-docutils) and [python3-jinja2](#port-python3-jinja2) do, and
they build in isolation from an older flit_core that their vendor bundles carry. A build with
`--no-build-isolation` would not notice such a cap: pip compares the installed backend with the
declared requirement only when it is also given `--check-build-dependencies`.
