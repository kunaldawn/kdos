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

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

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
