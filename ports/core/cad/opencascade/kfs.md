## About
[Open CASCADE Technology]{.idx} (OCCT) is a boundary-representation geometry kernel: solid
modelling, Boolean operations, meshing, and data exchange through STEP, IGES, glTF, OBJ, PLY,
STL and VRML. In KDOS it is there for other programs: [FreeCAD](#port-freecad), [KiCad](#port-kicad),
[netgen](#port-netgen), [gmsh](#port-gmsh), [IfcOpenShell](#port-ifcopenshell),
[LibrePCB](#port-librepcb), [Horizon EDA](#port-horizon-eda) and
[PrusaSlicer](#port-prusaslicer) link it, for their geometry and their STEP import and export.

## Flags
- `-DBUILD_LIBRARY_TYPE=Shared` — Build the toolkits (`libTKernel.so`, `libTKMath.so`,
  `libTKDESTEP.so` and the rest) as shared libraries rather than static archives.
- `-DINSTALL_DIR_LIB=lib`, `-DINSTALL_DIR_CMAKE=lib/cmake/opencascade` — Where the libraries and
  the `OpenCASCADEConfig.cmake` files go. Consumers find OCCT with `find_package(OpenCASCADE)`,
  which reads that directory.
- `-DINSTALL_DIR_SCRIPT=share/opencascade/scripts` — Install the environment scripts OCCT
  generates (`env.sh`, `custom.sh` and the `custom_*` scripts they source) under `share`. For the
  Unix layout their default is the `bin` directory, which would put them in `/usr/bin` as
  commands.
- `-DBUILD_MODULE_Draw=OFF`, `-DUSE_TK=OFF` — Do not build Draw (`DRAWEXE`), OCCT's
  Tcl/Tk test shell, nor use Tk, which only Draw's user interface uses. The one program left is
  `ExpToCasExe`, from the DETools module, which is on by default: it generates OCCT classes
  from a STEP EXPRESS schema and is installed in `/usr/bin`.
- `-DBUILD_DOC_Overview=OFF`, `-DINSTALL_SAMPLES=OFF`, `-DINSTALL_TEST_CASES=OFF` — Generate no
  HTML overview, and install neither the samples nor the non-regression test scripts.
- `-DUSE_XLIB=OFF` — Build the OpenGL driver, `libTKOpenGl`, on EGL instead of GLX and Xlib:
  with `HAVE_XLIB` undefined on Linux, OCCT defines `HAVE_EGL` and links `libGL` and `libEGL`
  instead of `libGL` and `libX11`. OCCT's viewer then works in any EGL context, a Wayland one
  among them; built with Xlib it is GLX and X11 only.
- `-DUSE_OPENGL=ON`, `-DUSE_GLES2=OFF` — Build the visualization module against desktop OpenGL
  through [libglvnd](#port-libglvnd), not OpenGL ES 2.0.
- `-DUSE_FREETYPE=ON` — Render text with [FreeType](#port-freetype2), from the font directories
  `Font_FontMgr` asks [fontconfig](#port-fontconfig) for; the switch is also what links
  `libfontconfig`.
- `-DUSE_RAPIDJSON=ON`, `-DUSE_DRACO=ON` — Read and write glTF: [RapidJSON](#port-rapidjson)
  parses the JSON of a glTF file, and [Draco](#port-draco) decodes meshes compressed with the
  `KHR_draco_mesh_compression` extension. Both are off by default.
- `-DUSE_FREEIMAGE=OFF`, `-DUSE_FFMPEG=OFF`, `-DUSE_OPENVR=OFF` — Build without FreeImage
  (image files in the visualization module), FFmpeg (recording a view to video) and OpenVR
  (virtual reality). All three are off by default. FreeImage and OpenVR are not in the tree;
  [ffmpeg](#port-ffmpeg) is, and the switch keeps the visualization module from linking it.
- `-DUSE_TBB=OFF` — Run OCCT's parallel algorithms on its own thread pool instead of Intel's
  Threading Building Blocks. Upstream's description says OCCT stays parallel without it;
  [onetbb](#port-onetbb) is in the tree, and with the switch on the toolkits that list
  `CSF_TBB`, `libTKernel`, `libTKMath`, `libTKGeomBase`, `libTKTopAlgo`, the Boolean toolkit
  `libTKBO`, `libTKV3d` and `libTKOpenGl`, would link it.
- `-DUSE_EIGEN=OFF` — Do not search for Eigen. No toolkit of this release lists `CSF_EIGEN` in
  its `EXTERNLIB`, so the switch changes no library.
- `-DUSE_VTK=OFF` — Build without the VTK Integration Services, OCCT's bridge from its shapes to
  [VTK](#port-vtk) pipelines.

## Patches
- `no_mallinfo.patch` — Replaces the `mallinfo()` heap figures of `OSD_MemInfo` with `getrusage()`'s peak resident set size (`ru_maxrss`), in the Linux branch and the Emscripten one. On Linux the unpatched code already reads `mallinfo` only under glibc and reports 0 otherwise; with the patch, on musl, the heap-usage counter reports the peak resident size in KiB instead of 0.
- `no_feenableexcept.patch` — Rewrites two `#ifdef __linux__` lines of `OSD_signal.cxx`, around the `<cfenv>` include and the SIGFPE handler's call to `OSD::SetFloatingSignal()`, as `#ifdef __linux__ && defined(__GLIBC__)`; see the Notes for what that does.
- `no_backtrace.patch` — Removes `#include <execinfo.h>` and the body of `Standard::StackTrace()` that calls `backtrace()` and `backtrace_symbols()`, so the function returns false. musl has no `execinfo.h`, and without the patch the file does not compile.

## Notes
- **`no_feenableexcept.patch` changes nothing.** Its header says it limits `feenableexcept()`
  and `fedisableexcept()`, which musl lacks, to glibc. The calls it means are already guarded by
  `defined(__linux__) && defined(__GLIBC__)` in this release's `OSD::SetFloatingSignal`. The two
  lines it edits read `#ifdef __linux__ && defined(__GLIBC__)`, and `#ifdef` takes one name: the
  compiler warns about the extra tokens and tests `__linux__` alone, as before the patch.
- **The Draco check.** The last line of `build.sh` requires `libTKDEGLTF.so`, the glTF toolkit,
  to have `libdraco` among its `NEEDED` entries, so a configure that did not find Draco stops the
  build instead of installing a glTF reader that cannot open compressed meshes.
