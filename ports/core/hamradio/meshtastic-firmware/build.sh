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

_dest=$PKG/usr/share/meshtastic-firmware/$version
for _arch in esp32 esp32c3 esp32c6 esp32s3 nrf52840 rp2040 rp2350 stm32; do
	install -dm755 "$_dest/$_arch"
	unzip -q -o "$name-$_arch-$version.zip" -d "$_dest/$_arch"
done
install -Dm644 "$name-$version.json" "$_dest/manifest.json"

rm -f "$_dest"/*/*.bat
find "$_dest" -type f -exec chmod 644 {} +
find "$_dest" -type f -name '*.sh' -exec chmod 755 {} +
