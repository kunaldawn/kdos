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

# dynmanifest is the LV2 extension no current plugin ships and that loads
# plugin code merely to list it; it stays off, as upstream defaults it.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Ddocs=disabled -Dtests=disabled \
	-Dtools=enabled -Dbindings_cpp=enabled -Dbindings_py=enabled \
	-Ddynmanifest=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The completion is installed under /etc/bash_completion.d, which bash reads
# only through a compatibility path; the lazy-loading directory is the real one.
install -Dm644 "$PKG/etc/bash_completion.d/lilv" \
	"$PKG/usr/share/bash-completion/completions/lilv"
rm "$PKG/etc/bash_completion.d/lilv"
rmdir "$PKG/etc/bash_completion.d" "$PKG/etc"
