# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# OBD-II IS THE ONE MANUFACTURER-INDEPENDENT DIAGNOSTIC INTERFACE, and the
# adapter that speaks it is a serial port — so the whole of this is pyserial
# plus a protocol. Reaching the adapter needs the `dialout` group, which
# fs/etc/group already grants the console user, and the udev rules under
# fs/etc/udev/rules.d that give that group the CH341 and CP210x bridges these
# cables are built from.
#
# --no-build-isolation because setuptools is an installed port; --no-deps
# because pyserial is one too.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
