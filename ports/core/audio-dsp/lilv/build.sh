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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Ddocs=disabled -Dtests=disabled \
	-Dtools=enabled -Dbindings_cpp=enabled -Dbindings_py=enabled \
	-Ddynmanifest=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -Dm644 "$PKG/etc/bash_completion.d/lilv" \
	"$PKG/usr/share/bash-completion/completions/lilv"
rm "$PKG/etc/bash_completion.d/lilv"
rmdir "$PKG/etc/bash_completion.d" "$PKG/etc"
