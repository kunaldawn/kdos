# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# --without-x, AND OMITTING IT IS NOT THE SAME THING. autoconf's AC_PATH_XTRA
# runs by default, so a configure with no X flag at all LOOKS for X and stops
# on `Couldn't find Xaw library` — it has to be refused out loud. The X plotting
# is ngspice's own Xt window anyway; what a scriptable machine wants is the raw
# file, which gnuplot already draws. --with-readline=yes is what makes the
# interactive prompt usable at all.
#
# --enable-xspice AND --enable-cider are the two that turn this from a netlist
# simulator into the tool the catalogue names: XSPICE is the code-model
# interface every mixed-signal example uses, and CIDER is the device-level
# solver. Both default OFF and neither fails without its dependency.
./configure --prefix=/usr --libdir=/usr/lib --disable-static \
	--without-x --with-readline=yes --enable-xspice --enable-cider \
	--enable-openmp --disable-debug
make
make DESTDIR=$PKG install
