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

patch -p1 -i "$PORT_SRC/default-modules.patch"

modules=(
	account alsa aubridge auconv aufile augain auresamp ausine
	avcodec avfilter avformat swscale
	cons contact ctrl_dbus ctrl_tcp debug_cmd dtls_srtp echo evdev
	fakevideo g711 aac httpd httpreq ice in_band_dtmf l16 menu
	mixausrc mixminus mwi natpmp netroam opus opus_multistream pcp
	pipewire presence rtcpsummary sdl selfview serreg snapshot sndfile
	srtp stdio stun syslog turn uuid v4l2 vidbridge vidinfo vp8 vp9 vumeter
)

mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSTATIC=OFF \
	-DDEFAULT_AUDIO_DEVICE=pipewire,default \
	"-DMODULES=$(IFS=';'; echo "${modules[*]}")"
ninja
DESTDIR=$PKG ninja install

rm -f "$PKG/usr/share/applications/com.github.baresip.desktop"
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/baresip.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Telephone
GenericName=SIP Softphone
Comment=Voice calls over SIP
Exec=baresip
Icon=call-start
Terminal=true
Categories=Network;Telephony;
Keywords=sip;voip;call;phone;telephone;baresip;
EOF
chmod 644 "$PKG/usr/share/applications/baresip.desktop"
