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

# version-tag stops the build asking git for a version; the source is not a
# checkout. systemd=disabled drops the monitor units and the dax udev rule,
# which only asks systemd to start a unit. asciidoctor renders the manual
# pages.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	-Dversion-tag=$version \
	-Dsystemd=disabled \
	-Ddocs=enabled \
	-Dasciidoctor=enabled \
	-Dlibtracefs=enabled \
	-Dkeyutils=enabled \
	-Dtest=disabled \
	-Dbashcompletiondir=/usr/share/bash-completion/completions
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
