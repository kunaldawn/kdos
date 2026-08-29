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
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPICO_SDK_PATH="$SRC_ROOT/pico-sdk-$version"
make
make DESTDIR=$PKG install
