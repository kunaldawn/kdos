## About

constantly gives Python symbolic constants: named constants, constants with values, and bit
flags, each grouped in a container class. [python3-twisted](#port-python3-twisted) uses them for
its log levels and protocol constants.

## Notes

`setup.py` imports `versioneer` to read the version, so
[python3-versioneer](#port-python3-versioneer) is in `depends`. The release archive carries a
generated `constantly/_version.py` that holds the version, so versioneer needs no git metadata
and no directory name to find it.
