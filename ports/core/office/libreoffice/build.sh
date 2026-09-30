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

# The help tarball unpacks as libreoffice-$version/helpcontent2, which is
# $SRC/helpcontent2: --with-help finds it there and needs nothing else.
#
# THE VENDOR BUNDLE IS EVERY FILE download.lst NAMES, each at the name and
# sha256 download.lst gives it. --with-external-tar points the build at them
# and --disable-fetch-external makes a missing one a build error rather than a
# download. Which of them are unpacked is decided by the --with-system-* and
# --without-system-* flags below, so a flag moved from one list to the other
# needs no new source. Rebuild the bundle on a version bump: every
# *_SHA256SUM / *_TARBALL (or _JAR, _TTF) pair from the new download.lst,
# fetched from https://dev-www.libreoffice.org/src or /extern and checked
# against its sum, under externals/, packed with ports/fetch's vendor_tar
# flags plus --mode=go-w.
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"

# The gettext port's libintl.h renames bindtextdomain to
# libintl_bindtextdomain, which musl does not define: unotools, which calls
# it, links -lintl or it does not link.
patch -p1 -i "$PORT_SRC/musl-libintl.patch"
# musl gives a new thread 128 KiB of stack; the parser and layout threads
# overflow it. The patch makes sal size every thread's stack itself.
patch -p1 -i "$PORT_SRC/musl-stacksize.patch"

# The top-level Makefile refuses to run as root unless $container is set.
# The chroot build is root.
export container=kdos-chroot

export QT6DIR=/usr/lib/qt6

# Unless both are set, configure asks distutils for pyuno's embedding flags,
# and distutils left the standard library in Python 3.12.
PYTHON_CFLAGS=$(pkg-config --cflags python3-embed)
PYTHON_LIBS=$(pkg-config --libs python3-embed)
export PYTHON_CFLAGS PYTHON_LIBS

# System libraries where a port exists and LibreOffice accepts any release of
# it. Built from the bundle: every library with no port; harfbuzz and
# graphite, because a system harfbuzz must carry graphite2 and icu support,
# which the harfbuzz port leaves out; and boost, abseil, poppler, mdds and
# orcus, whose C++ APIs LibreOffice pins to the release download.lst names.
#
# Off by rule: Java (and with it HSQLDB, the report builder, BeanShell and
# Rhino), the CMIS cloud connectors, the Impress remote, online update and
# extension-update checks, and breakpad. Firebird is off because it is
# unproven on musl, so Base has no embedded database: it connects to
# PostgreSQL, LDAP and file sources (dBase, CSV, spreadsheets). Skia is off:
# gtk3 and qt6 draw through cairo and Qt, and only the plain X11 backend
# would use it.
bash ./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--mandir=/usr/share/man \
	--localstatedir=/var \
	--disable-dependency-tracking \
	--disable-ccache \
	--enable-release-build \
	--with-vendor=KDOS \
	--with-external-tar="$SRC_ROOT/externals" \
	--disable-fetch-external \
	--with-help=html \
	--without-java \
	--without-junit \
	--without-doxygen \
	--without-lxml \
	--without-krb5 \
	--without-gssapi \
	--disable-odk \
	--disable-online-update \
	--disable-extension-update \
	--disable-breakpad \
	--disable-libcmis \
	--disable-sdremote \
	--disable-sdremote-bluetooth \
	--disable-avahi \
	--disable-dconf \
	--disable-cpdb \
	--disable-skia \
	--disable-eot \
	--disable-coinmp \
	--enable-lpsolve \
	--disable-firebird-sdbc \
	--disable-mariadb-sdbc \
	--enable-postgresql-sdbc \
	--enable-ldap \
	--enable-cups \
	--enable-dbus \
	--enable-gio \
	--enable-randr \
	--enable-opengl \
	--enable-gstreamer-1-0 \
	--enable-pdfimport \
	--enable-pdfium \
	--enable-python=system \
	--enable-gtk3 \
	--disable-gtk4 \
	--enable-qt6 \
	--enable-qt6-multimedia \
	--disable-qt5 \
	--disable-kf5 \
	--disable-kf6 \
	--disable-gtk3-kde5 \
	--disable-introspection \
	--with-tls=nss \
	--without-fonts \
	--without-myspell-dicts \
	--with-system-dicts \
	--with-external-dict-dir=/usr/share/hunspell \
	--with-external-hyph-dir=/usr/share/hyphen \
	--with-external-thes-dir=/usr/share/mythes \
	--with-system-zlib \
	--with-system-bzip2 \
	--with-system-lzma \
	--with-system-zstd=yes \
	--with-system-expat \
	--with-system-libxml \
	--with-system-curl \
	--with-system-openssl \
	--with-system-nss \
	--with-system-icu \
	--with-system-freetype \
	--with-system-fontconfig \
	--with-system-cairo \
	--with-system-libpng \
	--with-system-jpeg \
	--with-system-libtiff \
	--with-system-libwebp \
	--with-system-lcms2 \
	--with-system-openjpeg \
	--with-system-epoxy \
	--with-system-glm \
	--with-system-hunspell \
	--with-system-altlinuxhyph \
	--with-system-mythes \
	--with-system-zxing \
	--with-system-gpgmepp \
	--with-system-openldap \
	--with-system-postgresql \
	--with-system-sqlite3 \
	--with-system-sane \
	--with-system-librevenge \
	--with-system-libwpd \
	--with-system-libwpg \
	--with-system-libcdr \
	--with-system-libvisio \
	--without-system-harfbuzz \
	--without-system-graphite \
	--without-system-boost \
	--without-system-abseil \
	--without-system-poppler \
	--without-system-afdko \
	--without-system-mdds \
	--without-system-orcus \
	--without-system-clucene \
	--without-system-argon2 \
	--without-system-box2d \
	--without-system-liblangtag \
	--without-system-libexttextcat \
	--without-system-libnumbertext \
	--without-system-redland \
	--without-system-xmlsec \
	--without-system-libodfgen \
	--without-system-libepubgen \
	--without-system-libwps \
	--without-system-libmspub \
	--without-system-libmwaw \
	--without-system-libetonyek \
	--without-system-libfreehand \
	--without-system-libebook \
	--without-system-libabw \
	--without-system-libpagemaker \
	--without-system-libqxp \
	--without-system-libzmf \
	--without-system-libstaroffice \
	--without-system-lpsolve \
	--without-system-colamd \
	--without-system-cppunit \
	--without-system-md4c \
	--without-system-dragonbox \
	--without-system-frozen \
	--without-system-fast-float \
	--without-system-libfixmath \
	--without-system-zxcvbn \
	--without-system-odbc

