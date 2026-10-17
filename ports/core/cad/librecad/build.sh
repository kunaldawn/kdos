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

patch -p1 -i "$PORT_SRC/no-execinfo.patch"

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr
ninja
DESTDIR=$PKG ninja install
cd ..

install -d "$PKG/usr/lib/librecad"
mv "$PKG/usr/bin/resources/plugins" "$PKG/usr/lib/librecad/plugins"
rmdir "$PKG/usr/bin/resources"
install -d "$PKG/usr/share/librecad"
cp -r librecad/support/fonts librecad/support/patterns librecad/support/library \
	"$PKG/usr/share/librecad/"
chmod -R u=rwX,go=rX "$PKG/usr/share/librecad"

install -Dm644 desktop/librecad.1 "$PKG/usr/share/man/man1/librecad.1"
install -Dm644 librecad/res/main/librecad.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/librecad.png"
install -Dm644 desktop/org.librecad.librecad.appdata.xml \
	"$PKG/usr/share/metainfo/org.librecad.librecad.appdata.xml"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/librecad.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LibreCAD
GenericName=2D CAD
Comment=Draft 2D drawings and plans in DXF
Exec=librecad %F
Icon=librecad
Terminal=false
StartupWMClass=librecad
MimeType=image/vnd.dxf;
Categories=Graphics;VectorGraphics;Engineering;Qt;
Keywords=cad;2d;drafting;drawing;dxf;dwg;plan;librecad;
DESKTOP
chmod 644 "$PKG/usr/share/applications/librecad.desktop"
