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

# musl has no *64 file calls and no res_n* resolver: the first two patches
# point Qt at the plain names. The third pairs a prepared Wayland read with a
# cancel on shutdown, or a second reader thread waits forever for this one.
patch -p1 -i "$PORT_SRC/musl-lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-dns-resolver.patch"
patch -p1 -i "$PORT_SRC/wayland-cancel-read.patch"

# WAYLAND AND XCB ARE BOTH BUILT, AND WAYLAND IS FIRST. QT_QPA_PLATFORMS makes
# wayland the default platform plugin, and Qt prepends it whenever
# WAYLAND_DISPLAY is set, so xcb is reached only by a program that asks for it
# (QT_QPA_PLATFORM=xcb) and then runs under Xwayland. xcb needs
# xkbcommon-x11 and xcb_glx_plugin needs GLX from libglvnd and mesa; with
# either missing, the FEATURE_ line below stops configure instead of dropping
# the backend.
#
# EGLFS, LINUXFB AND VNC ARE OFF. A Qt program started outside kdos-comp would
# otherwise take the card and the input devices from the compositor already
# drawing on them. Without them it says it could not find a platform and exits.
#
# THE TOOLS LIVE IN /usr/lib/qt6/bin and INSTALL_PUBLICBINDIR links each
# user-facing one into /usr/bin with a 6 suffix (qmake6, ...), so a Qt 5 qmake
# can sit beside it. Every later Qt module reads these paths from qtbase's
# installed build internals, so changing one here moves all of them.
#
# accessibility_atspi_bridge is named so a missing atspi-2 stops configure
# instead of leaving every Qt program invisible to a screen reader. journald is off (no systemd);
# libproxy and the gtk3 platform theme are off because the session themes Qt
# through QT_QPA_PLATFORMTHEME=kde, not through GTK. The bundled md4c and
# BLAKE2 are used because neither is a port.
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DINSTALL_BINDIR=lib/qt6/bin \
	-DINSTALL_PUBLICBINDIR=bin \
	-DINSTALL_LIBEXECDIR=lib/qt6/libexec \
	-DINSTALL_ARCHDATADIR=lib/qt6 \
	-DINSTALL_DATADIR=share/qt6 \
	-DINSTALL_INCLUDEDIR=include/qt6 \
	-DINSTALL_MKSPECSDIR=lib/qt6/mkspecs \
	-DINSTALL_DOCDIR=share/doc/qt6 \
	-DINSTALL_EXAMPLESDIR=share/doc/qt6/examples \
	-DINSTALL_SYSCONFDIR=/etc/xdg \
	-DQT_BUILD_EXAMPLES=OFF \
	-DQT_BUILD_TESTS=OFF \
	-DQT_QPA_PLATFORMS="wayland;xcb" \
	-DINPUT_opengl=desktop \
	-DINPUT_publicsuffix=qt \
	-DFEATURE_relocatable=OFF \
	-DFEATURE_reduce_relocations=OFF \
	-DFEATURE_wayland=ON \
	-DFEATURE_xcb=ON \
	-DFEATURE_xlib=ON \
	-DFEATURE_xcb_xlib=ON \
	-DFEATURE_xkbcommon=ON \
	-DFEATURE_xkbcommon_x11=ON \
	-DFEATURE_system_xcb_xinput=ON \
	-DFEATURE_xcb_glx_plugin=ON \
	-DFEATURE_xcb_egl_plugin=ON \
	-DFEATURE_xcb_sm=OFF \
	-DFEATURE_opengl=ON \
	-DFEATURE_opengl_desktop=ON \
	-DFEATURE_egl=ON \
	-DFEATURE_egl_x11=ON \
	-DFEATURE_gbm=ON \
	-DFEATURE_kms=ON \
	-DFEATURE_vulkan=ON \
	-DFEATURE_vkkhrdisplay=OFF \
	-DFEATURE_eglfs=OFF \
	-DFEATURE_linuxfb=OFF \
	-DFEATURE_vnc=OFF \
	-DFEATURE_directfb=OFF \
	-DFEATURE_tslib=OFF \
	-DFEATURE_libinput=ON \
	-DFEATURE_mtdev=ON \
	-DFEATURE_evdev=ON \
	-DFEATURE_libudev=ON \
	-DFEATURE_accessibility=ON \
	-DFEATURE_accessibility_atspi_bridge=ON \
	-DFEATURE_dbus=ON \
	-DFEATURE_dbus_linked=ON \
	-DFEATURE_glib=ON \
	-DFEATURE_icu=ON \
	-DFEATURE_fontconfig=ON \
	-DFEATURE_system_freetype=ON \
	-DFEATURE_system_harfbuzz=ON \
	-DFEATURE_system_png=ON \
	-DFEATURE_system_jpeg=ON \
	-DFEATURE_system_zlib=ON \
	-DFEATURE_system_pcre2=ON \
	-DFEATURE_system_doubleconversion=ON \
	-DFEATURE_system_sqlite=ON \
	-DFEATURE_system_libb2=OFF \
	-DFEATURE_system_textmarkdownreader=OFF \
	-DFEATURE_zstd=ON \
	-DFEATURE_brotli=ON \
	-DFEATURE_liburing=ON \
	-DFEATURE_openssl=ON \
	-DFEATURE_openssl_linked=ON \
	-DFEATURE_gssapi=ON \
	-DFEATURE_libproxy=OFF \
	-DFEATURE_sctp=OFF \
	-DFEATURE_journald=OFF \
	-DFEATURE_jemalloc=OFF \
	-DFEATURE_lttng=OFF \
	-DFEATURE_gtk3=OFF \
	-DFEATURE_cups=ON \
	-DFEATURE_sql_sqlite=ON \
	-DFEATURE_sql_psql=OFF \
	-DFEATURE_sql_mysql=OFF \
	-DFEATURE_sql_odbc=OFF \
	-DFEATURE_sql_ibase=OFF \
	-DFEATURE_sql_oci=OFF \
	-DFEATURE_sql_db2=OFF \
	-DFEATURE_sql_mimer=OFF
ninja
DESTDIR=$PKG ninja install