# `make` alone also runs the unit tests; `build` is the product.
make build
make DESTDIR="$PKG" distro-pack-install

# distro-pack-install leaves its per-module file lists (gid_Module_*) as plain
# files at the top of DESTDIR, where a distribution splitting the suite reads
# them. One package owns every path, and a file at / would be installed there.
find "$PKG" -mindepth 1 -maxdepth 1 -type f -delete

# The entries in share/applications are absolute links into
# /usr/lib/libreoffice/share/xdg. They resolve in the image and dangle
# anywhere else, so preflight's name, icon and command checks would skip
# every one of them. A copy is checked like any other entry.
for _e in "$PKG"/usr/share/applications/*.desktop; do
	[ -L "$_e" ] || continue
	cp --remove-destination "$PKG$(readlink "$_e")" "$_e"
	chmod 644 "$_e"
done

# Settings layered over the defaults, read like any other .xcd in the
# registry directory: no donation or get-involved bars, no what's-new bar or
# dialog after an upgrade (each opens a web page), and no crash-report
# consent, since no reporter is built.
cat > "$PKG/usr/lib/libreoffice/share/registry/kdos.xcd" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<oor:data xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:oor="http://openoffice.org/2001/registry">
<dependency file="main"/>
<oor:component-data oor:package="org.openoffice.Office.UI" oor:name="Infobar">
 <node oor:name="Enabled">
  <prop oor:name="Donate" oor:op="fuse"><value>false</value></prop>
  <prop oor:name="GetInvolved" oor:op="fuse"><value>false</value></prop>
  <prop oor:name="WhatsNew" oor:op="fuse"><value>false</value></prop>
 </node>
</oor:component-data>
<oor:component-data oor:package="org.openoffice.Office" oor:name="Common">
 <node oor:name="Misc">
  <prop oor:name="ShowDonation" oor:op="fuse"><value>false</value></prop>
  <prop oor:name="CrashReport" oor:op="fuse"><value>false</value></prop>
 </node>
</oor:component-data>
<oor:component-data oor:package="org.openoffice" oor:name="Setup">
 <node oor:name="Product">
  <prop oor:name="WhatsNew" oor:op="fuse"><value>false</value></prop>
  <prop oor:name="WhatsNewDialog" oor:op="fuse"><value>false</value></prop>
 </node>
</oor:component-data>
</oor:data>
EOF
chmod 644 "$PKG/usr/lib/libreoffice/share/registry/kdos.xcd"
