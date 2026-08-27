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
# driver, the CUPS backend and the PPD generator.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--localstatedir=/var --disable-static \
	--disable-libgutenprintui2 --without-gimp2 \
	--disable-test --disable-testpattern
make
make DESTDIR=$PKG install
