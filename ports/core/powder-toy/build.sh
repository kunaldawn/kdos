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

# HTTP IS OFF: the save browser, sign-in, votes and the update check all
# speak to powdertoy.co.uk, and on a machine with no network each is a
# dialog that fails. Saves are opened and written as local files.
# can_install=no keeps the game from writing its own desktop entry and MIME
# type into the home directory at start-up; this port installs both.
# Lua is LuaJIT, named rather than left to `auto`, which picks by platform.
# bzip2 is found through its pkg-config file; the default `simple`
# workaround would look for it by compiler search instead.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dstatic=none \
	-Dhttp=false \
	-Dignore_updates=true \
	-Dcan_install=no \
	-Dlua=luajit \
	-Dworkaround_elusive_bzip2=none \
	-Duse_bluescreen=no \
	-Dresolve_vcs_tag=no \
	-Drender_icons_with_inkscape=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -Dm644 resources/powder.man "$PKG/usr/share/man/man6/powder.6"
install -Dm644 resources/generated_icons/icon_exe.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/powder-toy.png"
install -Dm644 resources/generated_icons/icon_exe_48.png \
	"$PKG/usr/share/icons/hicolor/48x48/apps/powder-toy.png"
install -Dm644 build/resources/save.xml \
	"$PKG/usr/share/mime/packages/powder-toy.xml"

# The Wayland app_id is SDL's default, the executable's name: powder. The
# ptsave: scheme upstream's entry claims opens the online browser, which
# this build does not have.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/powder-toy.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=The Powder Toy
GenericName=Physics Sandbox
Comment=Falling-sand sandbox of pressure, heat, gravity and electronics
Exec=powder %f
Icon=powder-toy
Terminal=false
StartupWMClass=powder
MimeType=application/vnd.powdertoy.save;
Categories=Game;Simulation;
Keywords=sandbox;physics;falling sand;simulation;tpt;powder;
DESKTOP
chmod 644 "$PKG/usr/share/applications/powder-toy.desktop"
