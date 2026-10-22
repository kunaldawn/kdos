## About

libplist reads and writes Apple [property lists]{.idx} in all four forms: binary, XML, JSON and
OpenStep. It is the bottom of the iPhone stack (libimobiledevice, usbmuxd and ifuse) and is also
linked by libgpod, kodi, gvfs and KDE's Solid and kio-extras. `plistutil` converts a plist from one
form to another.

## Flags

- `--without-cython` — Build no Python binding. The configure script builds the Cython module
  whenever `cython` is installed, and [python3-cython](#port-python3-cython) is built in `40_lang`,
  before this phase.
- `--without-tests` — Build no test suite.
