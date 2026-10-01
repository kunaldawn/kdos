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

# RECOIL (the retro-format loader) and the 6502 emulator are compiled into
# the program from 3rdparty/. src/Makefile only calls 3rdparty/Makefile, which
# downloads them and asks GitHub for the latest SDL releases, when their
# sources are missing, so both are laid out there first from the two later
# sources, the 6502 core with 3rdparty's own patch applied.
cp -a "$SRC_ROOT/recoil-$_recoil" 3rdparty/
cp -a "$SRC_ROOT/6502" 3rdparty/
(cd 3rdparty/6502 && patch -p1 -i ../6502-illegal-opcode.patch)

# sdl2-compat installs no sdl2-config, and src/Makefile runs that name for the
# SDL2 flags; a shim answering from pkg-config stands in for it.
install -d "$SRC_ROOT/shim"
printf '#!/bin/sh\nexec pkg-config sdl2 "$@"\n' > "$SRC_ROOT/shim/sdl2-config"
chmod 755 "$SRC_ROOT/shim/sdl2-config"
export PATH="$SRC_ROOT/shim:$PATH"

export CFLAGS="$CFLAGS -g0"

# API=sdl2: the default is SDL 1.2. NO_X11=1 drops the X11 clipboard and
# window calls, which SDL3 underneath could never reach with no X11 driver.
# LUAPKG names Lua 5.4. The makefile's own search tries 5.3 and older, then
# plain `lua`, which is the 5.5 port: a language version the scripts were
# never run against.
_opts="API=sdl2 NO_X11=1 LUAPKG=lua5.4 PREFIX=/usr"
make -C src $_opts
make -C src $_opts DESTDIR=$PKG install

# The binary is named after its API; the window's app_id is the program's
# own name, so it is installed as plain grafx2. Data is found at
# ../share/grafx2 from the binary, which the rename keeps.
mv "$PKG/usr/bin/grafx2-sdl2" "$PKG/usr/bin/grafx2"

# THE MENU ICON: upstream ships SVG and XPM, and the panel reads neither.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s share/icons/grafx2.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/grafx2.png"
done

# UPSTREAM'S ENTRY IS REPLACED. MimeType is left out: it claims PNG, GIF and
# BMP, which belong to the image viewer; mimeapps.list chooses defaults.
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
