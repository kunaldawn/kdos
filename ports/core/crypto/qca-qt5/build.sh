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

_ossl="$SRC_ROOT/openssl-$_openssl"
(
	cd "$_ossl"
	./config \
		--prefix=/usr \
		--libdir=lib \
		--openssldir=/etc/ssl \
		enable-ec_nistp_64_gcc_128 \
		enable-camellia \
		enable-seed \
		enable-rfc3779 \
		enable-ktls \
		enable-argon2 \
		no-mdc2 \
		no-ec2m \
		no-sm2 \
		no-sm4 \
		no-module \
		no-tests \
		shared \
		threads \
		zlib
	make build_generated
)
cmake -S . -B build -G Ninja \
	-D CMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_BUILD_TYPE=Release \
	-D BUILD_WITH_QT6=OFF \
	-D QCA_SUFFIX=qt5 \
	-D BUILD_TESTS=OFF \
	-D BUILD_TOOLS=OFF \
	-D BUILD_PLUGINS="ossl;gnupg;cyrus-sasl;logger;softstore" \
	-D QCA_FEATURE_INSTALL_DIR=/usr/lib/qt5/mkspecs/features \
	-D OPENSSL_INCLUDE_DIR="$_ossl/include" \
	-D OPENSSL_SSL_LIBRARY=/usr/lib/libssl.so.3 \
	-D OPENSSL_CRYPTO_LIBRARY=/usr/lib/libcrypto.so.3 \
	-D CMAKE_DISABLE_FIND_PACKAGE_Doxygen=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build
_need=$(readelf -d "$PKG/usr/lib/qca-qt5/crypto/libqca-ossl.so")
case $_need in *libssl.so.3*) ;; *) exit 1 ;; esac
case $_need in *libssl.so.4*|*libcrypto.so.4*) exit 1 ;; esac
