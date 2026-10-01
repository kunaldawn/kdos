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

# Print backends and the GStreamer media backend are GIO modules found by
# scanning their directories, so they need no cache. The X11 backend serves
# applications that force GDK_BACKEND=x11 under Xwayland; with GDK_BACKEND
# unset GDK tries Wayland first.
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Db_ndebug=if-release \
	-Dwayland-backend=true \
	-Dx11-backend=true \
	-Dbroadway-backend=false \
	-Dmedia-gstreamer=enabled \
	-Dprint-cups=enabled \
	-Dprint-cpdb=disabled \
	-Dvulkan=enabled \
	-Dcloudproviders=disabled \
	-Dsysprof=disabled \
	-Dtracker=disabled \
	-Dcolord=disabled \
	-Daccesskit=disabled \
	-Dintrospection=enabled \
	-Ddocumentation=false \
	-Dscreenshots=false \
	-Dman-pages=true \
	-Dbuild-demos=false \
	-Dbuild-testsuite=false \
	-Dbuild-examples=false \
	-Dbuild-tests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
