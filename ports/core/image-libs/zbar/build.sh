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

# THE X OVERLAY IS zbarcam's VIEWFINDER, and it is the one front end built.
# Without it zbarcam runs only with --nodisplay and the user aims a camera
# blind; with it the camera picture is an X window under Xwayland while the
# decoded text goes to standard output. X is detected rather than required,
# so the config.h check below makes a missing libX11 a failed build. MIT-SHM
# (libXext) is named so the picture is not copied through the socket; XVideo
# is off, because the XImage path already draws it and Xv under Xwayland
# depends on the GPU driver.
#
# THE WIDGETS ARE OFF. zbar's Qt 6 widget is marked broken by upstream, and
# its Qt 5 and GTK widgets draw the same X overlay into an X window id, which
# a Wayland-native Qt or GTK window does not have. Java and Python bindings
# have no consumer here.
#
# LIBV4L2 IS FORCED ON. zbarcam reads a camera through libv4l2 when configure
# finds libv4l2.h, which converts the pixel formats many webcams deliver and
# zbar cannot read raw. There is no switch for it, so the header's cache
# variable is preset and PKG_CHECK_MODULES then fails configure if v4l-utils
# is missing, instead of shipping a zbarcam that cannot see those cameras.
#
# --without-graphicsmagick IS WHAT MAKES --with-imagemagick BINDING: while the
# GraphicsMagick fallback is left at "check", a MagickWand that pkg-config does
# not find turns image scanning off with a notice and zbarimg is not built.
# --without-gir: the introspection data describes the GTK widget, which is off.
#
# XMLTO IS NAMED, NOT SEARCHED FOR: --enable-doc only asks, and a missing xmlto
# drops the man pages with no message. Preset, the program is run at build
# time and its absence stops the build.
#
# xmlto AND docbook-xsl: the man pages are built from docbook, and the
# stylesheet is named by its sourceforge URL. Only docbook-xsl's XML catalog
# rewrites that to the local copy — without it xsltproc tries to FETCH it and
# the build dies inside xmlto with an unresolved external entity. The catalog
# has to be NAMED: libxml2's compiled-in default is not what this build sees.
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

# zbarcam prints each code it reads, so the entry runs it in a terminal: the
# camera picture is its own window and the text lands where it can be copied.
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
Categories=Utility;Video;
Keywords=qr;barcode;scan;camera;zbar;
DESKTOP
chmod 644 "$PKG/usr/share/applications/zbarcam.desktop"
