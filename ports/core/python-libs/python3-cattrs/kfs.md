## About
[cattrs]{.idx} converts between attrs classes or dataclasses and plain Python data (dicts, lists,
strings and numbers), the step before and after JSON or another wire format.
[python3-lsp-ruff](#port-python3-lsp-ruff)'s `lsprotocol` builds its language-server messages
with it, and [Blender](#port-blender)'s Extensions platform and remote asset libraries read their
listings with it.

## Notes
cattrs builds with hatchling and hatch-vcs, both ports in `depends`. hatch-vcs asks version
control for the version, and an unpacked source distribution is not a checkout, so `build.sh`
hands it the recipe's version in `SETUPTOOLS_SCM_PRETEND_VERSION`. Its run-time dependencies are
[python3-attrs](#port-python3-attrs) and
[python3-typing-extensions](#port-python3-typing-extensions); `exceptiongroup` is required only
below Python 3.11.
