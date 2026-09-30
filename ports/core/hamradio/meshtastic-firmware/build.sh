#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# CODE FOR ANOTHER PROCESSOR. These are the images upstream builds for the
# radio's own microcontroller (ESP32, nRF52840, RP2040/RP2350, STM32); none
# of it runs on the host, and a node is flashed from them with no network:
# `esptool.py write_flash 0x0 <board>.factory.bin` for an ESP32, or the .uf2
# copied onto the board's USB drive for the nRF52 and RP2040 families.
#
# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe does,
# each family into a directory of its own under the version, so a board's
# images are found under the chip that runs them and the release they
# belong to is in the path.
_dest=$PKG/usr/share/meshtastic-firmware/$version
for _arch in esp32 esp32c3 esp32c6 esp32s3 nrf52840 rp2040 rp2350 stm32; do
	install -dm755 "$_dest/$_arch"
	unzip -q -o "$name-$_arch-$version.zip" -d "$_dest/$_arch"
done
install -Dm644 "$name-$version.json" "$_dest/manifest.json"

# device-install.sh and device-update.sh drive esptool for an ESP32 board;
# the .bat copies beside them are the same scripts for Windows.
rm -f "$_dest"/*/*.bat
find "$_dest" -type f -exec chmod 644 {} +
find "$_dest" -type f -name '*.sh' -exec chmod 755 {} +
