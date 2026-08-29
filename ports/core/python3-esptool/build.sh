# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE ROM BOOTLOADER IS IN MASK ROM AND CANNOT BE BRICKED, which is what makes
# this the one flashing tool that always works: an ESP32 with nothing valid in
# flash still answers esptool. Reaching it needs the dialout group and the
# CP210x/CH341 udev rules this tree already ships.
#
# The bundle is the pure-python remainder; pyserial and cryptography are ports
# and pip finds them installed. Build isolation stays ON and pointed at the
# bundle, because these build with hatchling and setuptools-scm rather than
# plain setuptools.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
pip3 install --no-index --find-links=vendor --root=$PKG --prefix=/usr .
