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
# THE DEVICE LIBRARIES ARE THE C RUNTIME. Without them there is no crt0, no
# stdlib and no multiply/divide helpers for mcs51, z80 or stm8, and an ordinary
# program fails to link. ucsim is the simulator sdcdb drives, so code runs and
# is stepped through with no board attached. Both are built from this tarball
# with no further dependency; ucsim's serial-console tool finds ncurses and
# sdcdb finds readline, which is why both are declared.
./configure --prefix=/usr \
	--disable-pic14-port --disable-pic16-port \
	--disable-doc
make
make DESTDIR=$PKG install
