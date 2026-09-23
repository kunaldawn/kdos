# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE SDK IS A SECOND source= BECAUSE picotool CANNOT CONFIGURE WITHOUT IT:
# CMakeLists.txt opens with `message(FATAL_ERROR "PICO_SDK_PATH is not
# defined")`. What it wants from the SDK is the bootrom and UF2 headers that
# describe the layout of an RP2040/RP2350 image, not a cross toolchain — this
# is a HOST program that reads and writes those files, and nothing here is
# compiled for the target.
#
# An extra archive source is extracted into $SRC_ROOT unstripped, so the SDK
# tree sits beside $SRC under its own name.
#
# USB (load, reboot, info and otp against a board in BOOTSEL) has only an off
# switch, and find_package(LIBUSB) drops it with a message when libusb is
# missing: libusb in depends is what keeps it, and PICOTOOL_NO_LIBUSB=OFF
# states the intent.
#
# Signing, hashing and encryption (seal --sign, encrypt) are off. picotool
# compiles mbedtls from a source tree, 2.28 or 3.6 only, found at
# PICO_MBEDTLS_PATH or the SDK's lib/mbedtls — a submodule the SDK tarball
# carries empty — and the mbedtls port is 4.x, which it does not build.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPICOTOOL_NO_LIBUSB=OFF \
	-DPICO_SDK_PATH="$SRC_ROOT/pico-sdk-$version"
make
make DESTDIR=$PKG install
