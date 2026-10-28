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

patch -p1 -i "$PORT_SRC/largefile.patch"
patch -p1 -i "$PORT_SRC/musl-locale-l.patch"
patch -p1 -i "$PORT_SRC/no-langinfo-h.patch"

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
