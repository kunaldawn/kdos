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

cp -a "$SRC_ROOT/recoil-$_recoil" 3rdparty/
cp -a "$SRC_ROOT/6502" 3rdparty/
(cd 3rdparty/6502 && patch -p1 -i ../6502-illegal-opcode.patch)

install -d "$SRC_ROOT/shim"
printf '#!/bin/sh\nexec pkg-config sdl2 "$@"\n' > "$SRC_ROOT/shim/sdl2-config"
chmod 755 "$SRC_ROOT/shim/sdl2-config"
export PATH="$SRC_ROOT/shim:$PATH"

export CFLAGS="$CFLAGS -g0"

_opts="API=sdl2 NO_X11=1 LUAPKG=lua5.4 PREFIX=/usr"
make -C src $_opts
make -C src $_opts DESTDIR=$PKG install

mv "$PKG/usr/bin/grafx2-sdl2" "$PKG/usr/bin/grafx2"

for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s share/icons/grafx2.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/grafx2.png"
done

cat > "$PKG/usr/share/applications/grafx2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=GrafX2
GenericName=Pixel Art Editor
Comment=Draw pixel art and edit indexed-colour pictures
TryExec=grafx2
Exec=grafx2 %f
Icon=grafx2
Terminal=false
StartupWMClass=grafx2
Categories=Graphics;2DGraphics;RasterGraphics;
Keywords=pixel;art;paint;sprite;palette;deluxe;grafx2;
DESKTOP
chmod 644 "$PKG/usr/share/applications/grafx2.desktop"
