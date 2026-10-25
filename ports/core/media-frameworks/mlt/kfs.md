## About
[MLT]{.idx} is the multimedia framework that video editors are built on: a library, `libmlt` with
its C++ wrapper `libmlt++`, that composes producers, filters, transitions and consumers into a
timeline and renders it, plus the `melt` command that plays or renders an MLT XML project from the
command line. Almost everything it can do lives in modules, loadable libraries under
`/usr/lib/mlt-7`, each wrapping one library or one family of effects. In this tree
[kdenlive](#port-kdenlive), [shotcut](#port-shotcut) and [krita](#port-krita) link it.

## Flags
- `-DCMAKE_DISABLE_FIND_PACKAGE_JACK=ON` — make CMake's `find_package(JACK)` fail whatever is
  installed. The `jackrack` directory then builds no `mltjackrack` module, which is MLT's JACK
  consumer and JACK-hosted LADSPA rack, and still builds `mltladspa`, which hosts LADSPA, LV2 and
  VST2 plugins inside MLT itself. [pipewire](#port-pipewire) installs a JACK library and `jack.pc`
  in an earlier phase and is not in `depends`, so without this switch the package would carry a
  module linking a library it does not declare.
- `-DGPL=ON` — with `-DGPL3=ON`: allow the GPL-licensed modules. With `GPL` off, upstream turns off
  normalize, openfx, plusgpl, qt6, resample, rubberband, vidstab and xine, and LADSPA hosting is a
  configuration error; `GPL3` adds the `vqm` video-quality transition to the Qt module. Both are
  upstream's defaults, named.
- `-DCLANG_FORMAT=OFF` — no formatting target, which looks for `clang-format` version 14 exactly.
  It formats source and builds nothing; upstream's default is on.
- `-DBUILD_DOCS=OFF` — no Doxygen API reference. Upstream's default, named.
- `-DMOD_AVFORMAT=ON` — the FFmpeg module, MLT's decoder and encoder for nearly every format, with
  `-DUSE_AVDEVICE=ON` adding libavdevice's capture inputs. Both are upstream's defaults, and the
  module makes [ffmpeg](#port-ffmpeg) a hard requirement.
- `-DMOD_DECKLINK=OFF` — no module for Blackmagic Design DeckLink capture and playback cards. The
  module carries the SDK headers but loads `libDeckLinkAPI.so`, Blackmagic's driver library, at run
  time, and nothing in this tree provides it. Upstream's default is on.
- `-DMOD_NDI=OFF` — no NewTek NDI network video module, which needs the NDI SDK (not a port).
  Upstream's default, named.
- `-DMOD_GLAXNIMATE_QT6=ON` — the producer that plays Glaxnimate and Lottie vector animations.
  It compiles the copy of Glaxnimate's own source that ships under
  `src/modules/glaxnimate/glaxnimate`, against Qt 6. Off by upstream's default.
- `-DMOD_OPENCV=ON` — the `opencv.tracker` filter, which follows an object across frames. It needs
  OpenCV's `tracking` module, which [opencv](#port-opencv) builds from its contrib sources. Off by
  upstream's default.
- `-DMOD_SPATIALAUDIO=ON` — the ambisonic audio filters, through
  [libspatialaudio](#port-libspatialaudio). Off by upstream's default.
- `-DMOD_JACKRACK=ON` — with `-DUSE_LV2=ON` and `-DUSE_VST2=ON`: the `jackrack` directory, where
  LADSPA hosting lives, with LV2 plugins through [lilv](#port-lilv) and VST2 plugins through MLT's
  own host, written against the `vestige.h` header it carries. LADSPA hosting itself, `USE_LADSPA`, is left at its default of on, and
  the [ladspa](#port-ladspa) header must be present or configuration stops. All are upstream's
  defaults, named.
- `-DMOD_MOVIT=ON` — the GPU effects module, through [movit](#port-movit). It requires OpenGL and,
  on Linux, X11's client library, which is why [libglvnd](#port-libglvnd) and [libX11](#port-libX11)
  are in `depends`.
- `-DMOD_QT6=ON` — the Qt 6 module: image and text producers, Kdenlive's title producer, the
  audio visualisation filters, and Qt-drawn blend, crop, shadow and GPS-overlay effects. `-DMOD_GDK=ON` is its GdkPixbuf and Pango counterpart, an image
  and text producer of its own.
- `-DMOD_RTAUDIO=ON` — the audio consumer through [rtaudio](#port-rtaudio). When `rtaudio.pc` is
  missing, upstream falls back to its own copy of RtAudio over ALSA and PulseAudio.
- `-DMOD_SDL1=OFF` — with `-DMOD_SDL2=ON`: build the SDL 2 preview consumer, not the SDL 1.2 one.
  Both are upstream's defaults, named.
- `-DMOD_XINE=ON` — MLT's deinterlacing module, whose yadif and other deinterlacers come from xine
  and are compiled from MLT's own source; it links no xine library.
- `-DMOD_FREI0R=ON` — and the remaining `MOD_` switches (`KDENLIVE`, `NORMALIZE`, `OLDFILM`,
  `OPENFX`, `PLUS`, `PLUSGPL`, `RESAMPLE`, `RUBBERBAND`, `RNNOISE`, `SOX`, `VIDSTAB`, `VORBIS`,
  `XML`): each module at upstream's default of on, named. Upstream requires the library of each
  that has one (frei0r, libsamplerate, rubberband, rnnoise, SoX, vid.stab, Vorbis, libxml2), so a
  missing one stops the configure, and each is in `depends`. The Plus module links
  [libebur128](#port-libebur128) when pkg-config finds it and compiles its own copy otherwise.
- `-DSWIG_PYTHON=OFF` — no Python bindings, the `mlt7` module. Upstream's default, named.

## Patches
- `melt-include-missing-system-headers.patch` — adds `<sys/time.h>` and `<sys/select.h>` to `src/melt/io.c`, which uses `struct timeval`, `FD_ZERO`, `FD_SET` and `select()` without including the headers that declare them.
- `mlt-man-link-destdir.patch` — makes the install step that links `melt.1` to `melt-<major>.1` run inside `$DESTDIR`. Upstream runs it in the live `/usr/share/man/man1`, so a staged install would write outside the package and leave the package without the link.

## Notes
`build.sh` ends with `test -L "$PKG/usr/share/man/man1/melt.1"`, which fails the build if the
package lacks the man page link that the second patch makes.

The libplacebo module is not named in `build.sh`. Upstream's `MOD_PLACEBO` defaults to on and
builds the module only when pkg-config finds a recent enough `libplacebo`, and
[libplacebo](#port-libplacebo) is installed in an earlier phase but is not in `depends`. The module
is in the package when libplacebo was installed at build time, and is left out without an error
when it was not.
