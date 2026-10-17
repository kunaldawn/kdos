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

patch -p1 -i "$PORT_SRC/ffmpeg-supported-config.patch"
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpipewire=enabled \
	-Dpulse=disabled \
	-Ddefault_audio_backend=pipewire
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -d "$PKG/usr/share/fish/vendor_completions.d"
mv "$PKG/usr/share/fish/fish/vendor_completions.d/wf-recorder.fish" \
	"$PKG/usr/share/fish/vendor_completions.d/"
rm -r "$PKG/usr/share/fish/fish"
