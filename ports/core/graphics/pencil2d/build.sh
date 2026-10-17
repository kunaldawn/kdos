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

patch -p1 -i "$PORT_SRC/no-palette-mime-glob.patch"

mkdir -p build && cd build
qmake6 .. \
	PREFIX=/usr \
	CONFIG+=release \
	CONFIG+=NO_TESTS \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS"
make
make INSTALL_ROOT="$PKG" install

cat > "$PKG/usr/share/applications/org.pencil2d.Pencil2D.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Pencil2D
GenericName=2D Animation
Comment=Hand-drawn animation with bitmap and vector layers
Exec=pencil2d %f
Icon=org.pencil2d.Pencil2D
Terminal=false
StartupWMClass=org.pencil2d.Pencil2D
MimeType=application/x-pencil2d-pcl;application/x-pencil2d-pclx;
Categories=Qt;Graphics;2DGraphics;VectorGraphics;RasterGraphics;
Keywords=animation;cartoon;drawing;frame;onion;pencil2d;
EOF
chmod 644 "$PKG/usr/share/applications/org.pencil2d.Pencil2D.desktop"
