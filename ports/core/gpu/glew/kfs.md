## About

GLEW, the OpenGL Extension Wrangler, finds out at run time which OpenGL version and extensions the
current context offers and loads their entry points; a program calls `glewInit()` once and then
uses the functions as if they were linked. It also installs `glewinfo`, which prints every
extension and entry point the driver supports, and `visualinfo`, which lists the visuals and
framebuffer configurations. Games and emulators written against GLEW link it.

## Flags

- `-DBUILD_UTILS=ON` — Builds `glewinfo` and `visualinfo`, upstream's default.
- `-DGLEW_X11=ON`, `-DGLEW_EGL=OFF` — Builds the GLX variant of the library. `GLEW_EGL` swaps GLX
  for EGL under the same soname, so one build serves one of them; this is the GLX build the
  programs that use GLEW are written against. Function lookup goes through libglvnd's dispatch and
  resolves under an EGL context too, but `glewInit()` returns `GLEW_ERROR_NO_GLX_DISPLAY` when no
  GLX display is current, so a program that treats that as fatal runs under Xwayland.
- `-DGLEW_OSMESA=OFF`, `-DGLEW_REGAL=OFF` — Builds neither the OSMesa (off-screen Mesa) nor the
  Regal variant; both are upstream's defaults, named here.
- `-DOpenGL_GL_PREFERENCE=GLVND` — Tells CMake's `FindOpenGL` to take the
  [libglvnd](#port-libglvnd) libraries, `libOpenGL` and `libGLX`, rather than the legacy `libGL`.
  The link line names those two with libX11 and libXext; under the tree's `--as-needed`,
  `libGLX.so.0` is the one that stays a run-time dependency, since GLEW resolves every GL entry
  point through `glXGetProcAddressARB`.

## Notes

GLEW's CMake project is in `build/cmake`, not at the top of the tree, and `build.sh` changes into
it first. It is used rather than the top-level `Makefile` because it installs into `lib` rather
than `lib64` and writes a `glew.pc` with the real library directory.

The installed `glew.pc` says `Requires: glu`, so [glu](#port-glu) is in `depends` although the
library is compiled with `GLEW_NO_GLU` and links no GLU itself: without `glu.pc` in the root, every
consumer's `pkg-config glew` would fail.
