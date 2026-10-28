## About

gst-plugins-base is GStreamer's base set: the libraries every other plugin set builds on
(`gstreamer-audio`, `-video`, `-pbutils`, `-tag`, `-rtp`, `-rtsp`, `-sdp`, `-app`, `-allocators`
and the OpenGL integration library `gstreamer-gl`) and the elements most pipelines need: playbin
and decodebin, audio and video conversion and scaling, Ogg, Vorbis, Opus and Theora, Pango text
overlays, ALSA, and the OpenGL elements. It is built on [gstreamer](#port-gstreamer) and drawn
through Wayland, EGL and DRM only.

## Flags

- `--libexecdir=/usr/lib` — The directory every GStreamer port in the tree passes. Here it sets
  the `pluginscannerdir` the `.pc` files report, `/usr/lib/gstreamer-1.0`, where the core installs
  `gst-plugin-scanner`, and the path compiled into `gstreamer-pbutils` for the missing-plugin
  installer, `/usr/lib/gst-install-plugins-helper`.
- `-Dintrospection=enabled` — Installs the `GstAudio`, `GstVideo`, `GstPbutils`, `GstGL` and
  sibling GIRs, which gst-plugins-bad's GIRs include.
- `-Ddoc=disabled`, `-Dexamples=disabled`, `-Dtests=disabled` — No documentation, examples or
  tests.
- `-Dnls=disabled` — Builds and installs no translated messages. KDOS ships English only
  (see [the size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Dx11=disabled`, `-Dxshm=disabled`, `-Dxvideo=disabled`, `-Dxi=disabled` — No `ximagesink` or
  `xvimagesink`, and no X11 support in the libraries or the OpenGL library. On `auto` each would
  follow whether [libX11](#port-libX11), libXext, libXv and libXi happened to be in the root; none
  of them is in `depends`.
- `-Dqt5=disabled` — Builds no Qt 5 QML examples. Qt 5 is built in a later phase, `43_toolkits`.
- `-Dgl=enabled` — Builds `gstreamer-gl` and the OpenGL plugin (`glimagesink`, `glupload`,
  `glcolorconvert` and the other `gl*` elements).
- `-Dgl_api=opengl,gles2` — Desktop OpenGL and OpenGL ES 2 and later, from
  [libglvnd](#port-libglvnd) and [mesa](#port-mesa).
- `-Dgl_platform=egl` — EGL only, no GLX, so the GL library never needs X11.
- `-Dgl_winsys=wayland,egl,surfaceless,gbm` — The window systems the GL library can open a context
  on: a Wayland surface, a bare EGL display, a surfaceless context for offscreen work, and GBM on a
  DRM device with no compositor.
- `-Dgl-jpeg=enabled`, `-Dgl-png=enabled`, `-Dgl-graphene=enabled` — The GL elements that need a
  library: `gldifferencematte` (libpng), `gloverlay` (libpng and libjpeg), and `gltransformation`
  and `glvideoflip` ([graphene](#port-graphene)).
- `-Ddrm=enabled` — Builds against [libdrm](#port-libdrm): the DRM dumb-buffer allocator in
  `gstreamer-allocators`, and the DRM format modifiers the GL library uses to import and export
  DMA-bufs through EGL.
- `-Dalsa=enabled`, `-Dogg=enabled`, `-Dvorbis=enabled`, `-Dopus=enabled`, `-Dtheora=enabled`,
  `-Dpango=enabled` — ALSA input and output; Ogg demuxing and muxing; Vorbis, Opus and Theora; and
  the Pango text overlays. `enabled` makes each library a requirement rather than a probe.
- `-Diso-codes=enabled` — Language names in `gstreamer-tag` from [iso-codes](#port-iso-codes).
- `-Dorc=enabled` — Builds the SIMD inner loops with ORC, through [orc](#port-orc).
- `-Dorc-compiler=disabled` — Uses the pre-generated C that the tarball carries for each ORC file,
  instead of regenerating it with `orcc`.
- `-Dtremor=disabled`, `-Dcdparanoia=disabled`, `-Dlibvisual=disabled` — Tremor (the integer Vorbis
  decoder), cdparanoia (audio CD ripping) and libvisual (visualisations) are not ports.
