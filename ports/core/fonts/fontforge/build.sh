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

mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
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
	-DENABLE_LIBSPIRO=ON \
	-DENABLE_WOFF2=ON \
	-DENABLE_HARFBUZZ=ON \
	-DENABLE_DOCS=OFF
make
make DESTDIR=$PKG install

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
