## About

qrupdate is a Fortran library of fast updates to QR, Cholesky and LU
factorisations: after a rank-one change, or a row or column inserted or deleted, it recomputes the factors from the old ones
instead of from scratch. [octave](#port-octave) links it for its `qrupdate`, `cholupdate`,
`qrinsert`, `qrdelete` and related functions. The source is `qrupdate-ng`, a maintained fork
of Jaroslav Hajek's original library with a CMake build. It installs the same `libqrupdate`, so
Octave's configure finds it as `-lqrupdate`; it calls BLAS and LAPACK, which here are
[openblas](#port-openblas).

## Flags

- `-DBLA_VENDOR=OpenBLAS` — Tell CMake's `FindBLAS` and `FindLAPACK` to look for OpenBLAS only,
  rather than trying each vendor they know in turn.
- `-DINTEGER8=OFF` — Build with Fortran's default 4-byte integers. `ON` would ask for a BLAS with
  8-byte integers (`BLA_SIZEOF_INTEGER 8`); [openblas](#port-openblas) is built with the default
  32-bit interface, and the two must agree on the integer size.
- `-DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON` — Make CMake's `find_package(Doxygen)` fail, so no
  API reference is set up. Without it the build would pick doxygen up whenever another port has
  put it in the build root, and the package would differ by what happened to be installed.
- `-DBUILD_SHARED_LIBS=ON` — Build `libqrupdate.so`, upstream's default.

## Patches

- `pc-openblas.patch` — In `qrupdate.pc.in`, replaces `Requires: blas, lapack` with `Requires.private: openblas`. The tree has no `blas.pc` or `lapack.pc`, only OpenBLAS's `openblas.pc`, so with upstream's line `pkg-config qrupdate` fails for want of two packages that do not exist. As a private requirement it is read only for static linking, and a consumer of the shared library is not given `-lopenblas` it does not call directly.

## Notes

`BUILD_STATIC_LIBS` is an option of its own in `qrupdate-ng`, on by default, and the recipe leaves
it alone, so the package installs `libqrupdate.a` beside the shared library.
