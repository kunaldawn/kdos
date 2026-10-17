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

patch -p1 -i "$PORT_SRC/fix-null-usage.patch"
export CFLAGS="${CFLAGS/-std=gnu11/}"

_mk=(
	CC="gcc -std=gnu11"
	PREFIX=/usr
	LIBDIR=/usr/lib
	PLUGINDIR=/usr/lib/mupen64plus
	SHAREDIR=/usr/share/mupen64plus
	SDL_CFLAGS="$(pkg-config --cflags sdl2)"
	SDL_LDLIBS="$(pkg-config --libs sdl2)"
	GL_CFLAGS=
	MINIZIP_CFLAGS="-I$(pkg-config --variable=includedir minizip)/minizip"
	MINIZIP_LDLIBS="$(pkg-config --libs minizip)"
	OSD=1
	VULKAN=1
	NETPLAY=0
	HIRES=1
	PIE=1
	V=1
)
_gl() { [ "$1" = video-glide64mk2 ] && echo -lGL || echo -lOpenGL; }
for _c in core ui-console audio-sdl input-sdl rsp-hle video-rice video-glide64mk2; do
	make -C "source/mupen64plus-$_c/projects/unix" all "${_mk[@]}" \
		GL_LDLIBS="$(_gl $_c)"
done
for _c in core ui-console audio-sdl input-sdl rsp-hle video-rice video-glide64mk2; do
	make -C "source/mupen64plus-$_c/projects/unix" install "${_mk[@]}" \
		GL_LDLIBS="$(_gl $_c)" LDCONFIG=true DESTDIR="$PKG"
done

cat > "$PKG/usr/share/applications/mupen64plus.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Mupen64Plus
GenericName=Nintendo 64 Emulator
Comment=Play a Nintendo 64 ROM image
TryExec=mupen64plus
Exec=mupen64plus %f
Icon=mupen64plus
Terminal=false
NoDisplay=true
StartupWMClass=mupen64plus
MimeType=application/x-n64-rom;
Categories=Game;Emulator;
Keywords=emulator;nintendo;n64;mupen64plus;
DESKTOP
chmod 644 "$PKG/usr/share/applications/mupen64plus.desktop"
