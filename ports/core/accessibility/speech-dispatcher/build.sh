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

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-ltdl \
	--with-espeak-ng \
	--without-espeak \
	--without-flite \
	--without-pico \
	--without-ivona \
	--without-ibmtts \
	--without-voxin \
	--without-baratinoo \
	--without-kali \
	--with-alsa \
	--without-libao \
	--without-pulse \
	--without-pipewire \
	--without-nas \
	--without-oss \
	--without-systemdsystemunitdir \
	--without-systemduserunitdir \
	--enable-python
make LDFLAGS="$LDFLAGS"
make LDFLAGS="$LDFLAGS" DESTDIR=$PKG install

rm -f "$PKG"/usr/libexec/speech-dispatcher-modules/sd_{cicero,dummy,espeak-ng-mbrola,festival,generic,openjtalk}
find "$PKG/etc/speech-dispatcher/modules" "$PKG/usr/share/speech-dispatcher/conf/modules" \
	-type f ! -name espeak-ng.conf -delete
