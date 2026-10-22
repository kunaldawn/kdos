## About
tomlkit reads and writes TOML while keeping the document's comments, ordering and formatting,
so a program can change one value in a file a person wrote and leave the rest as it was.

In KDOS it is in `30_foundation` because [python3-hatchling](#port-python3-hatchling) depends on
it: `hatchling version` uses it to rewrite the version in a project's `pyproject.toml`. [picard](#port-picard), [toot](#port-toot), [spyder](#port-spyder) and
[python3-lsp-server](#port-python3-lsp-server) depend on it at run time.

## Flags
- `--no-build-isolation` — Build with the backend already installed in the build root rather
  than one pip would fetch into a temporary environment. tomlkit builds with poetry-core
  (`requires = ["poetry-core>=1.0.0a9"]`), so [python3-poetry-core](#port-python3-poetry-core)
  is in `depends`.
