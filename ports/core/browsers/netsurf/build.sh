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

mkdir -p "$SRC_ROOT/shim/libutf8proc"
ln -sf /usr/include/utf8proc.h "$SRC_ROOT/shim/libutf8proc/utf8proc.h"

export CFLAGS="$CFLAGS -fcommon -g0 -I$SRC_ROOT/shim"

_opts=(
	TARGET=gtk3 PREFIX=/usr LIBDIR=lib INCLUDEDIR=include
	NETSURF_USE_CURL=YES NETSURF_USE_OPENSSL=YES
	NETSURF_USE_UTF8PROC=YES NETSURF_USE_DUKTAPE=YES
	NETSURF_USE_JPEG=YES NETSURF_USE_PNG=YES NETSURF_USE_WEBP=YES
	NETSURF_USE_JPEGXL=YES NETSURF_USE_BMP=YES NETSURF_USE_GIF=YES
	NETSURF_USE_RSVG=YES NETSURF_USE_NSSVG=YES
	NETSURF_USE_NSPSL=YES NETSURF_USE_NSLOG=YES
	NETSURF_USE_ROSPRITE=NO NETSURF_USE_HARU_PDF=NO NETSURF_USE_VIDEO=NO
	NETSURF_USE_GRESOURCE=YES
	GTK_TRANSLATIONS_HTML=en
)
make "${_opts[@]}"
make "${_opts[@]}" DESTDIR=$PKG install
install -Dm644 docs/netsurf-gtk.1 "$PKG/usr/share/man/man1/netsurf-gtk3.1"

install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
magick frontends/gtk/res/netsurf.xpm -background none -gravity center \
	-extent 136x136 -resize 128x128 \
	"$PKG/usr/share/icons/hicolor/128x128/apps/netsurf.png"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/netsurf.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=NetSurf
GenericName=Web Browser
Comment=A small, fast web browser for slow machines
Exec=netsurf-gtk3 %u
Icon=netsurf
Terminal=false
StartupWMClass=netsurf-gtk3
Categories=Network;WebBrowser;
Keywords=web;browser;internet;www;http;light;
DESKTOP
chmod 644 "$PKG/usr/share/applications/netsurf.desktop"
