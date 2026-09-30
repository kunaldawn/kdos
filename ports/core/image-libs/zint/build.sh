# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# libQZint is shared, so the GUI and any other Qt consumer link one copy.
patch -p1 -i "$PORT_SRC/shared-libqzint.patch"

# The GS1 Syntax Engine is libgs1encoders, which is not a port; zint then
# checks GS1 data with its own rules. The tcl backend is not built.
cmake -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DZINT_SHARED=ON \
	-DZINT_STATIC=OFF \
	-DZINT_FRONTEND=ON \
	-DZINT_USE_PNG=ON \
	-DZINT_USE_QT=ON \
	-DZINT_QT6=ON \
	-DZINT_USE_GS1SE=OFF \
	-DZINT_TEST=OFF \
	-DZINT_UNINSTALL=OFF
cmake --build build
DESTDIR=$PKG cmake --install build

# No desktop file name is set, so Qt's Wayland app_id is the organisation
# domain the main window sets, reversed, before the program name:
# uk.org.zint.zint-qt.
install -Dm644 zint-qt.png "$PKG/usr/share/icons/hicolor/48x48/apps/zint-qt.png"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/zint-qt.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Zint Barcode Studio
GenericName=Barcode Generator
Comment=Make barcodes and QR codes for labels and print
Exec=zint-qt
Icon=zint-qt
Terminal=false
StartupWMClass=uk.org.zint.zint-qt
Categories=Graphics;2DGraphics;Office;
Keywords=barcode;qr;ean;upc;code128;datamatrix;label;zint;
EOF2
chmod 644 "$PKG/usr/share/applications/zint-qt.desktop"
