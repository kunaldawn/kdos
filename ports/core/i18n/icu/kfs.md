## About

ICU, International Components for Unicode, is a library of Unicode and locale services:
character properties and names, collation, text boundaries, number, date and time formatting,
time zones, and conversion between character sets. In KDOS it sits under
[harfbuzz](#port-harfbuzz), [libxml2](#port-libxml2), boost, both Qt series, the web engines,
[nodejs](#port-nodejs)'s `Intl`, and postgresql's collation, among others.

[kdos-shell](#port-kdos-shell) depends on it without linking it: it runs a small program against
ICU at build time to write `/usr/share/kdos/charnames.idx`, the index of Unicode character names
the character picker searches, and ships that file instead of the library.

The source is upstream's archive of the release tag, which holds ICU for C, ICU for Java and the
tools beside them; the build uses `icu4c/source` alone. The tag archive carries the locale data
in source form, so configure runs python3's `icutools.databuilder` to write the data build's
rules, and the data library is compiled from those sources. That is the reason for python3 in
`depends`.

## Flags

- `CC=gcc` — Names the C compiler. configure's own search tries `cc` and `clang` before `gcc`. The
  chroot already sets the same `CC=gcc` and `CXX=g++` for every port; see
  [A configuration script preferring another compiler](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-configuration-script-preferring-another-compiler).
- `CXX=g++` — Names the C++ compiler; configure's search tries `c++` and `clang++` before `g++`.
- `--disable-samples` — Builds none of the sample programs, which are not installed.
- `--disable-tests` — Builds none of the test suites, which the recipe does not run.

## Notes

The last line compiles the time-zone rules `data/misc/zoneinfo64.txt` into a separate
`zoneinfo64.res` in `/usr/share/icu/$version/`, with the `genrb` just built, run against the
libraries just built in `lib/` rather than any ICU already installed. ICU's own data library
carries the same rules; this extra file is for nodejs, whose `build.sh` copies it over the
`zoneinfo64.res` inside its vendored Rust crate, so that its `Temporal` and ICU's `Intl` read
one time-zone database (see
[Data with no other source form](../../../../docs/kdos/01-philosophy/why-kdos.md#data-with-no-other-source-form)).
nodejs finds the file by the version `pkg-config --modversion icu-uc` prints, so the directory
name must stay the ICU version.
