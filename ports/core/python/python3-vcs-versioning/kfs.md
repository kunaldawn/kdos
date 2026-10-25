## About
vcs-versioning is the machinery that derives a Python package's version from version control:
it reads the tags and distance of a Git, Mercurial or Jujutsu checkout, and, where there is no
checkout, falls back to the metadata a source distribution carries. It installs a
`vcs-versioning` command and the entry points through which version schemes and file finders are
plugged in.

[python3-setuptools-scm](#port-python3-setuptools-scm) is built on it and is its one reverse
dependency in the tree; through setuptools-scm, and [python3-hatch-vcs](#port-python3-hatch-vcs) on
top of it, it is part of the build of every port that builds with either.

## Flags
- `--no-build-isolation` — Build with what the build root already holds rather than with
  requirements pip would fetch into a temporary environment. vcs-versioning builds with
  setuptools and needs packaging at build time (`requires = ["setuptools>=45",
  "packaging>=26.2"]`); both are ports, named in `depends`.
