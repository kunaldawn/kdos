## About

GpgME++ is the C++ binding to [gpgme](#port-gpgme): key, context and operation classes over the C
API. It is released separately from GPGME. In KDOS it is used by [poppler](#port-poppler) for
signed PDFs, by [libreoffice](#port-libreoffice), [kate](#port-kate), [ledger](#port-ledger), and
through [qgpgme](#port-qgpgme) by [kleopatra](#port-kleopatra) and its libraries.

## Flags

- `-DENABLE_SHARED=ON`, `-DENABLE_STATIC=OFF` — Build the shared library only. These are the
  project's own switches, and both values are upstream's defaults.

## Patches

- `release-tarball-version.patch` — In `cmake/modules/G10GetFullVersion.cmake`, a build with no `.git` directory never runs `git describe`, so the commit count it reads stays undefined, and the original test treats undefined the same as an empty answer and appends `-unknown` to the version. The patch separates the two cases: undefined leaves the version as it is, and only an empty answer from `git describe` still adds `-unknown`. The full version is written into `gpgmepp.pc`, so without the patch a build from the release tarball installs a pkg-config file that says `Version: <version>-unknown`.
