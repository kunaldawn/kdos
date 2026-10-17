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
	--localstatedir=/var --disable-static \
	--enable-imagefilters --enable-exif --enable-texttopdf \
	--enable-ghostscript --enable-dbus --disable-mutool \
	--with-jpeg --with-png --with-tiff --with-jpegxl --with-fontconfig \
	--with-cups-rundir=/run/cups \
	--with-test-font-path=/usr/share/fonts/TTF/DejaVuSans.ttf
make
make DESTDIR=$PKG install
