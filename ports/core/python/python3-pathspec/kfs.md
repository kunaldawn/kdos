## About
pathspec matches file paths against patterns in the syntax of `.gitignore` and similar files.
Build backends use it to decide which files of a source tree go into a wheel or a source
distribution.

In KDOS, [python3-hatchling](#port-python3-hatchling) and
[python3-scikit-build-core](#port-python3-scikit-build-core) use it for exactly that, and
[python3-lsp-server](#port-python3-lsp-server) depends on it as well.

## Flags
- `--no-build-isolation` — Build with the backend already installed in the build root rather
  than one pip would fetch into a temporary environment. pathspec builds with flit_core
  (`requires = ["flit_core >=3.2,<5"]`), so [python3-flit-core](#port-python3-flit-core) is in
  `depends`.
