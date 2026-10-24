## About
[dateutil]{.idx} extends Python's `datetime`: a parser for most date formats, relative deltas
(`relativedelta`), recurrence rules after RFC 5545, and time-zone support. [pandas](#port-pandas)
and [matplotlib](#port-matplotlib) import it when they load, and so do the other ports that name
it.

## Notes
The package takes its version from setuptools-scm. The source distribution has no `.git`, so
setuptools-scm reads the version from the `PKG-INFO` file the sdist carries.
