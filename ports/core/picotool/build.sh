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
# defined")`. The host program takes the bootrom and UF2 headers from it, and
# the three device stubs picotool embeds are built with it.
#
# An extra archive source is extracted into $SRC_ROOT unstripped, so the SDK
# tree sits beside $SRC under its own name.
#
# THE DEVICE STUBS ARE COMPILED HERE. enc_bootloader.elf, xip_ram_perms.elf
# and flash_id.bin run on an RP2350; picotool embeds all three and installs the
# two .elf files under /usr/share/picotool. The release tarball carries them
# prebuilt; USE_PRECOMPILED=OFF builds each as a pico-sdk project with the
# arm-none-eabi toolchain instead, and the prebuilt copies are deleted first so
# a build that fell back to them fails rather than shipping them. Every pico-sdk
# project finds its compiler through PICO_TOOLCHAIN_PATH, read from the
# environment because the sub-builds are separate CMake runs. The SDK's
# new_delete.cpp is part of every stub linked against pico_stdlib, so the stubs
# need libstdc++ headers and libraries: libstdcxx-arm-none-eabi.
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
rm -f enc_bootloader/enc_bootloader.elf enc_bootloader/enc_bootloader_mbedtls.elf \
	xip_ram_perms/xip_ram_perms.elf picoboot_flash_id/flash_id.bin
export PICO_TOOLCHAIN_PATH=/usr
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPICOTOOL_NO_LIBUSB=OFF \
	-DUSE_PRECOMPILED=OFF \
	-DPICO_SDK_PATH="$SRC_ROOT/pico-sdk-$version"
make
# cmake --install, not `make install`: the install target reruns each stub's
# ExternalProject build and install step, and with DESTDIR in the environment
# the stubs' own install rules copy them into $PKG under the source tree's path.
DESTDIR=$PKG cmake --install .
