#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# Xlib's macros collide with bmalloc's names in a unified source, so Xlib.h
# must be included after the WebKit headers in the X11 pointer-lock manager.
patch -p1 -i "$PORT_SRC/x11-include-order.patch"

# Release logging is off here (no journald, and LOG is off in a release
# build), so WebDriver's log channels are not declared; its request handler
# still tests LOG_CHANNEL(WebDriverClassic) outside any guard. The patch puts
# that block under #if !RELEASE_LOG_DISABLED, as WebKit's main branch does.
patch -p1 -i "$PORT_SRC/webdriver-release-log.patch"

# Both windowing targets are built and GDK picks Wayland first at run time.
# FreeType is built with brotli and decodes WOFF2 itself, so libwoff2 stays
# unused. Gamepads need libmanette and speech synthesis needs Flite or Spiel;
# none is a port, so both features are off rather than found missing hours
# in. journald logging is off: there is no systemd. The bundled
# libsysprof-capture is used because sysprof is not a port. WebKitWebDriver
# has one path for both APIs, so only the GTK 3 build installs it. The Swift
# features need swiftc, which is not a port, and gcc cannot build them.
export LDFLAGS="$LDFLAGS -fuse-ld=lld"
cmake -S . -B build -G Ninja \
	-DPORT=GTK \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_SKIP_RPATH=ON \
	-DUSE_GTK4=OFF \
	-DENABLE_WAYLAND_TARGET=ON \
	-DENABLE_X11_TARGET=ON \
	-DENABLE_INTROSPECTION=ON \
	-DENABLE_DOCUMENTATION=OFF \
	-DENABLE_JOURNALD_LOG=OFF \
	-DENABLE_BUBBLEWRAP_SANDBOX=ON \
	-DBWRAP_EXECUTABLE=/usr/bin/bwrap \
	-DDBUS_PROXY_EXECUTABLE=/usr/bin/xdg-dbus-proxy \
	-DENABLE_MINIBROWSER=OFF \
	-DENABLE_WEBDRIVER=ON \
	-DENABLE_API_TESTS=OFF \
	-DENABLE_GAMEPAD=OFF \
	-DENABLE_SPEECH_SYNTHESIS=OFF \
	-DENABLE_SPELLCHECK=ON \
	-DENABLE_VIDEO=ON \
	-DENABLE_WEB_AUDIO=ON \
	-DUSE_GSTREAMER=ON \
	-DUSE_GBM=ON \
	-DUSE_LIBDRM=ON \
	-DUSE_LIBHYPHEN=ON \
	-DUSE_LIBSECRET=ON \
	-DUSE_AVIF=ON \
	-DUSE_JPEGXL=ON \
	-DUSE_LCMS=ON \
	-DUSE_WOFF2=OFF \
	-DUSE_LIBBACKTRACE=OFF \
	-DUSE_SYSTEM_SYSPROF_CAPTURE=OFF \
	-DUSE_SYSTEM_UNIFDEF=ON \
	-DUSE_VULKAN=OFF \
	-DENABLE_SWIFT_DEMO_URI_SCHEME=OFF \
	-DENABLE_BACK_FORWARD_LIST_SWIFT=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# USE_GTK4=OFF with libsoup 3 is the 4.1 API. wxWidgets finds its web view
# through these two files and builds without one when they are absent.
test -f "$PKG/usr/lib/pkgconfig/webkit2gtk-4.1.pc"
test -f "$PKG/usr/lib/pkgconfig/webkit2gtk-web-extension-4.1.pc"

# English only: the translations of the engine's own strings are not shipped.
rm -rf "$PKG/usr/share/locale"
