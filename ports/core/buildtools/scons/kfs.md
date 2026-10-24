## About
[SCons]{.idx} is a build tool written in Python: a project describes its build in an
`SConstruct` file, which is a Python script. KDOS carries it because
[gpsd](#port-gpsd) has no other build system; [goxel](#port-goxel) builds with it too.

## Notes
The install runs with pip's build isolation on, which is unusual among the tree's Python ports.
The vendor bundle holds one distribution, the setuptools sdist, and pip builds that into its
temporary build environment from `vendor` (`--no-index --find-links=vendor`) before it builds
SCons. Nothing from the bundle is installed into the package.

The three manual pages, `scons.1`, `scons-time.1` and `sconsign.1`, are already rendered at
the top of the source distribution and are copied as they are; pip does not install them.
