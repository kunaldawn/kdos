## About
[cloudpickle]{.idx} extends Python's `pickle` to functions, lambdas and classes defined
interactively, which plain `pickle` refuses. [spyder](#port-spyder)'s kernel sends the values of
the variables it shows across to the editor with it.

## Patches
- `flit-project-table.patch` — Moves the package's metadata in `pyproject.toml` from flit's `[tool.flit.metadata]` table to the standard `[project]` table, with the version and description read from the module (`dynamic = ["version", "description"]`), the home page under `[project.urls]`, and the build requirement pinned to `flit_core >=3.2,<5`. It also drops the `License :: OSI Approved` classifier. The [python3-flit-core](#port-python3-flit-core) port refuses both the old table and a license classifier beside a `license` expression, so without the patch the build stops before anything is built.
