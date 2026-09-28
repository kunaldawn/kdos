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

# Dear ImGui (the SDL front end's interface) and miniaudio (sound in scripts)
# are git submodules the archive carries empty; each is a later source,
# placed where the build looks for it.
cp -a "$SRC_ROOT/imgui-$_imgui/." thirdparty/imgui/
cp -a "$SRC_ROOT/miniaudio-$_miniaudio/." thirdparty/miniaudio/

# Two front ends: Qt 6, the full interface, and SDL, the light one for old
# hardware and software rendering. FFmpeg gives movie capture and video
# overlays, libavif AVIF textures. Lua is the tree's lua, not LuaJIT, whatever
# the build root holds. ENABLE_NLS=OFF: bundled data is English only. SPICE
# orbits and rotations, which spacecraft add-ons use, link the cspice port.
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

# The per-language key references and tours are left out with the
# translations. The bundled DejaVu faces are the files ttf-dejavu installs, so
# the data directory links to those.
rm -rf "$PKG/usr/share/celestia/locale"
ln -sf /usr/share/fonts/TTF/DejaVuSans.ttf "$PKG/usr/share/celestia/fonts/DejaVuSans.ttf"
ln -sf /usr/share/fonts/TTF/DejaVuSans-Bold.ttf "$PKG/usr/share/celestia/fonts/DejaVuSans-Bold.ttf"

# The logo is a 150-pixel PNG under pixmaps; hicolor takes it at 128.
install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
magick celestia-logo.png -resize 128x128 \
	"$PKG/usr/share/icons/hicolor/128x128/apps/celestia.png"

# Celestia scripts are not in the shared MIME database, so the port defines
# the type; the Qt entry is the one that claims it. The entries are replaced
# for StartupWMClass: the Qt front end sets its desktop file name, and so its
# Wayland app_id, to celestia-qt6; SDL takes the program name, celestia-sdl.
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
