## About
[platformdirs]{.idx} tells a Python program where its configuration, data, cache, state and log
directories are on the platform it runs on; on Linux that is the XDG base directories, such as
`$XDG_CONFIG_HOME` or `~/.config`. It has no runtime dependencies.

## Notes
The vendor bundle is the build backend and nothing installed: platformdirs builds with hatchling
and hatch-vcs, and the bundle carries them with what they need (pluggy, pathspec, trove-classifiers,
setuptools-scm, vcs-versioning and the rest). The install runs with pip's build isolation on, and
pip builds that environment from `vendor` (`--no-index --find-links=vendor`) before building
platformdirs.
