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

patch -p1 -i "$PORT_SRC/musl-lfs64.patch"
patch -p1 -i "$PORT_SRC/musl-dns-resolver.patch"
patch -p1 -i "$PORT_SRC/wayland-cancel-read.patch"
patch -p1 -i "$PORT_SRC/no-source-tree.patch"

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
	-DQT_DISABLE_RPATH=ON \
	-DQT_QPA_PLATFORMS="wayland;xcb" \
	-DINPUT_opengl=desktop \
	-DINPUT_publicsuffix=qt \
	-DFEATURE_relocatable=OFF \
	-DFEATURE_reduce_relocations=OFF \
	-DFEATURE_rpath=OFF \
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
	-DFEATURE_sql_psql=ON \
	-DFEATURE_sql_mysql=OFF \
	-DFEATURE_sql_odbc=OFF \
	-DFEATURE_sql_ibase=OFF \
	-DFEATURE_sql_oci=OFF \
	-DFEATURE_sql_db2=OFF \
	-DFEATURE_sql_mimer=OFF
ninja
DESTDIR=$PKG ninja install
test "$(grep -rl /var/cache/kpkg "$PKG/usr/lib/cmake" | wc -l)" = 0
