## About
[wxWidgets]{.idx} is a C++ toolkit that draws each widget with the platform's own: on Linux, GTK.
This port builds it on [GTK 3](#port-gtk3) as shared libraries, with the `wx-config` script,
`wxwin.m4` for autoconf projects and the CMake package. KiCad, PrusaSlicer, Audacity, Tenacity,
FileZilla, VeraCrypt, OpenCPN and FreeDV are wx applications; like every toolkit it is for
applications only, never the desktop (see
[Toolkits are for applications, not the desktop](../../../../docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).

## Flags
- `-DwxBUILD_TOOLKIT=gtk3` — the GTK 3 port of wx. It links `libX11` because GDK has an X11
  backend, and runs on Wayland or X11 according to the display GDK opened.
- `-DwxUSE_SYS_LIBS=ON` — with `-DwxUSE_REGEX=sys`, `-DwxUSE_ZLIB=sys`, `-DwxUSE_EXPAT=sys`,
  `-DwxUSE_LIBJPEG=sys`, `-DwxUSE_LIBPNG=sys` and `-DwxUSE_LIBTIFF=sys`: use the installed
  [pcre2](#port-pcre2), [zlib](#port-zlib), [expat](#port-expat),
  [libjpeg-turbo](#port-libjpeg-turbo), [libpng](#port-libpng) and [libtiff](#port-libtiff)
  instead of the copies wx carries in `src/` and `3rdparty/pcre`.
- `-DwxUSE_GLCANVAS_EGL=ON` — with `-DwxUSE_OPENGL=ON`: build `wxGLCanvas` on EGL. The canvas is
  chosen when wx is compiled: the EGL one works on Wayland and on X11, the GLX one only on X11.
  configure turns this off without a message when it finds no EGL.
- `-DwxUSE_WEBVIEW=ON` — with `-DwxUSE_WEBVIEW_WEBKIT=ON`: `wxWebView` on
  [WebKitGTK](#port-webkitgtk)'s 4.1 API; KiCad's build requires the `webview` library. CMake
  looks for 4.1 first and falls back to 4.0; the 4.1 API is the one built against
  [libsoup3](#port-libsoup3). On GTK 3 wx records the backend it found as
  `wxUSE_WEBVIEW_WEBKIT2`, and `wxUSE_WEBVIEW_WEBKIT` is 0 in the installed `setup.h`.
- `-DwxUSE_MEDIACTRL=ON` — `wxMediaCtrl` on [GStreamer](#port-gstreamer).
- `-DwxUSE_SOUND=ON` — with `-DwxUSE_LIBSDL=ON`: `wxSound` plays through SDL, here
  [sdl2-compat](#port-sdl2-compat), which reaches PipeWire. wx's other Unix sound backend is OSS
  on `/dev/dsp`, which exists only while the kernel's `snd-pcm-oss` module is loaded, and nothing
  in KDOS loads it.
- `-DwxUSE_LIBNOTIFY=ON` — notifications through [libnotify](#port-libnotify).
- `-DwxUSE_SECRETSTORE=ON` — `wxSecretStore`, which keeps passwords, on
  [libsecret](#port-libsecret).
- `-DwxUSE_SPELLCHECK=ON` — spell checking in `wxTextCtrl`, through [gspell](#port-gspell).
- `-DwxUSE_LIBMSPACK=ON` — read CHM help files with [libmspack](#port-libmspack).
- `-DwxUSE_WEBREQUEST_CURL=ON` — `wxWebRequest` on [curl](#port-curl).
- `-DwxUSE_LIBLZMA=ON` — LZMA compression streams, with [xz](#port-xz)'s liblzma.
- `-DwxUSE_XTEST=ON` — `wxUIActionSimulator`, which synthesises input; with GTK 3 it cannot
  work without the XTest extension ([libXtst](#port-libXtst)).
- `-DwxUSE_GTKPRINT=ON` — printing through GTK's print dialog.
- `-DwxUSE_PRIVATE_FONTS=ON` — let an application load a font file it ships, through fontconfig
  and PangoFT2.
- `-DwxUSE_LIBGNOMEVFS=OFF` — no GNOME VFS for associating MIME types; GNOME VFS is not a port.
- `-DwxUSE_DETECT_SM=OFF` — no code that looks for an X11 session manager. Upstream's default,
  named.
- `-DwxBUILD_PRECOMP=OFF` — compile without precompiled headers, which upstream uses by default.
- `-DwxBUILD_SAMPLES=OFF` — with `-DwxBUILD_TESTS=OFF` and `-DwxBUILD_DEMOS=OFF`: build only the
  libraries.

## Patches
- `invalid-header-syntax.patch` — in `include/wx/matrix.h`, moves `class` from before `wxDEPRECATED_EXPORT_CORE(...)` to after it, so the macro's attributes precede the class key. GCC ignores attributes in that position with a `-Wattributes` warning: the deprecated `wxTransformMatrix` loses its deprecation and its default visibility, and `libwx_gtk3u_core` exports none of its members.
- `largefile.patch` — makes `HAVE_LARGEFILE_SUPPORT` an unconditional `#define 1` in CMake's `setup.h.in`. On Unix `init.cmake` already sets it whenever `wxBUILD_LARGEFILE_SUPPORT` is on, which is upstream's default and is not changed here, so the generated `setup.h` differs only if that option is turned off.
- `musl-locale-l.patch` — in `include/wx/xlocale.h`, defines `strtol_l`, `strtoul_l`, `wcstod_l`, `wcstol_l` and `wcstoul_l` as the plain C calls without the locale argument when they are not macros. musl has none of the five.
- `no-langinfo-h.patch` — removes the `langinfo.h` probe and its `HAVE_LANGINFO_H` line from CMake's setup, so wx compiles its locale code without `nl_langinfo()`: number separators come from `localeconv()`, and `wxLocale::GetInfo` returns an empty string for the date and time formats.

## Notes
Naming an optional backend `ON` does not make configure require it. When a probe fails, configure
turns the feature off and carries on, with a warning, a status line or no message at all;
`wxSecretStore` is the one exception, and stops configure when libsecret is missing. The checks
at the end of `build.sh` turn a lost backend into a failed build: the webview, GL and
media libraries and the WebKit web extension must be installed, and `setup.h` must define each of
`wxUSE_GLCANVAS_EGL`, `wxUSE_LIBSDL`, `wxUSE_LIBNOTIFY`, `wxUSE_SECRETSTORE`, `wxUSE_SPELLCHECK`,
`wxUSE_LIBMSPACK` and `wxUSE_WEBREQUEST_CURL` as 1.

The source tree already has a `build/` directory, holding wx's own build systems, so the CMake
build directory is `_build`.
