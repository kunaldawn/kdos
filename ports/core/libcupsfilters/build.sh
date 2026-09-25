# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# EVERY FILTER FUNCTION IS NAMED. cups-filters, libppd and cups-browsed are
# thin callers into this library, so a switch left at its default here is a
# filter every one of them silently lacks. --disable-mutool: mupdf is a port
# but its path duplicates the pdftoppm one. --with-jpegxl fails configure when
# libjxl is absent rather than building without the JPEG-XL reader.
#
# configure runs `gs` and `pdftoppm` to find them, so ghostscript and poppler
# are build dependencies as well as run-time ones.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--enable-imagefilters --enable-exif --enable-texttopdf \
	--enable-ghostscript --enable-dbus --disable-mutool \
	--with-jpeg --with-png --with-tiff --with-jpegxl --with-fontconfig \
	--with-cups-rundir=/run/cups \
	--with-test-font-path=/usr/share/fonts/TTF/DejaVuSans.ttf
make
make DESTDIR=$PKG install
