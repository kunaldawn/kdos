## About

Embree is Intel's library of ray-tracing kernels for the processor: the acceleration structures
and the ray intersection tests a renderer builds on. In KDOS [Blender](#port-blender) builds its Cycles
renderer with it (`WITH_CYCLES_EMBREE`). Kernels for every x86-64 instruction set from SSE2 to AVX-512 are compiled into
the one library, and it chooses the widest the running processor has.

## Flags

- `-DEMBREE_STATIC_LIB=OFF` — Build `libembree4.so`, the shared library. `build.sh` checks that it
  was installed.
- `-DEMBREE_ZIP_MODE=OFF` — Install with the system layout. Upstream's default, on, lays the files
  out as its binary release archive: `embree-vars.sh` scripts at the top of the prefix and an
  `$ORIGIN`-relative RPATH.
- `-DEMBREE_INSTALL_DEPENDENCIES=OFF` — Do not copy oneTBB's libraries into the package; the
  [onetbb](#port-onetbb) port provides them.
- `-DEMBREE_TUTORIALS=OFF` — Leave out the tutorial programs, which upstream builds by default.
- `-DEMBREE_TASKING_SYSTEM=TBB` — Build hierarchies in parallel with oneTBB. It is upstream's
  default.
- `-DEMBREE_ISPC_SUPPORT=ON`, `-DEMBREE_ISPC_EXECUTABLE=/usr/bin/ispc` — Install the headers and
  bindings for programs written in ISPC, Intel's SPMD compiler, built with the [ispc](#port-ispc)
  port. Off by default upstream.
- `-DEMBREE_SYCL_SUPPORT=OFF` — No SYCL kernels for Intel GPUs.
- `-DEMBREE_ISA_SSE2=ON` — This switch and the four after it, `-DEMBREE_ISA_SSE42=ON`,
  `-DEMBREE_ISA_AVX=ON`, `-DEMBREE_ISA_AVX2=ON` and `-DEMBREE_ISA_AVX512=ON`, compile a kernel set
  for each instruction set; the library picks one at run time. Upstream turns the AVX sets on only
  when the compiler passes a test for each, so naming them keeps the set the same on every build.
- `-DEMBREE_STACK_PROTECTOR=ON` — Compile with stack protection against overwritten return
  addresses. Off by default upstream.
- `-DEMBREE_RAY_MASK=ON`, `-DEMBREE_FILTER_FUNCTION=ON` — Ray masks, with which a ray skips the
  geometry whose mask it does not match, and intersection filter callbacks. Both are upstream's
  defaults.
- `-DEMBREE_BACKFACE_CULLING=OFF` — The kernels report hits on both sides of a surface. It is
  upstream's default.
- `-DEMBREE_GEOMETRY_TRIANGLE=ON` — This switch and the other `EMBREE_GEOMETRY_` switches (quads,
  curves, subdivision surfaces, user geometry, instances, instance arrays, grids and points) build
  every geometry type. All are upstream's defaults, named so the set cannot change.
