# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

for p in 0001-Enable-building-on-musl 0002-temp-failure-retry \
	0003-qt-musl-mallinfo 0004-qt-musl-resolve \
	0005-qtwebengine-missing-include 0006-no-execinfo 0007-musl-sandbox \
	0008-missing-includes gcc13 no-sandbox-settls systypes fstatat-32bit \
	devtools-frontend-compress_files cr140-musl-prctl no-simd-fallback-fix; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done
patch -p1 -i "$PORT_SRC/gn-cstdint.patch"
patch -p1 -i "$PORT_SRC/gn-ldflags.patch"
patch -p1 -i "$PORT_SRC/sandbox-path-max.patch"
export CFLAGS="$CFLAGS -D_LARGEFILE64_SOURCE -Wno-builtin-macro-redefined -Wno-deprecated-declarations"
export CXXFLAGS="$CXXFLAGS -D_LARGEFILE64_SOURCE -Wno-builtin-macro-redefined -Wno-deprecated-declarations"
export NINJAFLAGS="$MAKEFLAGS"
export NODEJS_EXECUTABLE=/usr/bin/node
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
	-DFEATURE_webengine_system_minizip=ON \
	-DFEATURE_webengine_system_libxml=OFF
ninja
test -z "$(find . -path '*/obj/third_party/zlib/bundled_zlib/*' -name '*.o')"
test -z "$(find . -path '*/obj/third_party/zlib/bundled_minizip/*' -name '*.o')"
DESTDIR=$PKG ninja install
rm -f "$PKG/usr/bin/QtWebEngineProcess6"
find "$PKG/usr/share/qt6/translations/qtwebengine_locales" -name '*.pak' \
	! -name 'en-US.pak' ! -name 'en-GB.pak' -delete
