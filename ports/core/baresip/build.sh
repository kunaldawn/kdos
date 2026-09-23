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
#
# THE INTERFACE IS A TERMINAL MENU AND THE PICTURE IS A WINDOW. baresip drives
# itself from the terminal it was started in; the far end's video goes in a
# window of its own, under the compositor. The gtk menu and every other toolkit
# front end stay off by rule.
#
# THE PICTURE HAS A PLACE TO GO, AND `sdl2-compat` IN `depends` IS THE WHOLE OF
# IT. `modules/sdl` builds itself whenever pkg-config answers for `sdl2` and
# returns silently when it does not, so naming the port is what turns a call
# that could send your camera and not show you theirs into one that can do
# both. `x11` stays unbuilt by rule — there is no X server here — and
# `fakevideo` and `vidbridge` remain what they are, a null sink and a loopback.
#
# Capture is the half that was never missing: the built avformat module
# registers a video source and the shipped ffmpeg carries video4linux2.
#
# THE MODULES THIS IMAGE BUILDS ARE THE ONES ITS CONFIG LOADS, and the patch is
# what makes that true. Upstream's generated `config` comments every codec and
# every display out — a default written for a build that may carry none of
# them, where an uncommented `module opus.so` names a file that is not there
# and is a start-up error — so an unpatched baresip here reports `Populated 0
# video codecs` and negotiates G.711 on a machine carrying all four, and has
# nowhere to draw on a machine carrying sdl.so.
#
# THE FIVE IT UNCOMMENTS ARE THE FIVE THIS RECIPE GUARANTEES: opus, libvpx,
# ffmpeg and sdl2-compat are `depends`, so opus.so, vp8.so, vp9.so, avcodec.so and
# sdl.so are installed beside the binary wherever this config is written. A
# sixth line for a module whose library is not in `depends` would be the
# start-up error above.
#
# There is no flag for it: the template is a run of re_fprintf calls.
patch -p1 -i "$PORT_SRC/default-modules.patch"

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
