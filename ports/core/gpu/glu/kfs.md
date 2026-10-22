## About

GLU is the OpenGL Utility library, `libGLU` and `GL/glu.h`: a helper layer over OpenGL with mipmap
building, quadrics (spheres, cylinders, disks), NURBS surfaces, polygon tessellation and the
`gluPerspective` and `gluLookAt` camera calls. The Mesa project releases it as a tarball of its
own, apart from Mesa. Games, emulators and other programs written against the fixed-function OpenGL
API link it, and so does [glew](#port-glew).

## Flags

- `-Dgl_provider=glvnd` — Links `libGLU` against `libOpenGL` from [libglvnd](#port-libglvnd), the
  vendor-neutral library with the GL entry points and no window system, rather than against
  `libGL`, which pulls in GLX. This is upstream's default, named here; it makes GLU as usable by an
  EGL program on Wayland as by a GLX one under Xwayland.
