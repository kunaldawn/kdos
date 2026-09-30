# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# OpenSSL is the TLS backend and libuv the optional event loop, linked into
# the library rather than loaded as a runtime plugin, so a consumer that asks
# for the uv loop cannot find it missing. zlib with extensions on gives
# permessage-deflate. External poll stays on for the consumers that drive the
# library from their own poll() loop. The test applications and minimal
# examples are not built; they would install a demo server.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DDISABLE_WERROR=ON \
	-DLWS_WITH_STATIC=OFF \
	-DLWS_WITH_SHARED=ON \
	-DLWS_WITH_SSL=ON \
	-DLWS_WITH_MBEDTLS=OFF \
	-DLWS_WITH_WOLFSSL=OFF \
	-DLWS_WITH_LIBUV=ON \
	-DLWS_WITH_EVLIB_PLUGINS=OFF \
	-DLWS_WITH_LIBEV=OFF \
	-DLWS_WITH_LIBEVENT=OFF \
	-DLWS_WITH_GLIB=OFF \
	-DLWS_WITH_SDEVENT=OFF \
	-DLWS_WITH_ZLIB=ON \
	-DLWS_WITHOUT_EXTENSIONS=OFF \
	-DLWS_WITH_HTTP2=ON \
	-DLWS_IPV6=ON \
	-DLWS_WITH_SOCKS5=ON \
	-DLWS_WITH_EXTERNAL_POLL=ON \
	-DLWS_WITHOUT_TESTAPPS=ON \
	-DLWS_WITH_MINIMAL_EXAMPLES=OFF
ninja
DESTDIR=$PKG ninja install

test -e "$PKG/usr/lib/pkgconfig/libwebsockets.pc"
