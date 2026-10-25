## About
This is [WebKitGTK]{.idx} built for GTK 4: the `webkitgtk-6.0` and `javascriptcoregtk-6.0` APIs,
for GTK 4 programs with a web view. It is built from the same source tarball as
[webkitgtk](#port-webkitgtk), the GTK 3 build, with the same switches apart from `-DUSE_GTK4` and
`-DENABLE_WEBDRIVER`; that port's page explains every switch, the switches that turn a feature off
are repeated below, and the two ports install side by side.

## Flags
- `-DUSE_GTK4=ON` — build for GTK 4, which is the 6.0 API.
- `-DENABLE_WEBDRIVER=OFF` — do not build `WebKitWebDriver`. It has one installed path for both
  APIs, so only the GTK 3 build installs it.
- `-DENABLE_JOURNALD_LOG=OFF` — no logging to journald, which needs libsystemd or libelogind;
  there is no systemd.
- `-DENABLE_GAMEPAD=OFF` — with `-DENABLE_SPEECH_SYNTHESIS=OFF`: no Gamepad API (libmanette) and
  no speech synthesis (Flite or Spiel); none of the three is a port.
- `-DUSE_WOFF2=OFF` — no libwoff2; the tree's FreeType decodes WOFF2 fonts itself.
- `-DUSE_SYSTEM_SYSPROF_CAPTURE=OFF` — use WebKit's own copy of `libsysprof-capture`; sysprof is
  not a port.
- `-DUSE_VULKAN=OFF` — no Vulkan support, which needs the Volk loader, not a port.
- `-DENABLE_SWIFT_DEMO_URI_SCHEME=OFF` — with `-DENABLE_BACK_FORWARD_LIST_SWIFT=OFF`: the Swift
  features need `swiftc`, which is not a port.
- `-fuse-ld=lld` — added to `LDFLAGS`: link with LLVM's [lld](#port-lld) instead of GNU ld.

## Patches
- `x11-include-order.patch` — moves `#include <X11/Xlib.h>` after the WebKit headers in `PointerLockManagerX11.cpp`. Macros from `X11/X.h` collide with definitions in WebKit's bmalloc code, so Xlib must come after them.

## Notes
With WebDriver off, the WebDriver source is not compiled, so this port does not carry the GTK 3
build's `webdriver-release-log.patch`.
