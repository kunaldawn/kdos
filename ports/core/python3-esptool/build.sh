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
# The bundle is the remainder of the runtime closure that is not a port, named
# one by one and installed --no-deps: pyserial, cryptography, PyYAML, Pygments
# and click are ports, and a resolving install would leave out whichever of the
# bundle's modules another package had already put in the build root. Build
# isolation stays ON and pointed at the bundle, because markdown-it-py and mdurl
# need a flit_core below 4 and rich needs poetry-core.
#
# BUILD_EXTENSION=yes makes websockets' C speedups a hard requirement; unset,
# a failed compile falls back to pure python without a word.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor
BUILD_EXTENSION=yes pip3 install --no-deps --no-index --find-links=vendor --root=$PKG --prefix=/usr \
	bitarray bitstring esp-pylib intelhex reedsolo rich rich-click markdown-it-py mdurl websockets .
