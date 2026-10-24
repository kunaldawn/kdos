## About
[adafruit-nrfutil]{.idx} is Adafruit's fork of Nordic's `nrfutil` for Python 3: it makes DFU
packages for nRF52 boards and flashes them over the board's serial bootloader.
[rnode-firmware](#port-rnode-firmware)'s `rnodeconf` uses it to flash RNodes built on nRF52
boards.

## Patches
- `setup-setuptools.patch` — Removes from `setup.py` the import of setuptools' `test` command and the `NoseTestCommand` class and `cmdclass` entry built on it, and the line that rebinds `setup` to `distutils.core.setup`. setuptools 72 and later have no `test` command, so that import stops the build; Python 3.12 and later have no `distutils` in the standard library, and with that line gone `setup()` is setuptools' own.

## Notes
`test -x "$PKG/usr/bin/adafruit-nrfutil"` checks that the console script the package exists for
was installed.
