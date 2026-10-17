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

autoreconf -f -i

export XML_CATALOG_FILES=/etc/xml/catalog

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--without-gtk \
	--without-qt \
	--without-java \
	--without-python \
	--with-x \
	--with-xshm \
	--without-xv \
	--enable-video \
	--with-imagemagick \
	--without-graphicsmagick \
	--without-gir \
	--with-jpeg \
	--with-dbus \
	--enable-doc \
	--enable-nls \
	XMLTO=xmlto \
	ac_cv_header_libv4l2_h=yes
if grep -q '^#define X_DISPLAY_MISSING' include/config.h; then
	echo "zbar: libX11 not found, zbarcam would have no window" >&2
	exit 1
fi
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/zbarcam.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Barcode Scanner
GenericName=QR and Barcode Reader
Comment=Read QR codes and barcodes held up to the camera
Exec=zbarcam
Icon=camera-web
Terminal=true
Categories=AudioVideo;Video;
Keywords=qr;barcode;scan;camera;zbar;
DESKTOP
chmod 644 "$PKG/usr/share/applications/zbarcam.desktop"
