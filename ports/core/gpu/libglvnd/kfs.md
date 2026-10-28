## About

libglvnd, the GL Vendor-Neutral Dispatch library, is what a program links when it links OpenGL:
`libGL`, `libOpenGL`, `libGLX`, `libEGL` and `libGLESv2` are libglvnd's, and each passes the
program's calls on to the vendor library that owns the current context. In KDOS that vendor is
Mesa, built `-D glvnd=enabled`, which installs `libEGL_mesa` and `libGLX_mesa` behind these.
libglvnd also installs the `gl.pc`, `glx.pc`, `egl.pc` and `glesv2.pc` files the rest of the tree
builds against. How a Wayland client and an X11 client under Xwayland each reach Mesa through it is
in
[Desktop GL and GLX](../../../../docs/kdos/05-developer/testing.md#desktop-gl-and-glx).

## Flags

- `-D x11=enabled`, `-D glx=enabled` — Builds `libGLX`, the GLX front end, and the X11 platform of
  `libEGL`, against [libX11](#port-libX11), [libXext](#port-libXext) and
  [xorgproto](#port-xorgproto). Without them there is no `libGL` and no `gl.pc` or `glx.pc`: an X11
  client under Xwayland draws through GLX, and every build that asks for `OpenGL::GL` or `-lGL`
  links `libGL`. `enabled` turns the probe into a requirement.
- `-D gles1=false` — Builds no `libGLESv1_CM`, the OpenGL ES 1.x library. [mesa](#port-mesa) is
  built `-D gles1=disabled`, so there would be no vendor behind it.
- `-D egl=true` — Builds `libEGL`, upstream's default.
- `-D tls=false` — Keeps each thread's current dispatch table through POSIX thread-specific data
  instead of a `__thread` variable. Upstream's default is on.
- `-D hgl=false` — Builds no Haiku GL front end; the option has an effect only on Haiku.
- `-D asm=enabled` — Uses the assembly entry-point stubs for x86-64, and makes their absence a
  configure error. On musl the stubs are the TSD variant (`x86_64_tsd`): upstream builds the TLS
  stubs only on glibc and FreeBSD, because they need the initial-exec TLS model.
- `-D entrypoint-patching=enabled` — Lets a vendor library rewrite the assembly entry points with
  its own code, so that a GL call skips libGLdispatch's dispatch table. It needs the assembly
  stubs.

## Notes

`gles1=false` and Mesa's `gles1=disabled` go together: turning on one without the other gives
either a front end with no driver or a driver with no front end.
