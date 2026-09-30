# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# THE BACKEND IS libusb-1.0, FOUND AT RUN TIME. pyusb is pure python and loads
# the library through ctypes.util.find_library, which on this system answers
# through gcc and objdump rather than an ldconfig cache; libusb is in depends
# because a pyusb with no library to load reports "No backend available" for
# every device.
#
# --no-build-isolation because setuptools and setuptools-scm are installed
# ports; the sdist's PKG-INFO carries the version, so no git history is read.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .
