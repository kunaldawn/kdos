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

# Lua is LuaJIT 2.1 (configure's default pair, luajit and 5.1), and SDL is
# found through pkgconf. GME is off upstream by default and named on here;
# every other codec library is required by configure, so none can drop out.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-lua=luajit \
	--with-luaversion=5.1 \
	--enable-mpg123 \
	--enable-gme
make
make DESTDIR=$PKG install

# The panel reads PNG only: the program's icon and the .love document icon
# are rasterised from upstream's SVGs.
rm -f "$PKG/usr/share/pixmaps/love.svg"
rsvg-convert -w 256 -h 256 platform/unix/love.svg \
	-o love.png
install -Dm644 love.png "$PKG/usr/share/icons/hicolor/256x256/apps/love.png"
rsvg-convert -w 64 -h 64 platform/unix/application-x-love-game.svg \
	-o love-game.png
install -Dm644 love-game.png \
	"$PKG/usr/share/icons/hicolor/64x64/mimetypes/application-x-love-game.png"

# Upstream's entry is a hidden handler for .love files, and so is this one:
# LÖVE started with no game shows only its placeholder screen. It is
# rewritten for StartupWMClass, the Wayland app_id being SDL's default, the
# executable's name.
rm -f "$PKG/usr/share/applications/love.desktop"
cat > "$PKG/usr/share/applications/love.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=LÖVE
Comment=Run a game made with the LÖVE framework
Exec=love %f
Icon=love
Terminal=false
NoDisplay=true
StartupWMClass=love
MimeType=application/x-love-game;
Categories=Game;
DESKTOP
chmod 644 "$PKG/usr/share/applications/love.desktop"
