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


# ENABLE_GUI=ON: the editor window, which is GTK 3 and gtkmm. FontForge allows
# GDK only its X11 backend, so the window is an Xwayland client and needs
# gtk3's X11 backend. The `fontforge` command still runs native (`-script`)
# and Python scripts, and the `fontforge` Python module the font ports import
# to compile faces from their drawing sources is built beside it.
#
# Every optional library is named ON or OFF, because an AUTO option that
# misses its library builds a narrower fontforge without a word. harfbuzz is
# the editor's metrics-view shaper and woff2 reads and writes WOFF2 faces;
# libspiro is not a port, so there are no Spiro curves. The HTML manual needs
# Sphinx and is not built; the four manual pages install regardless.
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DENABLE_GUI=ON \
	-DENABLE_NATIVE_SCRIPTING=ON \
	-DENABLE_PYTHON_SCRIPTING=ON \
	-DENABLE_PYTHON_EXTENSION=ON \
	-DENABLE_LIBGIF=ON \
	-DENABLE_LIBJPEG=ON \
	-DENABLE_LIBPNG=ON \
	-DENABLE_LIBTIFF=ON \
	-DENABLE_LIBREADLINE=ON \
	-DENABLE_LIBSPIRO=OFF \
	-DENABLE_WOFF2=ON \
	-DENABLE_HARFBUZZ=ON \
	-DENABLE_DOCS=OFF
make
make DESTDIR=$PKG install

# Upstream's entry is replaced for its MimeType line: it claims TrueType and
# OpenType, which font-manager's viewer claims as well, and a double-clicked
# font is something to look at before it is something to edit. FontForge keeps
# its own .sfd and the formats nothing else opens. The icons are upstream's
# hicolor PNGs. Under Xwayland the compositor takes the WM_CLASS instance as
# the app_id, and GDK makes that the program name, `fontforge`.
cat > "$PKG/usr/share/applications/org.fontforge.FontForge.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=FontForge
GenericName=Font Editor
Comment=Draw, edit and convert outline and bitmap fonts
Exec=fontforge %F
TryExec=fontforge
Icon=org.fontforge.FontForge
Terminal=false
StartupWMClass=fontforge
Categories=Graphics;
MimeType=application/vnd.font-fontforge-sfd;application/x-font-type1;application/x-font-bdf;application/x-font-pcf;application/x-font-tex;font/woff;font/woff2;
Keywords=font;fonts;editor;glyph;typeface;TTF;OTF;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.fontforge.FontForge.desktop"
