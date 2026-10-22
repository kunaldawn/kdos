## About

HDF5 is a file format and C library for large, structured scientific data: groups of typed,
chunked, optionally compressed n-dimensional datasets in one file. In KDOS it is the storage layer
under [netcdf-c](#port-netcdf-c), [libmed](#port-libmed), [matio](#port-matio) and
[python3-h5py](#port-python3-h5py), and [octave](#port-octave), [gdal](#port-gdal),
[qgis](#port-qgis), [freecad](#port-freecad), [gmsh](#port-gmsh), [labplot](#port-labplot) and
[satdump](#port-satdump) read and write it. The package also carries the command-line tools,
`h5dump`, `h5ls`, `h5repack` and the rest.

## Flags

- `-DHDF5_INSTALL_DATA_DIR=share/hdf5` — Where HDF5 installs its loose data files, its
  `LICENSE` among them. Upstream's default on Linux is `share`, which would put them straight into
  `/usr/share`; here they land in `/usr/share/hdf5/`.
- `-DBUILD_SHARED_LIBS=ON`, `-DBUILD_STATIC_LIBS=OFF` — Build only the shared libraries. HDF5
  builds static ones as well by default.
- `-DHDF5_BUILD_CPP_LIB=ON` — Also build the C++ wrapper interface, `libhdf5_cpp`, which upstream
  leaves off.
- `-DHDF5_BUILD_FORTRAN=ON` — Also build the Fortran wrapper interface, `libhdf5_fortran` and its
  `.mod` files, which upstream leaves off. Much of the code that writes HDF5 files is Fortran, and
  without the interface such code cannot be built here. gfortran comes with [gcc](#port-gcc).
- `-DHDF5_BUILD_HL_LIB=ON` — Build the high-level library, `libhdf5_hl` (images, tables, packet
  tables and dimension scales), on top of the core one. Upstream's default.
- `-DHDF5_BUILD_TOOLS=ON` — Build the tool programs. Upstream's default.
- `-DHDF5_BUILD_EXAMPLES=OFF` — Build none of the C example programs, which upstream builds by
  default.
- `-DHDF5_ENABLE_ZLIB_SUPPORT=ON` — Build the deflate filter against [zlib](#port-zlib), so files
  with gzip-compressed datasets can be read and written. Off upstream; with it on, configure fails
  if zlib is not found.
- `-DHDF5_ENABLE_SZIP_SUPPORT=ON` — Build the szip filter. HDF5 links
  [libaec](#port-libaec), whose `libsz` is a free replacement for the SZIP library, and fails at
  configure time if it is not found. Satellite and climate data files are often written with the
  szip filter, and a library without it cannot read those datasets.

## Notes

The release tarball's entries start with `./hdf5-<version>/`. kpkg strips one component, the `.`,
so the source lands one level down and `build.sh` begins with `cd "$name-$version"`; see
[Sources, and what each one becomes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes).

The Fortran `.mod` files are installed in `/usr/include/mod/shared`. The exported CMake targets
in `/usr/lib/cmake/hdf5` put that directory on a consumer's include path, but `hdf5_fortran.pc`
gives `moddir=${prefix}/include`, and `h5fc` takes its flags from that file, so a Fortran program
compiled with `h5fc` or with `pkg-config hdf5_fortran` does not find the modules without an
`-I/usr/include/mod/shared` of its own.
