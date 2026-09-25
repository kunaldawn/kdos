# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A $200 HARDWARE TNC REPLACED BY A CABLE. direwolf demodulates AX.25 in
# software off the sound card, so an APRS station or a KISS TNC is a radio, an
# audio cable and this — and the ax25-tools beside it turn that into a real
# network interface.
# Every optional backend is a bare find_package() that quietly drops the
# feature when it misses; the CMAKE_REQUIRE_FIND_PACKAGE_ switches turn each
# one into a configure failure, so gpsd, hamlib, GPIO and CM108 PTT and the
# DNS-SD announce are always in the binary.
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DUNITTEST=OFF -DOPTIONAL_DNSSD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_GPSD=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_hamlib=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_gpiod=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_udev=ON \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Avahi=ON
make
make DESTDIR=$PKG install

# UPSTREAM'S LAUNCHER IS `xterm -e direwolf` AND THERE IS NO X HERE. A desktop
# entry whose Exec names a program this image cannot contain is a Start menu
# row that opens nothing; and `direwolf_icon.png` is a name no context of the
# shipped atlas carries, so the row lost its picture as well. Replaced whole
# rather than patched: the entry is three facts and every one of them is this
# distribution's, not upstream's.
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
