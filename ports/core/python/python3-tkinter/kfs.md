## About
Python's binding to Tk: the `_tkinter` extension module and the `tkinter` package, with the
parts of the standard library that need them, `turtle` and its demonstrations in `turtledemo`,
and IDLE, Python's own editor and shell, as `idlelib` and the `idle3` command. Tk draws through
X11, so these windows open under Xwayland.

The [python3](#port-python3) port is built in `30_foundation`, before [tk](#port-tk) exists, and
deletes all of this from its own package. This port configures the same CPython tarball once Tk is
installed and builds only `Modules/_tkinter` and the `idle` launcher, then copies the pure-Python
parts from the source's `Lib`. Both recipes carry `group = python3`, so the version checker offers
them as one bump.

## Notes
`idle3.14` is made by CPython's own make rule from `Tools/scripts/idle3`, which writes
`/usr/bin/python3.14` into its `#!` line; `idle3` is a link to it, as `make install` would make.
`idlelib/idle_test`, IDLE's test suite, is not installed, as the python3 port's
`--disable-test-modules` leaves out every other test package.
