## About

Pillow is the Python imaging library: it opens, converts, transforms and saves raster images in
the common formats, and draws text and shapes on them. Python programs import it as `PIL`. Its
codecs link the image library ports named in `depends`.

## Flags

- `export CFLAGS="$CFLAGS -flto=auto"` — Link-time optimisation for the imaging modules, one of
  the few hot libraries the tree builds with LTO; see the LTO rule under
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port).
- `export AR=gcc-ar NM=gcc-nm RANLIB=gcc-ranlib` — The archive tools that load GCC's LTO plugin.
  Pillow compiles `Mode.c` into a static library, `pil_imaging_mode`, that the `_imaging`
  extension links. Under `-flto` its object holds only LTO bytecode, which `ar` indexes only
  through GCC's plugin, and because an extension module may leave symbols undefined the link still
  succeeds; the failure appears at `import PIL.Image`, on `getRawModeData`.
- `-C tiff=enable`, `-C freetype=enable`, `-C raqm=enable`, `-C lcms=enable`, `-C webp=enable`,
  `-C jpeg2000=enable`, `-C imagequant=enable`, `-C avif=enable` — Require each optional codec
  and feature: TIFF, FreeType text, complex text layout through the system
  [libraqm](#port-libraqm), colour management through [lcms2](#port-lcms2), WebP, JPEG 2000
  through [openjpeg](#port-openjpeg), palette quantisation through
  [libimagequant](#port-libimagequant), and AVIF. Upstream's default is to build each one only
  when its library is found; `enable` makes a missing library a failed build, so the feature set
  does not follow whatever the build root happens to hold. JPEG and zlib are required by Pillow
  in any case.
- `-C xcb=disable` — Leave out `ImageGrab`'s X11 screen capture, which links `libxcb`. The
  Wayland compositor owns the screen, and the one X server, Xwayland, runs rootless for X11
  clients only
  ([principles](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)),
  so there is no X screen holding the desktop to grab.
- `--no-build-isolation` — Build with the setuptools and pybind11 already in the build root; both
  are in `depends`. An isolated build would try to fetch them, and the build has no network.
