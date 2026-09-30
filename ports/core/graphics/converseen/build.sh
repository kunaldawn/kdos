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

# Magick++'s headers size every Quantum from MAGICKCORE_HDRI_ENABLE and
# MAGICKCORE_QUANTUM_DEPTH. Taken from the installed Magick++.pc they match the
# library; a mismatch compiles and then reads every pixel as the wrong type.
export CXXFLAGS="$CXXFLAGS $(pkg-config --cflags-only-other Magick++)"

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D USE_QT6=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

# Bundled data is English only, and English is the untranslated source text.
rm -rf "$PKG/usr/share/converseen/loc"

# setDesktopFileName makes the Wayland app_id net.fasterland.converseen, and
# the entry has to name it or the panel cannot tie the window to its launcher.
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
