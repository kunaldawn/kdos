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

for p in poppler-26.05 poppler-26.06 poppler-26.07 poppler-version-check poppler-26.08; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_SHARED_LIBS=ON \
	-D BUILD_TESTING=OFF \
	-D WITH_INTERNAL_2GEOM=OFF \
	-D ENABLE_POPPLER=ON \
	-D ENABLE_POPPLER_CAIRO=ON \
	-D ENABLE_LCMS=ON \
	-D WITH_IMAGE_MAGICK=OFF \
	-D WITH_GRAPHICS_MAGICK=ON \
	-D WITH_LIBCDR=ON \
	-D WITH_LIBVISIO=ON \
	-D WITH_LIBWPG=ON \
	-D WITH_GSPELL=ON \
	-D WITH_GSOURCEVIEW=ON \
	-D WITH_GNU_READLINE=ON \
	-D WITH_OPENMP=ON \
	-D WITH_SVG2=ON \
	-D WITH_X11=ON \
	-D WITH_NLS=OFF \
	-D WITH_JEMALLOC=OFF \
	-D WITH_MANPAGE_COMPRESSION=OFF \
	-D ENABLE_BINRELOC=OFF \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/inkscape/extensions/inkman"
find "$PKG/usr/share/man" -mindepth 1 -maxdepth 1 ! -name 'man[0-9]*' -exec rm -rf {} +
find "$PKG/usr/share/inkscape/tutorials" -name '*.*.*' -exec rm -f {} +

cat > "$PKG/usr/share/applications/org.inkscape.Inkscape.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Inkscape
GenericName=Vector Graphics Editor
Comment=Create and edit Scalable Vector Graphics images
TryExec=inkscape
Exec=inkscape %F
Icon=org.inkscape.Inkscape
Terminal=false
StartupNotify=true
StartupWMClass=org.inkscape.Inkscape
MimeType=image/svg+xml;image/svg+xml-compressed;application/vnd.corel-draw;application/pdf;application/postscript;image/x-eps;application/illustrator;image/x-wmf;image/x-emf;application/x-xccx;application/x-xcdt;application/x-xcmx;image/x-xcdr;application/visio;application/x-visio;application/vnd.visio;application/vnd.ms-visio.viewer;application/visio.drawing;application/vsd;application/x-vsd;image/x-vsd;
Categories=Graphics;VectorGraphics;GTK;
Keywords=image;editor;vector;drawing;svg;inkscape;
Actions=new-window;

[Desktop Action new-window]
Name=Open a New Window
Exec=inkscape
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.inkscape.Inkscape.desktop"
