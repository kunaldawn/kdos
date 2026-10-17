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

patch -p1 -i "$PORT_SRC/no-update-check.patch"
patch -p1 -i "$PORT_SRC/imagemagick-flags.patch"

export CXXFLAGS="$CXXFLAGS $(pkg-config --cflags-only-other Magick++)"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D USE_QT6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/converseen/loc"

cat > "$PKG/usr/share/applications/net.fasterland.converseen.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Converseen
GenericName=Batch Image Converter
Comment=Convert, resize, rotate and rename many images at once
Exec=converseen %F
Icon=converseen
Terminal=false
StartupWMClass=net.fasterland.converseen
Categories=Qt;Graphics;2DGraphics;RasterGraphics;
Keywords=image;convert;resize;batch;rename;rotate;
EOF
chmod 644 "$PKG/usr/share/applications/net.fasterland.converseen.desktop"
