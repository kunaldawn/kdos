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
# read is not a private conversation; python and C plugins, which is the whole
# extension mechanism; and the themes, which is what makes it a KDOS surface
# rather than the default grey.
#
# WHAT IS OFF, and each for a reason rather than by omission: notifications
# needs libnotify and the desktop's own notifyd is what this machine has;
# icons-and-clipboard and gdk-pixbuf are GTK, which the host does not have by
# rule; xscreensaver is X11; omemo needs libsignal or libomemo-c, neither of
# which is a port; and spellcheck wants enchant, declined for fcitx5 already.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dpgp=enabled -Dpython-plugins=enabled -Dc-plugins=enabled \
	-Dnotifications=disabled -Dicons-and-clipboard=disabled \
	-Dgdk-pixbuf=disabled -Dxscreensaver=disabled -Domemo=disabled \
	-Dspellcheck=disabled -Dotr=disabled -Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
