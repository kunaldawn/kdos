## About
python-lsp-server, `pylsp`, is the Python language server: an editor connects to it over the
Language Server Protocol for completion, go-to-definition, hover, diagnostics and formatting. This
port ships it with every plugin it carries: Jedi (from the [ipython](#port-ipython) port), Rope,
Pylint, pyflakes, pycodestyle, pydocstyle, mccabe, flake8, autopep8 and YAPF, and the
`python-lsp-black` plugin with Black. The linters and formatters install their own commands too.
[spyder](#port-spyder) uses it for its editor, and [python3-lsp-ruff](#port-python3-lsp-ruff)
plugs into it.

## Flags
- `--target` — The first `pip3 install` puts `hatch-fancy-pypi-readme` and `pdm-backend`, build
  backends some bundle members need and which are not ports, into `$SRC_ROOT/backends` and onto
  `PYTHONPATH` for this build only, so nothing outside the package is written. See
  [The three Python keys](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
- `PYTOKENS_USE_MYPYC=0` — Build `pytokens` as plain Python. On CPython its `setup.py` otherwise
  compiles it with mypyc, which needs mypy, and mypy has no port. Black's and isort's mypyc build
  hooks are off unless enabled, so both build as plain Python without a switch.
- `python-lsp-jsonrpc` — The bundle members installed into this package, named on the second
  `pip3 install` and pinned in `pypackages`. `"$_d2m"` and `"$_ap8"` in that list are the
  unpacked, patched source trees of `docstring-to-markdown` and `autopep8`, installed in place of
  their sdists.

## Patches
- `stdlib-importlib-metadata.patch` — In `docstring-to-markdown`, import `entry_points` and `EntryPoint` from the standard library's `importlib.metadata` instead of the `importlib_metadata` backport, which has no port; the call it makes is the same. Without it `pylsp` fails on its first import.
- `autopep8-py314.patch` — In `autopep8`, held to an older release by `pylsp`'s version bound: `setup.py` reads the version through `ast`'s `.value` rather than `.s`, which the tree's Python does not have, so metadata generation runs; `detect_encoding()` comes from the standard library's `tokenize` instead of `lib2to3`, which the tree's Python does not ship, and whose import there would otherwise stop the command with `ModuleNotFoundError` on every file it reads; and `refactor()`, through which `--aggressive` runs its `lib2to3` fixers for E231, E721 and W690, returns the source unchanged when `lib2to3` cannot be imported.

## Notes
The two patched packages are unpacked from the vendor bundle into `$SRC_ROOT`, patched there and
installed from those directories; the bundle itself stays as it was fetched.
[patch](#port-patch) is in `depends` for that.

`test -x "$PKG/usr/bin/pylsp"` after the install stops the build if the server's entry point is
missing.
