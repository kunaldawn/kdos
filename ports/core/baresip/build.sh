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

# THE COMMS LADDER SKIPPED VOICE, WHICH IS WHAT PEOPLE DEFAULT TO UNDER STRESS.
# This tree can already carry text between machines (syncthing, croc), serve
# documents (kiwix, caddy) and stand up a network with no router (hostapd +
# dnsmasq) — and had no way for two people on that network to TALK. With
# Asterisk in a box or a plain peer-to-peer `sip:user@host`, this closes it.
#
# The codecs are the ones already ported for ffmpeg — opus for speech and vp8
# for video — so this adds a protocol stack and no new media dependency.
# Everything graphical is off by rule: baresip's own interface is a terminal
# menu, which is the right shape for this desktop anyway.
#
# THE VIDEO HOLE IS THE DISPLAY SIDE ONLY. Capture works — the built avformat
# module registers a video source and the shipped ffmpeg has video4linux2 — and
# the codecs build. Of the outputs, only `fakevideo` (a null sink) and
# `vidbridge` (a loopback) were: `x11` wants the X headers this tree refuses by
# rule, and `sdl` wants an SDL port that does not exist.
#
# THE CODECS ARE BUILT AND THE DEFAULT CONFIG DOES NOT USE THEM. opus.so,
# vp8.so, vp9.so and avcodec.so are all among the modules this installs, and
# baresip's generated `config` leaves every one of them commented out — so a
# first run reports `Populated 0 video codecs` and speaks G.711 alone. That is
# upstream's default, not a build result, and it is lifted by uncommenting the
# module lines in ~/.baresip/config.
mkdir -p build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DSTATIC=OFF
ninja
DESTDIR=$PKG ninja install

# UPSTREAM'S ENTRY IS REPLACED, NOT ADDED TO. baresip's own CMakeLists globs
# share/*.desktop and installs it, and what it installs is wrong three times
# over on this image: `Terminal=false` for a program whose entire interface is a
# terminal menu, `Exec=env GDK_BACKEND=x11 baresip` on a machine with no X
# server and no gtk module built, and `Categories=GNOME;GTK;…`, neither of which
# means anything here. Leaving it and adding ours would put two Baresip rows in
# the menu, so the file it installed goes first.
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
