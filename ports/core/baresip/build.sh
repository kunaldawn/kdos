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
# THE CODECS THIS IMAGE BUILDS ARE THE ONES ITS CONFIG LOADS, and the patch is
# what makes that true. Upstream's generated `config` comments every codec
# module out — a default written for a build that may carry none of them, where
# an uncommented `module opus.so` names a file that is not there and is a
# start-up error — so an unpatched baresip here reports `Populated 0 video
# codecs` and negotiates G.711 on a machine carrying all four.
#
# THE FOUR IT UNCOMMENTS ARE THE FOUR THIS RECIPE GUARANTEES: opus, libvpx and
# ffmpeg are `depends`, so opus.so, vp8.so, vp9.so and avcodec.so are installed
# beside the binary wherever this config is written. A fifth line for a module
# whose library is not in `depends` would be the start-up error above.
#
# There is no flag for it: the template is a run of re_fprintf calls.
patch -p1 -i "$PORT_SRC/default-codecs.patch"

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
