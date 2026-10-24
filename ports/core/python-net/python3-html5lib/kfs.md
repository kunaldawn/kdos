## About
[html5lib]{.idx} is a pure-Python HTML parser that follows the WHATWG HTML specification's
parsing algorithm, so it builds the same tree from malformed markup that a browser does.
[qt6-qtwebengine](#port-qt6-qtwebengine)'s build imports it, and [calibre](#port-calibre)
depends on it.

## Patches
- `html5lib-setuptools-82.patch` — Takes out `setup.py`'s unconditional `from pkg_resources import parse_version` and makes its `import pkg_resources` optional, setting the name to `None` when the import fails. The [python3-setuptools](#port-python3-setuptools) port installs no `pkg_resources`, and without the patch `setup.py` stops on that import before anything is built.

## Notes
Both patches change only `setup.py`.

With `html5lib-setuptools-82.patch` applied, nothing imports `parse_version`, and the block of
`setup.py` that calls it runs only when `pkg_resources` imports. Under a setuptools that still
ships `pkg_resources` that block would stop with a `NameError`, so the patch holds only while
the setuptools port has none.
