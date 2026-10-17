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

meson setup build --prefix=/usr --libdir=lib \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dset-install-rpath=false \
	-Dplugin-http-files=enabled \
	-Dplugin-ice=enabled \
	-Dplugin-omemo=enabled \
	-Dplugin-openpgp=enabled \
	-Dplugin-rtp=enabled \
	-Dplugin-rtp-webrtc-audio-processing=enabled \
	-Dplugin-rtp-vp9=enabled \
	-Dplugin-rtp-h264=disabled \
	-Dplugin-rtp-msdk=disabled \
	-Dplugin-rtp-vaapi=disabled \
	-Dplugin-notification-sound=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

for s in 48 64 128 256; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x$s/apps"
	rsvg-convert -w $s -h $s main/data/icons/scalable/apps/im.dino.Dino.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x$s/apps/im.dino.Dino.png"
done

cat > "$PKG/usr/share/applications/im.dino.Dino.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Dino
GenericName=XMPP Client
Comment=Chat, share files and call over XMPP
Exec=dino %U
Icon=im.dino.Dino
Terminal=false
StartupWMClass=im.dino.Dino
MimeType=x-scheme-handler/xmpp;
Categories=GTK;Network;Chat;InstantMessaging;
Keywords=chat;talk;im;message;xmpp;jabber;omemo;call;
DESKTOP
chmod 644 "$PKG/usr/share/applications/im.dino.Dino.desktop"
