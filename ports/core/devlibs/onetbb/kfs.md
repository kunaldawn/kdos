## About
[oneTBB]{.idx} (oneAPI Threading Building Blocks) is a C++ runtime for task parallelism: parallel
loops and reductions, flow graphs, concurrent containers, and a scalable memory allocator.
Blender, OpenCV, OpenImageIO, OpenVDB, Embree, PrusaSlicer and the other ports that name it in
`depends` run their parallel work on it.

## Flags
- `-DTBB_TEST=OFF`, `-DTBB_EXAMPLES=OFF`, `-DTBB4PY_BUILD=OFF` — Build no tests, no examples and
  no `tbb4py` Python module.
- `-DTBB_STRICT=OFF` — Do not treat compiler warnings as errors, which upstream does by default;
  with it on, a warning that a newer compiler adds would stop the build.
- `-DTBBMALLOC_BUILD=ON`, `-DTBBMALLOC_PROXY_BUILD=OFF` — Build `libtbbmalloc`, the scalable
  allocator programs call explicitly, but not `libtbbmalloc_proxy`, which replaces `malloc` and
  `free` in every process that loads it. The proxy looks up glibc's internal entry points
  `__libc_free` and `__libc_realloc` with `dlsym`, and musl's `libc.so` exports neither.
- `-DTBB_DISABLE_HWLOC_AUTOMATIC_SEARCH=OFF` — Let CMake find [hwloc](#port-hwloc) through
  pkg-config, and build `tbbbind` against it, the library that pins task arenas to NUMA nodes and
  core types. Upstream's default turns the search off whenever CMake considers the build a cross
  build; the switch states the native default.

## Notes
The last line of `build.sh` requires `libtbbbind_2_5.so`. oneTBB names `tbbbind` after the hwloc
it found through pkg-config, `tbbbind_2_5` for hwloc 2.5 or later and `tbbbind_2_0` below, and
builds none when pkg-config finds no hwloc. A configure that missed hwloc therefore stops the
build instead of installing a oneTBB without NUMA placement, and an hwloc older than 2.5 stops it
too.
