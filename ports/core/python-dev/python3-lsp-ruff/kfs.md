## About
python-lsp-ruff is a `pylsp` plugin that runs [ruff](#port-ruff) inside the
[Python language server](#port-python3-lsp-server): ruff's diagnostics, its fixes as code
actions, and its formatter. The plugin calls the `ruff` program from the ruff port.

## Flags
- `lsprotocol` — named before `.`, the plugin's one dependency that nothing else in the tree
  imports, so it comes from the
  [Python vendor bundle](../../../../docs/kdos/05-developer/writing-ports.md#the-three-python-keys)
  and installs into this package. cattrs, which lsprotocol converts its messages with, is imported
  by Blender too, so it is a port of its own, [python3-cattrs](#port-python3-cattrs), in
  `depends`.

## Notes
The bundle unpacks into `$SRC_ROOT/vendor`, outside the source tree. The plugin keeps its code at
the top of its tree, and a `vendor/` directory there would be a second top-level package to
setuptools' package discovery, which then refuses to build; see
[Where the bundle goes](../../../../docs/kdos/05-developer/writing-ports.md#where-the-bundle-goes).
The `test -e` on `pylsp_ruff/plugin.py` after the install checks that the plugin itself reached
the package.
