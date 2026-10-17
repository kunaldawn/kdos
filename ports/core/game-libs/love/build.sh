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

./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--with-lua=luajit \
	--with-luaversion=5.1 \
	--enable-mpg123 \
	--enable-gme
make
make DESTDIR=$PKG install

rm -f "$PKG/usr/share/pixmaps/love.svg"
rsvg-convert -w 256 -h 256 platform/unix/love.svg \
	-o love.png
install -Dm644 love.png "$PKG/usr/share/icons/hicolor/256x256/apps/love.png"
rsvg-convert -w 64 -h 64 platform/unix/application-x-love-game.svg \
	-o love-game.png
install -Dm644 love-game.png \
	"$PKG/usr/share/icons/hicolor/64x64/mimetypes/application-x-love-game.png"

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
