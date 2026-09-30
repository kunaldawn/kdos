# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A UNIT IS METADATA UNTIL SOMETHING CAN CONVERT IT. A netCDF variable carries
# `units = "kg m-2 s-1"` as a string; udunits is what parses that and answers
# whether it is commensurable with mm/day and by what factor — which is the
# difference between NCO reporting numbers and NCO reporting a quantity.
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
