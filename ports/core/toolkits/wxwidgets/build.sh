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

# musl has no strtol_l family and no locale data behind nl_langinfo(): two
# patches map the first to the plain C calls and send wx through its own
# locale tables for the second. The others keep matrix.h valid C++ under the
# deprecation macro and define HAVE_LARGEFILE_SUPPORT, which the CMake probe
# leaves unset.
patch -p1 -i "$PORT_SRC/invalid-header-syntax.patch"
patch -p1 -i "$PORT_SRC/largefile.patch"
patch -p1 -i "$PORT_SRC/musl-locale-l.patch"
patch -p1 -i "$PORT_SRC/no-langinfo-h.patch"

# GTK 3 with both GDK backends: wx links libX11 because GDK has an X11 backend,
# and picks Wayland or X11 at run time from the display GDK opened. The GL
# canvas is EGL on both, which is the only wxGLCanvas that works on Wayland.
#
# Every optional backend wx probes for is named ON because a failed probe turns
# the feature off with only a warning; the checks after the install turn that
# warning into a failed build. The webview is WebKitGTK's 4.1 API (KiCad needs
# it), the media control is GStreamer, wxSound is SDL (through sdl2-compat to
# PipeWire; wx's other Unix sound backend is OSS, which has no device here),
# wxSecretStore is libsecret, text-control spell checking is gspell, CHM help
# is libmspack. The third-party image, XML, regex and zlib code wx bundles is
# replaced by the system libraries. The tree already has a build/ directory,
# holding wx's own build systems, so the CMake build directory is _build.
cmake -S . -B _build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DwxBUILD_TOOLKIT=gtk3 \
	-DwxBUILD_SHARED=ON \
	-DwxBUILD_PRECOMP=OFF \
	-DwxBUILD_TESTS=OFF \
	-DwxBUILD_SAMPLES=OFF \
	-DwxBUILD_DEMOS=OFF \
	-DwxUSE_SYS_LIBS=ON \
	-DwxUSE_REGEX=sys \
	-DwxUSE_ZLIB=sys \
	-DwxUSE_EXPAT=sys \
	-DwxUSE_LIBJPEG=sys \
	-DwxUSE_LIBPNG=sys \
	-DwxUSE_LIBTIFF=sys \
	-DwxUSE_LIBLZMA=ON \
	-DwxUSE_OPENGL=ON \
	-DwxUSE_GLCANVAS_EGL=ON \
	-DwxUSE_GTKPRINT=ON \
	-DwxUSE_PRIVATE_FONTS=ON \
	-DwxUSE_WEBVIEW=ON \
	-DwxUSE_WEBVIEW_WEBKIT=ON \
	-DwxUSE_MEDIACTRL=ON \
	-DwxUSE_SOUND=ON \
	-DwxUSE_LIBSDL=ON \
	-DwxUSE_LIBNOTIFY=ON \
	-DwxUSE_SECRETSTORE=ON \
	-DwxUSE_SPELLCHECK=ON \
	-DwxUSE_LIBMSPACK=ON \
	-DwxUSE_XTEST=ON \
	-DwxUSE_WEBREQUEST_CURL=ON \
	-DwxUSE_DETECT_SM=OFF \
	-DwxUSE_LIBGNOMEVFS=OFF
ninja -C _build
DESTDIR=$PKG ninja -C _build install

install -Dm644 wxwin.m4 "$PKG/usr/share/aclocal/wxwin.m4"

for lib in webview gl media; do
	test -e "$PKG/usr/lib/libwx_gtk3u_$lib-3.2.so"
done
test -e "$PKG/usr/lib/wx/3.2/web-extensions/webkit2_extu-3.2.so"
for sym in wxUSE_GLCANVAS_EGL wxUSE_LIBSDL wxUSE_LIBNOTIFY wxUSE_SECRETSTORE wxUSE_SPELLCHECK wxUSE_LIBMSPACK wxUSE_WEBREQUEST_CURL; do
	grep -q "^#define $sym 1" "$PKG"/usr/lib/wx/include/gtk3-unicode-*/wx/setup.h
done
