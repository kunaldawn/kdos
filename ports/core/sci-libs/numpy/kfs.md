## About

NumPy is Python's N-dimensional array type and the numerical routines built on it. It is the base
of the scientific Python stack in KDOS: [scipy](#port-scipy), [matplotlib](#port-matplotlib),
[pandas](#port-pandas) and [astropy](#port-astropy) import it, and so do the Python parts of
[gnuradio](#port-gnuradio), [opencv](#port-opencv), [gdal](#port-gdal) and
[blender](#port-blender). It is built from source with meson through pip, and its linear algebra
goes to [openblas](#port-openblas).

## Flags

- `export CFLAGS="${CFLAGS/-O2/-O3}" CXXFLAGS="${CXXFLAGS/-O2/-O3}"` — Compile at `-O3`. meson's
  release optimisation is `-O3`, but meson puts the exported `CFLAGS` after its own, so the tree's
  `-O2` would win; this is the form
  [Every port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port) gives for a hot
  port.
- `--no-build-isolation` — Build with the meson-python, Cython and setuptools already in the build
  root rather than ones pip would fetch into a temporary environment; the build has no network.
- `-Csetup-args=-Dcpu-baseline=none` — Assume no processor feature beyond what the compiler
  targets. NumPy's default, `min`, is x86-64 v2 on x86_64, which would make the package refuse to
  import on an older processor. The faster code paths are not lost: `cpu-dispatch` stays at its
  default `max`, so NumPy still compiles its SIMD kernels for every newer level and picks among
  them at run time. This is the rule in
  [Every port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port).
- `-Csetup-args=-Dblas=openblas`, `-Csetup-args=-Dlapack=openblas` — Link
  [openblas](#port-openblas) for BLAS and LAPACK by name, instead of taking whichever library
  NumPy's search order finds first.
- `-Csetup-args=-Dallow-noblas=false` — Fail the build when no BLAS is found. Upstream's default
  is to build with its own slow fallback routines instead, which would ship a NumPy whose matrix
  products run without OpenBLAS and say nothing.

## Notes

The meson that configures NumPy is not the [meson](#port-meson) port. `pyproject.toml` names
`vendored-meson/meson/meson.py`, a copy NumPy carries with its own changes for its CPU feature
handling, and meson-python runs that one.

Upstream installs its test suites into the package (`--tags=…,tests,…` in `pyproject.toml`).
The `find … -name tests -prune -exec rm -rf` after the install removes every `tests` directory
under `numpy/` from the package.
