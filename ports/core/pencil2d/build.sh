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

# The palette type's *.xml glob would compete with application/xml for every
# XML file on the machine.
patch -p1 -i "$PORT_SRC/no-palette-mime-glob.patch"

# A qmake project. Qt 6's qmake reads no compiler flags from the environment,
# so the tree's flags, and with them the reproducibility maps, are passed in.
# NO_TESTS drops the test subproject. Movie export and import run the ffmpeg
# program found on PATH at the time; without it those menu entries fail.
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

# QGuiApplication::setDesktopFileName makes the Wayland app_id
# org.pencil2d.Pencil2D; the entry names it so the panel ties the window to
# its launcher. The MIME types are Pencil2D's own, from the XML installed above.
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
