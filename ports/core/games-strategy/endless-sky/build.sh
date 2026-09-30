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

# Dependencies come from the system, not vcpkg, which would fetch them. The
# game looks for its data under share/games/endless-sky of the prefix its
# executable is in, so the binary moves from upstream's /usr/games to
# /usr/bin without the data moving.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DES_USE_VCPKG=OFF \
	-DES_USE_SYSTEM_LIBRARIES=ON \
	-DES_GLES=OFF \
	-DES_STEAM=OFF \
	-DBUILD_TESTING=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
install -d "$PKG/usr/bin"
mv "$PKG/usr/games/endless-sky" "$PKG/usr/bin/endless-sky"
rmdir "$PKG/usr/games"

# The game names its window class at run time through SDL2's WMCLASS
# variable, which SDL3 underneath does not read; the app_id is then the
# program's name.
cat > "$PKG/usr/share/applications/io.github.endless_sky.endless_sky.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Endless Sky
GenericName=Space Trading Game
Comment=Space exploration, trading and combat
Exec=endless-sky
Icon=endless-sky
Terminal=false
StartupWMClass=endless-sky
Categories=Game;Simulation;
Keywords=game;simulator;space;sandbox;rpg;trading;
DESKTOP
chmod 644 "$PKG/usr/share/applications/io.github.endless_sky.endless_sky.desktop"
