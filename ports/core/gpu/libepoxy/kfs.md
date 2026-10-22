## About

libepoxy manages OpenGL function pointers for the programs that use it: a program calls `glFoo` as
if it were linked, and libepoxy resolves the right entry point on first use, through EGL or GLX,
for whichever context is current. It links no GL library itself and opens them with `dlopen` at run
time: `libEGL.so.1`, `libGL.so.1` or `libGLX.so.1`, `libOpenGL.so.0`, `libGLESv2.so.2`, all from
[libglvnd](#port-libglvnd). GTK 3 and 4, WebKitGTK, Xwayland's glamor renderer, QEMU's display and
SPICE client, Blender and Inkscape are among its users.

## Flags

- `-Degl=yes` — Builds the EGL path, through which every Wayland client resolves GL.
- `-Dglx=yes` — Builds the GLX path, which resolves `glX*` through `libGLX.so.1` or `libGL.so.1`;
  GTK's X11 backend draws through it under Xwayland. On Linux `auto` already builds both paths; the
  recipe names them.
- `-Dx11=true` — Builds the X11 support GLX and EGL-on-X11 need, against [libX11](#port-libX11).
  This is upstream's default, named here.
- `-Ddocs=false` — Builds no Doxygen API reference, upstream's default.
- `-Dtests=false` — Builds no test suite. Upstream's default is on.
