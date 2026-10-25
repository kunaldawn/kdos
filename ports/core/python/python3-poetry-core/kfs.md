## About
poetry-core is the build backend of Poetry, a self-contained package that lets a PEP 517
frontend such as pip build a project declaring `build-backend = "poetry.core.masonry.api"`
without Poetry itself. It carries its own copies of the libraries it needs (fastjsonschema, lark, packaging
and tomli, under `poetry/core/_vendor`), so its only dependency is the interpreter.

In KDOS, [python3-tomlkit](#port-python3-tomlkit) builds with it in `30_foundation`, and later
phases build python3-rich, python3-qrcode, python3-pkgconfig, vdirsyncer, cura and others
with it.

## Flags
- `--no-build-isolation` — Run the backend in the build root's own Python environment instead of
  a temporary one. poetry-core declares no build requirement and sets `backend-path = ["src"]`,
  so it is built by the copy of itself in the source being packaged.
