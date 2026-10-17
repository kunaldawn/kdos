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

tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" -C "$SRC_ROOT"

patch -p1 -i "$PORT_SRC/musl-libintl.patch"
patch -p1 -i "$PORT_SRC/musl-stacksize.patch"
patch -p1 -i "$PORT_SRC/sfinae-probe-cxxflags.patch"
patch -p1 -i "$PORT_SRC/externals-ldflags.patch"

export container=kdos-chroot

export QT6DIR=/usr/lib/qt6

PYTHON_CFLAGS=$(pkg-config --cflags python3-embed)
PYTHON_LIBS=$(pkg-config --libs python3-embed)
export PYTHON_CFLAGS PYTHON_LIBS

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
	--with-system-harfbuzz \
	--with-system-graphite \
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

export ENVCFLAGS=-D_LARGEFILE64_SOURCE ENVCFLAGSCXX=-D_LARGEFILE64_SOURCE

make build
make DESTDIR="$PKG" distro-pack-install

find "$PKG" -mindepth 1 -maxdepth 1 -type f -delete

for _e in "$PKG"/usr/share/applications/*.desktop; do
	[ -L "$_e" ] || continue
	_t=$(readlink "$_e")
	rm "$_e"
	cp "$PKG$_t" "$_e"
	chmod 644 "$_e"
done

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
