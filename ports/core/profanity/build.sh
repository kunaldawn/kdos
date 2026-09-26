# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# THE CLIENT HALF OF THE LAN COMMS PAIR, with prosody as the server. Every
# feature here defaults to `disabled`, so what is not named is not built —
# which is why the wanted ones are named rather than left to be discovered.
#
# WHAT IS ON: pgp, because a message store on a shared stick that anybody can
# read is not a private conversation; omemo, through libomemo-c, which is the
# end-to-end encryption dino and every current XMPP client speak, with the QR
# code that verifies a fingerprint from a phone; python and C plugins, which
# is the whole extension mechanism; gdk-pixbuf, which scales an avatar before
# it is uploaded; notifications, through libnotify to kdos-notifyd, so an
# incoming message raises a toast as `/notify` configures; spellcheck, through
# enchant and its Aspell provider; and the themes, which is what makes it a
# KDOS surface rather than the default grey.
#
# WHAT IS OFF, and each for a reason rather than by omission:
# icons-and-clipboard is GTK, which the host does not have by rule;
# xscreensaver is X11; and otr needs libotr, which is not a port.
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
