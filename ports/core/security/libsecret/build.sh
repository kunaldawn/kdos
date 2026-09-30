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

# The client library and secret-tool. There is no Secret Service in this
# package: a caller finds whichever program owns org.freedesktop.secrets on the
# session bus, and with none every lookup fails and the program re-prompts.
# secret-file-collection.c calls g_open(), which is open(), with O_CREAT and
# O_RDWR, and includes only <sys/file.h> for them: glibc's pulls in <fcntl.h>,
# musl's does not, and an undeclared function is an error.
export CFLAGS="$CFLAGS -include fcntl.h"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dcrypto=libgcrypt \
	-Dintrospection=true \
	-Dvapi=true \
	-Dmanpage=true \
	-Dgtk_doc=false \
	-Dbash_completion=enabled \
	-Dtpm2=false \
	-Dpam=false \
	-Dtest_setup=disabled \
	-Ddebugging=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
