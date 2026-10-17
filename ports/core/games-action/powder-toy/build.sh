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
