## About
pluggy is the plugin framework at the core of pytest, tox and devpi: a host program declares
hook specifications, and plugins register implementations that pluggy calls and whose results
it collects.

In KDOS it is how [python3-hatchling](#port-python3-hatchling) loads its build hooks, version
sources and builder plugins, which is why it is in `30_foundation` with the rest of the
packaging chain. [ocrmypdf](#port-ocrmypdf) uses it for its plugins, and
[python3-lsp-server](#port-python3-lsp-server) depends on it too.

## Flags
- `--no-build-isolation` — Build with what the build root already holds rather than with
  requirements pip would fetch into a temporary environment. pluggy builds with setuptools and
  takes its version from setuptools-scm (`requires = ["setuptools>=65.0",
  "setuptools-scm[toml]>=8.0"]`), so [python3-setuptools-scm](#port-python3-setuptools-scm) is
  in `depends` and is built first.
