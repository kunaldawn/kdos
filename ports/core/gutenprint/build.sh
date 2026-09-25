# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A CONSUMER INKJET WITH NO DRIVER PRINTS WASHED-OUT GARBAGE. cups' own
# everywhere/driverless path covers modern network printers and nothing else;
# gutenprint is what makes ~5000 older USB inkjets and dye-subs produce the
# colours they were sold with.
#
# The two GUI halves are off by rule — libgutenprintui2 is GTK and the gimp
# plugin needs GIMP, neither of which exists on this host. What ships is the
# driver, the CUPS backend and the PPD generator. The dye-sub backend
# (gutenprint53+usb) is built only when configure finds libusb-1.0, which is
# why libusb is a dependency. readline gives escputil's interactive head
# alignment prompts line editing; without it they fall back to a bare fgets.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--with-cups --with-modules=dlopen \
	--disable-libgutenprintui2 --without-gimp2 \
	--with-readline --without-doc --disable-nls \
	--disable-test --disable-testpattern
make
make DESTDIR=$PKG install
