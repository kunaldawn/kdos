## About
[git-cola]{.idx} is a Git GUI for staging, committing, branching and reviewing changes, and
`git-dag`, installed with it, draws a repository's history as a graph. Both run the
[git](#port-git) command for every operation. They are Python programs on Qt, reached through
[QtPy](#port-python3-qtpy), which loads whichever Qt binding it finds; the binding the recipe
depends on is [PyQt6](#port-python3-pyqt6).

## Flags
- `SETUPTOOLS_SCM_PRETEND_VERSION` — setuptools_scm takes the version from this variable instead
  of working it out from Git metadata or the source distribution's `PKG-INFO`. It is set to the
  recipe's `$version`, so the installed package reports the release the recipe names.

## Notes
- **The vendor bundle** carries one run-time module, `polib`, which `pyproject.toml` requires and
  which is not a port; `cola/i18n.py` imports it to read git-cola's translation catalogues, and
  falls back to its own copy, `cola/polib.py`, when it is missing. The rest of the bundle,
  setuptools, setuptools_scm and the packages they are built with, is the build-requirement chain
  `ports/fetch` collects from each `pyproject.toml`. `build.sh` passes `--no-build-isolation`, so
  pip builds polib and git-cola with the [setuptools](#port-python3-setuptools) and
  [setuptools-scm](#port-python3-setuptools-scm) ports in `depends` and installs none of those
  archives. `extras/` holds the `sphinxtogithub` Sphinx extension and the licences
  of polib and QtPy, and is not installed.
- **The manual pages** for `git-cola` and `git-dag` are built from the Sphinx sources in `docs/`
  with [python3-sphinx](#port-python3-sphinx). `docs/conf.py` puts `extras/` on `sys.path`,
  because the `sphinxtogithub` extension it loads lives there.
- **The menu icon.** Upstream ships `git-cola.svg` only, and the panel reads hicolor PNGs, so
  `build.sh` renders 48, 64, 128 and 256 pixel PNGs with `rsvg-convert`.
- **The menu entries are written by `build.sh`.** Upstream installs its `.desktop` files from its
  Makefile, not from the Python package that `pip` builds. Both programs set the desktop file
  name, and so the Wayland app_id, to `git-cola`, which is the `StartupWMClass` of both entries.
  Upstream's third entry, `git-cola-folder-handler.desktop`, is left out: it claims
  `inode/directory`, the file manager's type.
