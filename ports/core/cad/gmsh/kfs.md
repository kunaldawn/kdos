## About
[Gmsh]{.idx} is a finite-element mesh generator: it reads or builds a geometry, through its own
`.geo` scripting language or OpenCASCADE's CAD kernel (STEP, IGES, BRep), meshes it in one, two or
three dimensions, and displays the results a solver writes back. The port builds the FLTK
interface, the shared library `libgmsh` and its Python API, so a script can `import gmsh`.

## Flags
- `CXXFLAGS="$CXXFLAGS $(pkg-config --cflags cairo)"` — add Cairo's include directories to the
  C++ flags. The [fltk](#port-fltk) port is built with Cairo support, and its `FL/Fl.H` then
  includes, through `FL/Fl_Cairo.H`, `<cairo.h>` with no directory, which only `-I/usr/include/cairo` resolves; Gmsh's own
  Cairo check adds only the directory above it, for its `<cairo/cairo.h>`.
- `-DENABLE_BUILD_SHARED=ON` — build `libgmsh.so`, and with `-DENABLE_BUILD_DYNAMIC=ON` link the
  `gmsh` program against it rather than statically. Both are off by default upstream. The Python
  API, `gmsh.py`, is a ctypes wrapper that loads this library, and upstream installs the API files
  only when one of the library targets is enabled.
- `-DENABLE_FLTK=ON` — the graphical interface, upstream's default, named; with
  `-DENABLE_CAIRO=ON` it renders fonts through Cairo, which upstream marks experimental.
- `-DENABLE_OCC=ON` — the OpenCASCADE CAD kernel, and with `-DENABLE_OCC_CAF=ON` its CAF layer,
  which carries the names and colours of STEP and IGES entities. Both are upstream's defaults.
- `-DENABLE_SYSTEM_CONTRIB=ON` — prefer system copies of the libraries Gmsh carries under
  `contrib/`. Of those, the tree has [eigen](#port-eigen) and [tinyxml2](#port-tinyxml2); the rest
  (ANN, ALGLIB, MathEx, METIS, Voro++, gmm) are not ports and stay the bundled ones. Off by default
  upstream.
- `-DENABLE_EIGEN=ON` — linear algebra with Eigen instead of BLAS and LAPACK, upstream's default;
  `-DENABLE_GMP=ON` (GMP for the Kbipack homology code) and `-DENABLE_OPENMP=ON` are upstream's
  defaults too, named.
- `-DENABLE_MED=ON` — the MED mesh and post-processing formats, through [libmed](#port-libmed)
  and [hdf5](#port-hdf5). Upstream enables it only if both are found, and the `grep` after the
  configure stops the build when `GmshConfig.h` lacks `HAVE_MED`.
- `-DENABLE_CGNS=OFF` — with `ENABLE_MMG`, `ENABLE_PETSC`, `ENABLE_SLEPC`, `ENABLE_MUMPS` and
  `ENABLE_MPI`, the optional interfaces to libraries the tree does not carry: CGNS files, Mmg mesh
  adaptation, the PETSc and MUMPS solvers, SLEPc eigensolvers and MPI. CGNS and Mmg are on by
  default upstream and would be used if found; the others are off by default.
- `-DENABLE_POPPLER=OFF` — no PDF display inside the interface, an experimental feature that is off
  by default upstream; `-DENABLE_OSMESA=OFF`, likewise off by default, leaves out offscreen
  rendering through OSMesa.
- `-DENABLE_TESTS=OFF` — skip the test suite, which upstream enables by default.
- `-DENABLE_RPATH=OFF` — no RPATH in the program and library. Upstream's default writes the
  install's library directory, `/usr/lib`, plus the directory of every library it links from
  outside the build tree; here those are all on the loader's default path, an entry the
  [package lint](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
  warns about.
- `-DENABLE_PACKAGE_STRIP=OFF` — upstream's switch for stripping CPack packages. The port uses
  `cmake --install`, not CPack, so it changes nothing here.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_HDF5=ON` — make a missing HDF5 a configure error. Upstream looks
  for it without requiring it and leaves MED out when it is absent.

## Notes
Two checks after the configure guard the result: `HAVE_MED` in `build/src/common/GmshConfig.h`,
and `TINYXML2_LIB` in `CMakeCache.txt` naming `/usr/lib/libtinyxml2`, so neither the MED support
nor the system tinyxml2 can drop out in silence.

Upstream installs `gmsh.py` and the Julia binding `gmsh.jl` beside the library in `/usr/lib`.
`build.sh` moves `gmsh.py` into Python's `site-packages` and deletes `gmsh.jl`; the tree has no
Julia. The module loads the library by a name carrying Gmsh's major and minor version,
`libgmsh.so.4.15` for 4.15.2, and looks for it beside itself and in the two directories above it; from
`/usr/lib/python3.x/site-packages` the second of those is `/usr/lib`, so the moved module still
finds it. The 256-pixel icon is rendered from upstream's SVG with
`rsvg-convert`, which is why [librsvg](#port-librsvg) is in `depends`.
