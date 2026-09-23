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

# nettle4.patch is upstream's three commits that move the crypto layer to the
# nettle 4 API (digest functions take no length; sha.h is sha2.h) and require
# nettle/hogweed >= 4. 1.0.1 as released compiles only against nettle 3, and
# the third commit is load-bearing on its own: without it the WebSocket
# handshake passes the digest size where the hash type goes and crashes on
# every connection.
patch -p1 -i $PORT_SRC/nettle4.patch

# The build tries subproject('aml') before dependency('aml1'), and the release
# tarball ships no subprojects directory. --wrap-mode=nodownload makes a wrap
# that appeared anyway an error instead of a fetch: a build that reaches the
# network is not reproducible and cannot run in the chroot.
#
# tls and nettle are both 'auto' features, and an auto feature that misses its
# library disables itself and still succeeds. Spelling them 'enabled' is what
# makes the failure loud: without gnutls there is no VeNCrypt, and without
# nettle/hogweed/gmp there is no RSA-AES and no WebSocket transport, so a
# wayvnc linked against such a library offers cleartext RFB and nothing else.
#
# gbm is 'enabled' because neatvnc.h declares nvnc_buffer_from_gbm_bo() and
# nvnc_frame_from_gbm_bo() unconditionally while src/buffer.c and src/frame.c
# define them only under HAVE_GBM: a consumer importing a DMA-BUF capture
# against a gbm-less library fails at link time, not at configure.
#
# h264 stays off. It has no public API — every use is behind ENABLE_OPEN_H264
# inside the library — so disabling it breaks no consumer, and enabling it
# would bind a VNC library's ABI to libavcodec, libavfilter and libavutil.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dtests=false \
	-Dexamples=false \
	-Dbenchmarks=false \
	-Dexperimental=false \
	-Dsystemtap=false \
	-Dtls=enabled \
	-Dnettle=enabled \
	-Djpeg=enabled \
	-Dgbm=enabled \
	-Dh264=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
