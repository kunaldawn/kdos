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

patch -p1 -i "$PORT_SRC/musl-execinfo.patch"

# NETWORKING off removes the add-on browser and its downloads, which have
# nothing to show offline. With English the only language shipped, the system
# SDL2_ttf replaces the bundled copy, which differs only in right-to-left
# shaping through raqm. The scripting library is linked in rather than
# installed as a private shared object. GL comes through GLEW, and a missing
# OpenGL stops the configure instead of leaving only the SDL renderer.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DINSTALL_SUBDIR_BIN=bin \
	-DIS_SUPERTUX_RELEASE=ON \
	-DENABLE_OPENGL=ON \
	-DUSE_GL_LIBRARY=glew \
	-DENABLE_NETWORKING=OFF \
	-DENABLE_DISCORD=OFF \
	-DSTEAM_BUILD=OFF \
	-DFLATPAK=OFF \
	-DUSE_SYSTEM_SDL2_TTF=ON \
	-DUSE_STATIC_SIMPLESQUIRREL=ON \
	-DSUPERTUX_PCH=OFF \
	-DSUPERTUX_LTO=OFF \
	-DBUILD_TESTING=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON
cmake --build build
DESTDIR=$PKG cmake --install build

# English only: the translations of the menus and of every level set go.
find "$PKG/usr/share/games/supertux2" -name '*.po' -delete

# The panel reads no SVG: the menu icon is a PNG rasterised from upstream's.
# Upstream's entry names the icon supertux2 and the window's app_id is the
# program's name, supertux2.
install -d "$PKG/usr/share/icons/hicolor/256x256/apps"
rsvg-convert -w 256 -h 256 supertux2.svg \
	-o "$PKG/usr/share/icons/hicolor/256x256/apps/supertux2.png"
cat > "$PKG/usr/share/applications/supertux2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SuperTux
GenericName=Platform Game
Comment=Play a classic 2D platform game
Exec=supertux2
Icon=supertux2
Terminal=false
StartupWMClass=supertux2
Categories=Game;ArcadeGame;
Keywords=game;arcade;platform;tux;jump;
DESKTOP
chmod 644 "$PKG/usr/share/applications/supertux2.desktop"
