# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DUNITTEST=OFF -DOPTIONAL_DNSSD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_GPSD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_hamlib=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_gpiod=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_udev=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Avahi=ON
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/direwolf.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Dire Wolf
Comment=APRS soundcard TNC — AX.25 off the sound card
Exec=direwolf
Icon=network-wireless
Terminal=true
Categories=Network;HamRadio
Keywords=Ham Radio;APRS;Soundcard TNC;KISS;AGWPE;AX.25
ENTRY
