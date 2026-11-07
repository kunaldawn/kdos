## About

kdev-python is KDevelop's Python 3 language plugin: a parser built on CPython's own, a semantic
model (DUChain) with type inference, highlighting by meaning, code completion, navigation, a pdb
debugger front end, style checking, and documentation files that describe the standard library
and common modules to the completion.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`, which kpkg's
  English-only rule would remove from the package anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Wno-dev` — Silences CMake's warnings meant for the project's own developers.

## Notes

The parser links `libpython` from [python3](#port-python3) and requires a Python between 3.4.3
and 3.16, exclusive, with its development headers; a bump of python3 past that window stops the
configure until kdev-python follows.

The plugin requires the KDevPlatform and KDevelop CMake packages of its own major and minor
version, 6.0, from [kdevelop](#port-kdevelop), and moves to a new release together with it.
Depending on it makes kdevelop a build dependency of another port, which places kdevelop in
`43_toolkits` rather than `44_apps`.

Style checking runs the bundled `codestyle.py`, which imports `pycodestyle`; that module is not a
port, so the check reports it missing and does nothing.
