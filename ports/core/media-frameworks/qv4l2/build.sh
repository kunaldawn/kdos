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

export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE"
meson setup build \
	--prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dudevdir=/lib/udev \
	-Dbpf=disabled \
	-Dgconv=disabled \
	-Djpeg=enabled \
	-Dlibdvbv5=disabled \
	-Dqv4l2=enabled \
	-Dqvidcap=enabled \
	-Dv4l2-tracer=disabled \
	-Dv4l-plugins=true \
	-Dv4l-utils=true \
	-Dv4l-wrappers=true \
	-Dv4l2-compliance-libv4l=true \
	-Dv4l2-ctl-libv4l=true \
	-Dv4l2-ctl-stream-to=true \
	-Ddoxygen-doc=disabled
meson compile -C build
DESTDIR=$PWD/stage meson install --no-rebuild -C build

for f in usr/bin/qv4l2 usr/bin/qvidcap \
	usr/share/man/man1/qv4l2.1 usr/share/man/man1/qvidcap.1 \
	usr/share/applications/qv4l2.desktop usr/share/applications/qvidcap.desktop; do
	install -D "stage/$f" "$PKG/$f"
done
for d in stage/usr/share/icons/hicolor/*/apps; do
	s=${d#stage/}
	install -d "$PKG/$s"
	cp -a "$d"/qv4l2.* "$d"/qvidcap.* "$PKG/$s/"
done

test -x "$PKG/usr/bin/qv4l2"
test -x "$PKG/usr/bin/qvidcap"
readelf -d "$PKG/usr/bin/qv4l2" | grep -q 'NEEDED.*\[libQt6Widgets\.so\.6\]'
