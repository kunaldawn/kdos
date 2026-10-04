# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# Alpine's musl series for the bundled Chromium, in its order. Without them the
# configure step refuses a libc that is not glibc, and the tree then fails on
# mallinfo, res_ninit, execinfo, TEMP_FAILURE_RETRY and the kernel/libc
# header clashes musl does not paper over; no-sandbox-settls keeps the zygote
# from a CLONE_SETTLS use musl's clone rejects at run time.
for p in 0001-Enable-building-on-musl 0002-temp-failure-retry \
	0003-qt-musl-mallinfo 0004-qt-musl-resolve \
	0005-qtwebengine-missing-include 0006-no-execinfo 0007-musl-sandbox \
	0008-missing-includes gcc13 no-sandbox-settls systypes fstatat-32bit \
	devtools-frontend-compress_files cr140-musl-prctl no-simd-fallback-fix; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

# gn is built by its own gen.py, which reads no CFLAGS or CXXFLAGS, and its
# pool.h names int64_t without including <cstdint>, which GCC 16's headers do
# not pull in on the way to their own.
patch -p1 -i "$PORT_SRC/gn-cstdint.patch"

# musl declares off64_t, stat64 and the other *64 names only under
# _LARGEFILE64_SOURCE, and Chromium's third-party code still spells them.
# The build pins __DATE__ and __TIME__ for reproducibility, which gcc warns
# about on every file. GCC 16's C++ library headers do not include
# <cstdint> on the way to their own, and Chromium names int64_t and the rest
# in many headers that never include it; -include cstdint puts it in
# front of every C++ file.
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE -Wno-builtin-macro-redefined -Wno-deprecated-declarations"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE -Wno-builtin-macro-redefined -Wno-deprecated-declarations -include cstdint"

# The Chromium build is a ninja run of its own inside Qt's; NINJAFLAGS is its
# only job limit, and unbounded it takes every core at 2-3 GB each.
export NINJAFLAGS="$MAKEFLAGS"
export NODEJS_EXECUTABLE=/usr/bin/node

# gn: Qt's fork is built from the tarball; the tree's gn is upstream's and is
#   not the version Qt pins.
# FFmpeg, libvpx, re2, snappy, openh264, minizip, libxml2/libxslt: Chromium's
#   bundled copies. The system FFmpeg is a newer major than Chromium 140's
#   media code accepts, VA-API cannot be built against a system libvpx, and
#   the system libxml2 is newer than the API Blink's XSLT code is written to.
# ozone-x11 needs every X library in depends, or it is silently dropped and
#   XWayland-only callers lose the engine.
mkdir build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DQT_BUILD_TESTS=OFF \
	-DQT_BUILD_EXAMPLES=OFF \
	-DFEATURE_qtwebengine_build=ON \
	-DFEATURE_qtpdf_build=ON \
	-DFEATURE_webengine_build_gn=ON \
	-DFEATURE_webengine_rust_build=OFF \
	-DFEATURE_webengine_jumbo_build=8 \
	-DFEATURE_webengine_developer_build=OFF \
	-DFEATURE_webengine_full_debug_info=OFF \
	-DFEATURE_webengine_embedded_build=OFF \
	-DFEATURE_webenginedriver=OFF \
	-DFEATURE_webengine_ozone_x11=ON \
	-DFEATURE_webengine_vulkan=ON \
	-DFEATURE_webengine_vaapi=ON \
	-DFEATURE_webengine_system_gbm=ON \
	-DFEATURE_webengine_system_alsa=ON \
	-DFEATURE_webengine_system_pulseaudio=ON \
	-DFEATURE_webengine_webrtc=ON \
	-DFEATURE_webengine_webrtc_pipewire=ON \
	-DFEATURE_webengine_webrtc_system_openh264=OFF \
	-DFEATURE_webengine_proprietary_codecs=ON \
	-DFEATURE_webengine_printing_and_pdf=ON \
	-DFEATURE_webengine_pepper_plugins=ON \
	-DFEATURE_webengine_spellchecker=ON \
	-DFEATURE_webengine_extensions=ON \
	-DFEATURE_webengine_geolocation=ON \
	-DFEATURE_webengine_webchannel=ON \
	-DFEATURE_webengine_kerberos=ON \
	-DFEATURE_webengine_v8_context_snapshot=ON \
	-DFEATURE_webengine_system_icu=ON \
	-DFEATURE_webengine_system_glib=ON \
	-DFEATURE_webengine_system_zlib=ON \
	-DFEATURE_webengine_system_libpng=ON \
	-DFEATURE_webengine_system_libjpeg=ON \
	-DFEATURE_webengine_system_libtiff=ON \
	-DFEATURE_webengine_system_libwebp=ON \
	-DFEATURE_webengine_system_libopenjpeg2=ON \
	-DFEATURE_webengine_system_opus=ON \
	-DFEATURE_webengine_system_lcms2=ON \
	-DFEATURE_webengine_system_harfbuzz=ON \
	-DFEATURE_webengine_system_freetype=ON \
	-DFEATURE_webengine_system_libpci=ON \
	-DFEATURE_webengine_system_libudev=ON \
	-DFEATURE_webengine_system_ffmpeg=OFF \
	-DFEATURE_webengine_system_libvpx=OFF \
	-DFEATURE_webengine_system_re2=OFF \
	-DFEATURE_webengine_system_snappy=OFF \
	-DFEATURE_webengine_system_openh264=OFF \
	-DFEATURE_webengine_system_minizip=OFF \
	-DFEATURE_webengine_system_libxml=OFF
ninja
DESTDIR=$PKG ninja install

# English only: the engine loads en-US.pak and falls back to it for any
# other locale, so the other 50-odd locale packs are dead weight.
find "$PKG/usr/share/qt6/translations/qtwebengine_locales" -name '*.pak' \
	! -name 'en-US.pak' ! -name 'en-GB.pak' -delete
