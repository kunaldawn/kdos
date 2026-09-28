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


# musl has no *64 file calls and no BOM-less UTF-16 alias in iconv: the first
# two patches point Qt at the plain names and the BOM-less spelling. The third
# makes qmake read CFLAGS, CXXFLAGS and LDFLAGS from the environment, here and
# in every later Qt 5 module and qmake program, or the reproducibility flags
# (-ffile-prefix-map, --build-id) never reach a Qt 5 binary. The fourth passes
# the EGL-on-X11 probe, which only has to show that the headers and libraries
# are there.
patch -p1 -i "$PORT_SRC/lfs64.patch"
patch -p1 -i "$PORT_SRC/qt-musl-iconv-no-bom.patch"
patch -p1 -i "$PORT_SRC/qt5-base-cflags.patch"
patch -p1 -i "$PORT_SRC/egl-x11.patch"

# QT 5 NETWORK IS BUILT AGAINST OPENSSL 3, NOT 4. QtNetwork reads the length
# and data of an ASN1_STRING directly and wraps OpenSSL 4's const-corrected
# X509 accessors in non-const signatures, so it does not compile against the
# openssl port. It links the openssl3 slot's libssl.so.3 and libcrypto.so.3 by
# path, which records their sonames and never the OpenSSL 4 libssl.so. The
# slot ships no headers, so they are generated here from the same release with
# the openssl3 port's own configuration: an option set that differs defines a
# different OPENSSL_NO_* and QtNetwork then calls what the library lacks.
# CPATH puts them ahead of /usr/include for this build and is recorded nowhere.
#
# NOTHING ELSE QtNetwork LINKS MAY BRING OPENSSL 4 IN. musl's loader has one
# symbol namespace and ignores symbol versions, so with libcrypto.so.3 and
# libcrypto.so.4 in one process each resolves the other's calls to whichever
# loaded first. GSSAPI is off for that reason: krb5 links OpenSSL 4, and it
# serves only Negotiate authentication in QNetworkAccessManager.
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

# QT 5 LIVES BESIDE QT 6 WITHOUT SHARING A PATH. Libraries are libQt5*.so in
# /usr/lib; headers, tools, plugins, QML modules and mkspecs are under
# /usr/include/qt5, /usr/lib/qt5 and /usr/share/qt5; CMake packages are Qt5*
# and pkg-config files Qt5*.pc. Qt 6 uses the same layout with a 6.
#
# XCB IS THE BUILT-IN PLATFORM; WAYLAND COMES FROM qt5-qtwayland. A program
# starts on Wayland when XDG_SESSION_TYPE=wayland or QT_QPA_PLATFORM names it,
# and falls back to xcb under Xwayland. xcb needs xkbcommon-x11, and the GLX
# integration needs GLX from libglvnd and mesa: with either missing, its
# -feature- flag stops configure instead of dropping the backend. The XSMP
# session client is off: there is no session manager to talk to.
#
# EGLFS, LINUXFB, KMS AND VNC ARE OFF. A Qt program started outside kdos-comp
# would otherwise take the card and the input devices from the compositor
# already drawing on them.
#
# The accessibility bridge needs its explicit flag, or no Qt 5 program is
# visible to a screen reader. The bundled MIME database is off: QMimeDatabase
# reads shared-mime-info, the same database every other program reads.
# journald is off (no systemd); libproxy and the GTK platform theme are off;
# md4c is the bundled copy because it is not a port. Only the SQLite driver
# is built.
./configure \
	-confirm-license -opensource -release -shared \
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
make
make INSTALL_ROOT="$PKG" install

# Every tool gets a -qt5 name in /usr/bin, which is what a Qt 5 build system
# asks for when a Qt 6 tool of the same name may be there too. qmake also
# keeps its plain name: Qt 6 installs only qmake6, and a qmake project that
# says to run `qmake` means this one.
install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	_tool="${_tool##*/}"
	case "$_tool" in
	*.*) ln -s "../lib/qt5/bin/$_tool" "$PKG/usr/bin/${_tool%.*}-qt5.${_tool##*.}" ;;
	*) ln -s "../lib/qt5/bin/$_tool" "$PKG/usr/bin/$_tool-qt5" ;;
	esac
done
ln -s ../lib/qt5/bin/qmake "$PKG/usr/bin/qmake"
