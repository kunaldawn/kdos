## About
[WebKitGTK]{.idx} is the GTK port of the WebKit web engine, with JavaScriptCore. This port builds
it for GTK 3 against [libsoup3](#port-libsoup3), which is the `webkit2gtk-4.1` and
`javascriptcoregtk-4.1` API, and it installs `WebKitWebDriver`. GTK 3 programs with a web view use
it: [wxwidgets](#port-wxwidgets)'s `wxWebView`, and through it KiCad and PrusaSlicer; OrcaSlicer,
through the static wxWidgets it builds for itself; and GnuCash. The same source tarball builds the GTK 4 API, `webkitgtk-6.0`, as
[webkitgtk6](#port-webkitgtk6).

## Flags
- `-DUSE_GTK4=OFF` — build for GTK 3. With libsoup 3, that is the 4.1 API. wxWidgets finds its
  web view through `webkit2gtk-4.1.pc` and `webkit2gtk-web-extension-4.1.pc` and builds without
  one when they are missing, so `build.sh` checks that both are installed.
- `-DENABLE_WAYLAND_TARGET=ON` — with `-DENABLE_X11_TARGET=ON`: both windowing targets. GDK picks
  Wayland first at run time.
- `-DENABLE_BUBBLEWRAP_SANDBOX=ON` — run the web processes inside a
  [bubblewrap](#port-bubblewrap) sandbox, with D-Bus reached through
  [xdg-dbus-proxy](#port-xdg-dbus-proxy). `-DBWRAP_EXECUTABLE` and `-DDBUS_PROXY_EXECUTABLE` name
  the two programs by their installed paths rather than leaving CMake to search for them.
- `-DENABLE_WEBDRIVER=ON` — build and install `WebKitWebDriver`. It has one installed path for
  both APIs, so only this port builds it; [webkitgtk6](#port-webkitgtk6) turns it off.
- `-DENABLE_JOURNALD_LOG=OFF` — no logging to journald, which needs libsystemd or libelogind;
  there is no systemd (see [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-DENABLE_GAMEPAD=OFF` — with `-DENABLE_SPEECH_SYNTHESIS=OFF`: no Gamepad API, which needs
  libmanette, and no speech synthesis, which needs Flite or Spiel. None of the three is a port,
  and both features are on by upstream's default, with configure stopping when the library is
  missing.
- `-DUSE_WOFF2=OFF` — no libwoff2. The tree's FreeType is built with brotli and decodes WOFF2
  fonts itself.
- `-DUSE_SYSTEM_SYSPROF_CAPTURE=OFF` — use the copy of `libsysprof-capture` WebKit carries;
  sysprof is not a port.
- `-DUSE_LIBBACKTRACE=OFF` — no libbacktrace, which is not a port.
- `-DUSE_VULKAN=OFF` — no Vulkan support, which needs the Volk loader, not a port.
- `-DENABLE_SWIFT_DEMO_URI_SCHEME=OFF` — with `-DENABLE_BACK_FORWARD_LIST_SWIFT=OFF`: the
  features written in Swift need `swiftc`, which is not a port, and gcc cannot build them.
- `-DUSE_SYSTEM_UNIFDEF=ON` — use the [unifdef](#port-unifdef) port rather than building WebKit's
  copy.
- `-DUSE_GSTREAMER=ON` — with `-DENABLE_VIDEO=ON` and `-DENABLE_WEB_AUDIO=ON`: HTML media and Web
  Audio through [GStreamer](#port-gstreamer) and its plugin sets.
- `-DUSE_GBM=ON` — with `-DUSE_LIBDRM=ON`, `-DUSE_LIBHYPHEN=ON`, `-DUSE_LIBSECRET=ON`,
  `-DUSE_AVIF=ON`, `-DUSE_JPEGXL=ON`, `-DUSE_LCMS=ON` and `-DENABLE_SPELLCHECK=ON`: buffers shared
  with the GPU through GBM and libdrm, hyphenation, password storage through libsecret, AVIF and
  JPEG XL images, colour management, and spell checking through enchant. Each is on by upstream's
  default and stops configure when its library is missing; naming it keeps the feature from
  following a changed default.
- `-DENABLE_MINIBROWSER=OFF` — with `-DENABLE_API_TESTS=OFF` and `-DENABLE_DOCUMENTATION=OFF`:
  no MiniBrowser test program, API tests or gi-docgen reference.
- `-DCMAKE_SKIP_RPATH=ON` — no run path in the libraries and programs; everything they link is in
  `/usr/lib`.
- `-fuse-ld=lld` — added to `LDFLAGS`: link with LLVM's [lld](#port-lld) instead of GNU ld.

## Patches
- `webdriver-release-log.patch` — puts the request-logging block of `WebDriverService::handleRequest` under `#if !RELEASE_LOG_DISABLED`. With journald logging off, release logging is off, and `LOG` is off in a release build, so WebDriver's log channels are not declared, and the unguarded `LOG_CHANNEL(WebDriverClassic)` does not compile.
- `x11-include-order.patch` — moves `#include <X11/Xlib.h>` after the WebKit headers in `PointerLockManagerX11.cpp`. Macros from `X11/X.h` collide with definitions in WebKit's bmalloc code, so Xlib must come after them.

## Notes
`webkitgtk` and `webkitgtk6` are built from the same tarball, and their `build.sh` files differ
only in `-DUSE_GTK4`, `-DENABLE_WEBDRIVER`, the WebDriver patch and the two `.pc` checks.
