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

# NetSurf includes <libutf8proc/utf8proc.h>, the layout of its own utf8proc
# fork; the utf8proc port installs the same library's header at the top of the
# include path. A directory with that one name in it, first on the include
# path, lets the IDNA code link the port instead of losing its normalisation.
mkdir -p "$SRC_ROOT/shim/libutf8proc"
ln -sf /usr/include/utf8proc.h "$SRC_ROOT/shim/libutf8proc/utf8proc.h"

# The environment's CFLAGS are appended after NetSurf's own. -fcommon lets
# tentative definitions shared between objects link, which GCC otherwise
# refuses as multiple definitions.
export CFLAGS="$CFLAGS -fcommon -I$SRC_ROOT/shim"

# Every feature is named YES, so a missing library fails the build instead of
# quietly narrowing the browser. SVG is drawn by librsvg, which NetSurf prefers
# over libsvgtiny when both are built. Haru PDF export, RISC OS sprites and the
# GStreamer 0.10 video handler are off: none has a port. GTK_TRANSLATIONS_HTML
# installs the English welcome and credits pages only; the interface messages
# are compiled into the binary for every language and cannot be narrowed.
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

# The only application picture upstream ships for GTK is a 132x135 XPM, which
# the panel cannot read; it is centred on a square and scaled to 128 px.
install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
magick frontends/gtk/res/netsurf.xpm -background none -gravity center \
	-extent 136x136 -resize 128x128 \
	"$PKG/usr/share/icons/hicolor/128x128/apps/netsurf.png"

# UPSTREAM'S ENTRY IS NOT INSTALLED by its makefile and names netsurf-gtk, a
# binary the install does not create. GTK takes the Wayland app_id from the
# program name, netsurf-gtk3. No MimeType: Firefox ESR's entry is the web
# handler, and a second claim would make the default whichever entry sorts
# first.
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
