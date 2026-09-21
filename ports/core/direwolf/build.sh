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
mkdir -p build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DUNITTEST=OFF
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
