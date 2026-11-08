## About

Incremental is the Twisted project's version library: a `Version` object that formats CalVer and
release-candidate numbers, the `incremental` command that writes a project's `_version.py`, and
plugins that hand the version to setuptools and to hatchling. It is on the `python` shelf
because it is build tooling: [python3-twisted](#port-python3-twisted) names it as the source of
its version (`[tool.hatch.version] source = "incremental"`), and hatchling loads the plugin
through the `hatch` entry point.

## Notes

Incremental builds with [python3-hatchling](#port-python3-hatchling) and reads its own version
from `src/incremental/_version.py`. At run time it imports
[python3-packaging](#port-python3-packaging), which is in `depends`.
