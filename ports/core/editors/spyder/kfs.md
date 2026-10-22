## About

Spyder is the scientific Python IDE: an editor with code analysis through
[python3-lsp-server](#port-python3-lsp-server), an IPython console, a variable explorer, a plots
pane and a help pane that renders docstrings with [python3-sphinx](#port-python3-sphinx). It is a
PyQt6 program, built from one vendor bundle holding the Python packages that only Spyder uses.

## Flags

- `SPYDER_QT_BINDING=pyqt6` — Make `setup.py` require PyQt6 and its WebEngine binding, the ones
  [python3-pyqt6](#port-python3-pyqt6) and [python3-pyqt6-webengine](#port-python3-pyqt6-webengine)
  provide. Unset, it requires PyQt5, which is not installed.
- `SODIUM_INSTALL=system` — Build PyNaCl against the [libsodium](#port-libsodium) port instead of
  the copy of libsodium in its source.

## Patches

- `no-update-check.patch` — Changes the default of `check_updates_on_startup` in `spyder/config/main.py` from `True` to `False`. The update manager asks GitHub and PyPI at every start, and there is no build switch for it. The check stays available in Preferences and the Help menu.

## Notes

The bundle comes from `vendoring = python` with an explicit `pypackages` list (see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys)). The
build backends that are not ports go into `$SRC_ROOT/backends`, on `PYTHONPATH` for this build
only: `expandvars`, `coherent-licensed`, `dunamai` and `uv-dynamic-versioning` from the bundle, and
pbr, the recipe's second source. qstylizer's `setup.py` is pbr's
(`setup_requires=['pbr']`); without pbr, setuptools ignores its `setup.cfg` and reports version
0.0.0, which pip refuses.

The installed files go to two places:

- `spyder-kernels`, `wurlitzer` and `pyls-spyder` go into the system `site-packages`. Spyder
  removes `PYTHONPATH` from the environment of every kernel and language server it starts, so
  what those processes import must be importable by plain `python3`. Spyder is their only user.
- Everything else goes under `/usr/lib/spyder`. Several of these modules are also in other ports'
  bundles (`aiohttp` in vdirsyncer's, `arrow` in jupyterlab's), and two packages owning one path
  in `site-packages` is a conflict `kpkgadd` refuses.

`/usr/lib/spyder` is made a virtual environment over the system Python: a `pyvenv.cfg` that
includes the system `site-packages`, and `bin/python` linked to `python3`. Its own
`site-packages` is then a site directory of the interpreter rather than a `PYTHONPATH` entry,
which Spyder would remove from `sys.path` as it starts. `sys.executable` is `bin/python`, so a
restart and the kernels Spyder starts see the same paths. Sphinx's private directory,
`/usr/lib/python3-sphinx`, joins through a `.pth` file in that `site-packages`. `/usr/bin/spyder`
is a wrapper that sets `QT_API=pyqt6` and points `SPATIALINDEX_C_LIBRARY` at
[libspatialindex](#port-libspatialindex) for the Rtree binding, then runs the program with that
interpreter.

The translation directories under `spyder/locale` are removed, but the `locale` directory
itself stays, because Spyder lists it to offer languages.
