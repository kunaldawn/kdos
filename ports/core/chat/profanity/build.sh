# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpgp=enabled -Dpython-plugins=enabled -Dc-plugins=enabled \
	-Dgdk-pixbuf=enabled \
	-Dnotifications=enabled -Dicons-and-clipboard=disabled \
	-Dxscreensaver=disabled \
	-Domemo=enabled -Domemo-backend=libomemo-c -Domemo-qrcode=enabled \
	-Dspellcheck=enabled -Dotr=disabled -Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/profanity.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Chat
GenericName=Instant Messaging
Comment=XMPP chat
Exec=profanity
Icon=mail-message
Terminal=true
Categories=Network;InstantMessaging;
Keywords=chat;xmpp;jabber;im;profanity;
EOF
chmod 644 "$PKG/usr/share/applications/profanity.desktop"
