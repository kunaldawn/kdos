## About
setuptools-scm is the setuptools plugin that sets a package's version from version control,
using [python3-vcs-versioning](#port-python3-vcs-versioning) for the work. A project that
declares `dynamic = ["version"]` and lists setuptools-scm among its build requirements gets its
version from the Git tags of a checkout or, in a source distribution, from the `PKG-INFO` file
the distribution was made with.

Many ports in KDOS build with it. In `30_foundation` it is a build requirement of
[python3-pluggy](#port-python3-pluggy), and [python3-hatch-vcs](#port-python3-hatch-vcs) calls
it on behalf of hatchling projects; later phases build matplotlib, astropy, khal, khard,
deluge and many `python3-*` modules with it.

## Flags
- `--no-build-isolation` — Build with what the build root already holds rather than with
  requirements pip would fetch into a temporary environment. setuptools-scm builds with
  setuptools and vcs-versioning (`requires = ["setuptools>=45", "vcs-versioning>=2.5.0.dev0,<3"]`),
  both named in `depends`.

## Notes
A port's source has no version-control metadata: kpkg unpacks a tarball. A PyPI source
distribution carries `PKG-INFO`, which vcs-versioning's `pkginfo` fallback reads, so such a port
needs nothing more. A source taken from a forge's tag archive has neither `.git` nor `PKG-INFO`,
and the port gives the version itself through `SETUPTOOLS_SCM_PRETEND_VERSION` in its
`build.sh`, as several ports in the tree do.
