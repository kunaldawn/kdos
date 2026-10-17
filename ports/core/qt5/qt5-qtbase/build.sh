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

patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/lfs64.patch"
patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/qt-musl-iconv-no-bom.patch"
patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/qt5-base-cflags.patch"
patch -p1 --no-backup-if-mismatch -i "$PORT_SRC/egl-x11.patch"

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
export CPATH="$_ossl/include"
export OPENSSL_LIBS="/usr/lib/libssl.so.3 /usr/lib/libcrypto.so.3"

./configure \
	-confirm-license -opensource -release -optimized-qmake -shared \
	-prefix /usr \
	-libdir /usr/lib \
	-archdatadir /usr/lib/qt5 \
	-bindir /usr/lib/qt5/bin \
	-libexecdir /usr/lib/qt5/libexec \
	-plugindir /usr/lib/qt5/plugins \
	-importdir /usr/lib/qt5/imports \
	-qmldir /usr/lib/qt5/qml \
	-headerdir /usr/include/qt5 \
	-datadir /usr/share/qt5 \
	-translationdir /usr/share/qt5/translations \
	-docdir /usr/share/doc/qt5 \
	-examplesdir /usr/share/doc/qt5/examples \
	-sysconfdir /etc/xdg \
	-nomake examples -nomake tests \
	-no-rpath -no-pch -no-reduce-relocations -no-separate-debug-info \
	-pkg-config \
	-dbus-linked \
	-glib \
	-icu \
	-no-mimetype-database \
	-system-doubleconversion \
	-system-pcre \
	-system-zlib \
	-zstd \
	-no-journald -no-syslog \
	-openssl-linked -dtls -ocsp \
	-no-feature-gssapi \
	-no-libproxy -system-proxies \
	-no-sctp \
	-fontconfig \
	-system-freetype \
	-system-harfbuzz \
	-system-libpng \
	-system-libjpeg \
	-gif -ico \
	-qt-libmd4c \
	-opengl desktop -egl -vulkan \
	-xcb -xcb-xlib -no-bundled-xcb-xinput -xkbcommon \
	-feature-xkbcommon-x11 \
	-feature-xcb-glx-plugin \
	-feature-xcb-egl-plugin \
	-no-feature-xcb-sm \
	-no-eglfs -no-gbm -no-kms -no-linuxfb -no-directfb -no-feature-vnc \
	-libudev -evdev -libinput -mtdev -no-tslib \
	-accessibility -feature-accessibility-atspi-bridge \
	-cups \
	-no-gtk \
	-sql-sqlite -system-sqlite \
	-no-sql-psql -no-sql-mysql -no-sql-odbc -no-sql-tds \
	-no-sql-db2 -no-sql-ibase -no-sql-oci -no-sql-sqlite2
make EXTRA_LFLAGS="$LDFLAGS"
make INSTALL_ROOT="$PKG" install

install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	_tool="${_tool##*/}"
	case "$_tool" in
	*.*) ln -s "../lib/qt5/bin/$_tool" "$PKG/usr/bin/${_tool%.*}-qt5.${_tool##*.}" ;;
	*) ln -s "../lib/qt5/bin/$_tool" "$PKG/usr/bin/$_tool-qt5" ;;
	esac
done
ln -s ../lib/qt5/bin/qmake "$PKG/usr/bin/qmake"
strip --strip-debug "$PKG/usr/lib/qt5/bin/qmake"
if readelf -SW "$PKG/usr/lib/qt5/bin/qmake" | grep -q '\.debug_info'; then
	echo "qt5-qtbase: qmake carries debug information" >&2
	exit 1
fi
