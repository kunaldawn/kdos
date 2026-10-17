# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

rm -f enc_bootloader/enc_bootloader.elf enc_bootloader/enc_bootloader_mbedtls.elf \
	xip_ram_perms/xip_ram_perms.elf picoboot_flash_id/flash_id.bin
export PICO_TOOLCHAIN_PATH=/usr PICO_MBEDTLS_PATH="$SRC_ROOT/mbedtls-$_mbedtls"
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPICOTOOL_NO_LIBUSB=OFF \
	-DUSE_PRECOMPILED=OFF \
	-DPICO_SDK_PATH="$SRC_ROOT/pico-sdk-$version"
make
DESTDIR=$PKG cmake --install .
test -n "$(find "$PKG" -name enc_bootloader_mbedtls.elf)"
