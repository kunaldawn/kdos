# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE 8-BIT FAMILIES avr-gcc DOES NOT COVER. 8051, STM8, Z80/eZ80 and the PDK
# parts are still what a great deal of cheap hardware is built from, and gcc
# has no back end for any of them.
#
# THE PIC PORTS ARE OUT. sdcc's PIC14/PIC16 back ends need gputils — gpasm and
# gplink — which is a separate project and not a port here; without it the
# configure succeeds and the pic targets fail at link time inside a build that
# has otherwise finished.
#
# --disable-ucsim and --disable-device-lib because the simulator and the
# prebuilt device libraries are a second build each, and what is wanted is the
# compiler.
./configure --prefix=/usr \
	--disable-pic14-port --disable-pic16-port \
	--disable-ucsim --disable-device-lib \
	--disable-doc
make
make DESTDIR=$PKG install
