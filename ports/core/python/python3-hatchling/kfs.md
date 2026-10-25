## About
hatchling is the build backend of the Hatch project: a project declaring `build-backend = "hatchling.build"` is built by it, with plugins for
version sources, build hooks and file selection.

Much of `30_foundation`'s Python block is its closure. Its run-time dependencies are [python3-packaging](#port-python3-packaging),
[python3-pathspec](#port-python3-pathspec), [python3-pluggy](#port-python3-pluggy),
[python3-tomlkit](#port-python3-tomlkit) and
[python3-trove-classifiers](#port-python3-trove-classifiers), and those in turn need flit_core,
setuptools-scm, poetry-core and calver to build. With it in place,
[python3-hatch-vcs](#port-python3-hatch-vcs), [python3-sphinx](#port-python3-sphinx)'s closure
and, in later phases, python3-scikit-build-core, the Jupyter modules, calibre, deluge, khal and
radicale can be built.

## Flags
- `--no-build-isolation` — Build in the build root's own Python environment rather than in a
  temporary one that pip fills. hatchling builds itself: its `pyproject.toml` declares no build
  requirement and names `hatchling.ouroboros`, from the source's own `src`, as the backend. That
  module reports hatchling's run-time dependencies as its build requirements. With no isolation
  and no index, pip installs none of them, so they must already be in the build root, which
  `depends` sees to.
