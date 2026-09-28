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

# GNOME's music library player, GStreamer underneath. Every optional feature
# is named: audio CDs and the FM-radio plugin need only the kernel headers;
# gudev finds MTP players and Android phones (libmtp); libnotify posts the
# now-playing notification; libsecret keeps service passwords; Brasero's
# library burns a playlist to audio CD; libgpod syncs iPods; lirc's client
# library takes infrared remotes. The Python plugins (replay gain, the Python
# console, lyrics, cover search and the online services) load through
# PyGObject; upstream builds no context pane. The Vala toolchain builds the
# Vala plugin bindings. gst-libav is the AAC decoder: without it an .m4a or
# .aac file in the library does not play.
#
# DAAP sharing needs libdmapsharing and Grilo its media-source framework,
# neither a port. The online plugins (Last.fm and ListenBrainz scrobbling,
# Magnatune, internet radio, podcasts, lyrics, cover search) have no build
# switch; offline each finds nothing, and the scrobblers send nothing until an
# account is signed in.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
	-Dbrasero=enabled \
	-Ddaap=disabled \
	-Dfm_radio=enabled \
	-Dgrilo=disabled \
	-Dgudev=enabled \
	-Dipod=enabled \
	-Dlibnotify=enabled \
	-Dlibsecret=enabled \
	-Dlirc=enabled \
	-Dmtp=enabled \
	-Dplugins_python=enabled \
	-Dplugins_vala=enabled \
	-Dsample-plugins=false \
	-Dhelp=true \
	-Dapidoc=false \
	-Dtests=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Bundled data is English only: the interface falls back to its source
# strings, and the manual keeps its C (English) pages.
rm -rf "$PKG/usr/share/locale"
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

# THE MENU ICON: upstream ships it as SVG only, which the panel never reads.
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s data/icons/hicolor/scalable/apps/org.gnome.Rhythmbox3.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/org.gnome.Rhythmbox3.png"
done

# UPSTREAM'S ENTRY IS REPLACED: the GtkApplication id org.gnome.Rhythmbox3 is
# the Wayland app_id. MimeType is left out: Strawberry is the music player
# that claims audio files, and a second claim would pick whichever entry
# sorted first.
cat > "$PKG/usr/share/applications/org.gnome.Rhythmbox3.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Rhythmbox
GenericName=Music Player
Comment=Play and organize your music collection
TryExec=rhythmbox
Exec=rhythmbox %U
Icon=org.gnome.Rhythmbox3
Terminal=false
StartupNotify=true
StartupWMClass=org.gnome.Rhythmbox3
Categories=GTK;AudioVideo;Audio;Player;
Keywords=audio;song;mp3;cd;podcast;mtp;playlist;radio;music;rhythmbox;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.gnome.Rhythmbox3.desktop"
