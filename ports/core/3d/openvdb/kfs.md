## About

OpenVDB is the sparse volume format and library for smoke, fire, clouds and other volumetric data,
and NanoVDB is its compact read-only form for renderers. [Blender](#port-blender) reads and writes
volumes through OpenVDB, and its Cycles renderer compiles against the NanoVDB headers;
[OpenImageIO](#port-openimageio) and [PrusaSlicer](#port-prusaslicer) link the library too. The
package carries the core library, the `vdb_print` tool, the NanoVDB headers and the Python module.

## Flags

- `-DOPENVDB_ENABLE_RPATH=OFF` — Install the library and tools with no RPATH; everything is in
  `/usr/lib`.
- `-DOPENVDB_BUILD_CORE=ON`, `-DOPENVDB_CORE_SHARED=ON`, `-DOPENVDB_CORE_STATIC=OFF` — Build the
  core library, shared only. Upstream builds both a shared and a static library by default.
- `-DOPENVDB_BUILD_BINARIES=ON` — Build the command-line tools whose own switches are on, which by
  upstream's defaults is `vdb_print` alone.
- `-DOPENVDB_BUILD_PYTHON_MODULE=ON` — Build the `openvdb` Python module, off by default
  upstream. It is written on nanobind: `-Dnanobind_DIR` is the CMake directory that
  [python3-nanobind](#port-python3-nanobind) reports with `python3 -m nanobind --cmake_dir`, and
  `-DPython_EXECUTABLE` names the system Python. `build.sh` fails if the module was not installed.
- `-DOPENVDB_BUILD_UNITTESTS=OFF`, `-DOPENVDB_BUILD_DOCS=OFF` — No unit tests and no Doxygen
  documentation.
- `-DOPENVDB_BUILD_AX=OFF` — No OpenVDB AX, the expression language for volumes, which needs LLVM.
- `-DOPENVDB_BUILD_NANOVDB=ON`, `-DUSE_NANOVDB=ON` — Install NanoVDB with OpenVDB.
- `-DOPENVDB_USE_DELAYED_LOADING=OFF` — Build without delayed loading, the memory-mapped reader that
  loads a grid's voxels only when they are first touched. It is on by default upstream, and it is
  the one part of the library that needs Boost (`Boost::iostreams`); with it off, Boost is not
  linked.
- `-DOPENVDB_ENABLE_UNINSTALL=OFF` — No `uninstall` target.
- `-DUSE_BLOSC=ON`, `-DUSE_ZLIB=ON`, `-DUSE_TBB=ON` — Blosc and zlib compression of grids on disk,
  and oneTBB for threading. [c-blosc](#port-c-blosc), [zlib](#port-zlib) and
  [onetbb](#port-onetbb) are in `depends`.
- `-DUSE_EXR=OFF`, `-DUSE_PNG=OFF`, `-DUSE_LOG4CPLUS=OFF` — No OpenEXR or PNG image output, which
  only the `vdb_render` and `vdb_tool` programs use, and neither is built; and no log4cplus logging.
- `-DUSE_CCACHE=OFF` — Do not compile through ccache, which upstream uses by default whenever it is
  on the path.
- `-DCONCURRENT_MALLOC=Tbbmalloc` — Use oneTBB's scalable allocator. The default, `Auto`, takes
  jemalloc whenever the build root holds it and falls back to tbbmalloc, so the allocator would
  follow what other ports happened to install.
- `-DNANOVDB_BUILD_TOOLS=OFF`, `-DNANOVDB_USE_OPENVDB=OFF` — NanoVDB as headers only: its tools
  convert through OpenVDB grids, and nothing here uses them.
- `-DNANOVDB_USE_CUDA=OFF` — No CUDA.
- `-DNANOVDB_ALLOW_FETCHCONTENT=OFF` — Do not let NanoVDB's CMake download a missing dependency;
  the build has no network.

## Patches

- `tbb-version.patch` — Replaces the include at the top of the installed `openvdb/thread/Threading.h` with upstream's later form: it includes `tbb/version.h` when that header exists, and `tbb/task_arena.h` otherwise. The header reaches TBB's version macros through `tbb/blocked_range.h`, which in oneTBB 2023 includes no version header; `TBB_INTERFACE_VERSION` is then undefined, the code for TBB before 2021 is chosen, and neither OpenVDB nor anything that includes the header compiles.

## Notes

The version follows the one Blender pins. Cycles compiles against the NanoVDB headers, whose API
moves between minor releases, so a bump of either port checks the other.
