## About

GIMP is the GNU Image Manipulation Program: a raster editor for photo retouching, painting,
compositing and image format conversion, built on [babl](#port-babl) and [gegl](#port-gegl) and
drawn with GTK 3. It is built with its Python plug-ins, and F1 opens the local manual that
[gimp-help](#port-gimp-help) installs under `/usr/share/gimp/3.0/help`.

## Flags

- `-Wl,-z,stack-size=8388608`, added to `LDFLAGS` — ask for an 8 MiB stack in the `PT_GNU_STACK`
  header of every executable the build links: `gimp-3.2`, `gimp-console-3.2` and each C plug-in
  executable. The Python plug-ins run under `python3` and get its header, not this one. 8 MiB is
  the size a thread gets by default on glibc, which GIMP and its plug-ins are written for, and
  musl's ceiling for the default. musl gives each new thread the largest `PT_GNU_STACK` size among
  the executable and the libraries loaded with it, and 128 KiB when none asks for more. GIMP's
  `async` worker thread allocates about 256 KiB on its stack while it converts an image's colour
  profile, so without the flag, opening any image with an embedded profile crashes GIMP, whether
  the profile is kept or converted. The executables' 8 MiB covers the worker threads of
  [babl](#port-babl) and [gegl](#port-gegl) as well, so those libraries need no flag of their own.
- `-Dcheck-update=no` — Never ask gimp.org for a newer release at start-up.
- `-Drelocatable-bundle=no` — Use the build-time paths, not paths relative to the executable.
  This is what `platform-default` resolves to off Windows and macOS, written out.
- `-Denable-default-bin=enabled` — Install unversioned links, such as `gimp`, to the versioned
  executables, so the plain name starts the program.
- `-Daa=enabled` — Each of these features is turned from `auto` into `enabled`, and its library
  is in `depends`, so a missing library fails `meson setup` instead of quietly dropping a loader or
  a controller: `-Daa` (the ASCII-art exporter, through [aalib](#port-aalib)), `-Dalsa` and `-Dgudev`
  (the MIDI and input-device controllers), `-Dcairo-pdf`, `-Dfits`, `-Dghostscript` (the
  PostScript loader, through libgs), `-Dheif`, `-Djpeg2000`, `-Djpeg-xl`, `-Dopenexr`, `-Dopenmp`,
  `-Dwebp`, `-Dxcursor` and `-Dxpm` (the X cursor and XPM file plug-ins), `-Dlinux-input` (the
  evdev controller) and `-Dbash-completion`.
- `-Dilbm=disabled` — No Amiga IFF plug-in: libilbm is not a port. `-Dmng=disabled` and
  `-Dwmf=disabled` leave out the MNG and WMF plug-ins for the same reason (libmng and libwmf).
- `-Dappdata-test=disabled` — Do not validate the AppStream file with `appstreamcli` during the
  build.
- `-Dheadless-tests=disabled` — Do not require `xvfb-run` and `dbus-run-session`, which run the
  interface tests headless. `xvfb-run` starts Xvfb, an X server from the Xorg server package, which
  the tree does not carry
  ([principles](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)).
- `-Dgi-docgen=disabled` — Build no developer API documentation; gi-docgen is not a port.
- `-Dwebkit-unmaintained=false` — Upstream's default: no WebKit help browser. The help opens in
  the web browser instead. `-Dtwain-unmaintained=false`, the 32-bit TWAIN scanner plug-in, is off
  by default too.
- `-Dvala=disabled` — Build neither the Vala API files nor the Vala plug-ins.
- `-Djavascript=disabled` — No JavaScript plug-ins: they run under gjs, which is not a port.
  `-Dlua=false`, upstream's default, likewise leaves out the Lua plug-ins, which need lua-lgi.
- `-Dlibunwind=true` — Stack traces in the debug dialog, and symbol names in the Dashboard's
  sampled backtraces, through [libunwind-nongnu](#port-libunwind-nongnu), which installs the `libunwind.pc` the
  setup looks for; LLVM's [libunwind](#port-libunwind) installs none.
- `-Dlibbacktrace=false` — libbacktrace is not a port. Without it the Dashboard's backtraces have
  no source file and line information, which `meson setup` reports as "partially detailed".

## Notes

The X11 libraries in `depends` (`libXmu`, `libXext`, `libXfixes`, `libXcursor`, `libXpm`) are
needed because [gtk3](#port-gtk3) is built with its X11 backend: GIMP then links them for picking
windows under X11, and the cursor and XPM plug-ins are built from them.

meson has no switch for the message catalogues. They are built and then removed by the
[language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy),
and GIMP shows its English source strings.
