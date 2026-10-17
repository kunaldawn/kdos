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

cp -a "$SRC_ROOT/imgui-$_imgui/." thirdparty/imgui/
cp -a "$SRC_ROOT/miniaudio-$_miniaudio/." thirdparty/miniaudio/

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D ENABLE_QT6=ON \
	-D ENABLE_SDL=ON \
	-D ENABLE_CELX=ON \
	-D ENABLE_FFMPEG=ON \
	-D ENABLE_LIBAVIF=ON \
	-D ENABLE_MINIAUDIO=ON \
	-D ENABLE_NLS=OFF \
	-D ENABLE_SPICE=ON \
	-D ENABLE_TOOLS=OFF \
	-D ENABLE_TESTS=OFF \
	-D ENABLE_GLES=OFF \
	-D CMAKE_DISABLE_FIND_PACKAGE_LuaJIT=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

rm -rf "$PKG/usr/share/celestia/locale"
ln -sf /usr/share/fonts/TTF/DejaVuSans.ttf "$PKG/usr/share/celestia/fonts/DejaVuSans.ttf"
ln -sf /usr/share/fonts/TTF/DejaVuSans-Bold.ttf "$PKG/usr/share/celestia/fonts/DejaVuSans-Bold.ttf"

install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
magick celestia-logo.png -resize 128x128 \
	"$PKG/usr/share/icons/hicolor/128x128/apps/celestia.png"

install -Dm644 /dev/stdin "$PKG/usr/share/mime/packages/kdos-celestia.xml" <<'MIMEXML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-celestia-script">
    <comment>Celestia script</comment>
    <sub-class-of type="text/plain"/>
    <glob pattern="*.cel"/>
    <glob pattern="*.celx"/>
  </mime-type>
</mime-info>
MIMEXML
rm -f "$PKG"/usr/share/applications/space.celestiaproject.celestia_*.desktop
cat > "$PKG/usr/share/applications/space.celestiaproject.celestia_qt6.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Celestia
GenericName=Space Simulator
Comment=Explore the universe in a detailed 3D space simulation
TryExec=celestia-qt6
Exec=celestia-qt6 %f
Icon=celestia
Terminal=false
StartupWMClass=celestia-qt6
MimeType=application/x-celestia-script;
Categories=Qt;Education;Science;Astronomy;
Keywords=astronomy;space;planets;stars;galaxy;simulator;celestia;
DESKTOP
cat > "$PKG/usr/share/applications/space.celestiaproject.celestia_sdl.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Celestia (SDL)
GenericName=Space Simulator
Comment=Explore the universe, with the light SDL interface
TryExec=celestia-sdl
Exec=celestia-sdl
Icon=celestia
Terminal=false
StartupWMClass=celestia-sdl
Categories=Education;Science;Astronomy;
Keywords=astronomy;space;planets;stars;galaxy;simulator;celestia;
DESKTOP
chmod 644 "$PKG"/usr/share/applications/space.celestiaproject.celestia_*.desktop
