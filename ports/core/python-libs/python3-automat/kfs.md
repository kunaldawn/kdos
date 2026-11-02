## About

Automat writes finite-state machines in Python: states, inputs and outputs declared on a class,
with the transitions between them checked when the class is defined.
[python3-twisted](#port-python3-twisted) builds the state machine of its reconnecting
`ClientService` with it.

## Notes

Automat builds with setuptools and takes its version through
[python3-setuptools-scm](#port-python3-setuptools-scm), which reads the `PKG-INFO` in the release
archive. Its `pyproject.toml` also lists hatch-vcs and wheel among the build requirements; the
setuptools backend imports neither, and the install runs with `--no-build-isolation`, so neither
is in `depends`. On the Python versions the tree has, Automat imports nothing outside the
standard library.
