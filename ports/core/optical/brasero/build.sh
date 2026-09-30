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

# GNOME's disc burner, burning through its plugins: libburnia (libburn and
# libisofs) for data, audio and images, cdrdao for disc-at-once and copies,
# growisofs for DVD. The cdrtools and cdrkit plugins drive cdrecord/wodim and
# mkisofs/genisoimage, neither of which is a port, so they are off. The
# playlist pane reads playlists through totem-pl-parser; the preview pane
# plays through GStreamer. The Tracker search pane and the Nautilus extension
# have nothing here to talk to. Introspection and gtk-doc are off: nothing
# binds libbrasero from another language. --disable-caches leaves the icon,
# MIME and desktop caches to kpkg.
#
# gcc14.patch: two pointer-type mismatches that GCC 14 makes errors.
patch -p1 -i "$PORT_SRC/gcc14.patch"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static \
	--enable-libburnia \
	--enable-cdrdao \
	--enable-growisofs \
	--disable-cdrtools \
	--disable-cdrkit \
	--enable-playlist \
	--enable-preview \
	--enable-inotify \
	--disable-search \
	--disable-nautilus \
	--enable-introspection=no \
	--disable-gtk-doc \
	--disable-caches
make
make DESTDIR=$PKG install

# Bundled data is English only; the interface falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

# The Yelp manual ships in every language; bundled data is English only.
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED. Brasero takes window ids from GDK's X11
# backend for its dialogs and its video preview, so it is started under
# Xwayland; GTK 3 then sets WM_CLASS "Brasero". The name is qualified because
# K3b is the burner the menu lists first.
cat > "$PKG/usr/share/applications/brasero.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Disc Burner (Brasero)
GenericName=Disc Burner and Copier
Comment=Create and copy CDs and DVDs
TryExec=brasero
Exec=env GDK_BACKEND=x11 brasero %U
Icon=brasero
Terminal=false
StartupNotify=true
StartupWMClass=Brasero
Categories=GTK;AudioVideo;DiscBurning;
Keywords=disc;cdrom;dvd;burn;audio;video;copy;brasero;
Actions=Image;Disc;Audio;

[Desktop Action Image]
Name=Burn an Image File
Exec=env GDK_BACKEND=x11 brasero --image

[Desktop Action Disc]
Name=Copy a Disc
Exec=env GDK_BACKEND=x11 brasero --copy

[Desktop Action Audio]
Name=Create an Audio Project
Exec=env GDK_BACKEND=x11 brasero --audio
DESKTOP
chmod 644 "$PKG/usr/share/applications/brasero.desktop"
