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

# THE CLOSURE IS IN THE BUNDLE, except where a module is already a port.
# `pypackages` names the part of Picard's runtime set nothing else here
# imports: mutagen (tag reading and writing), discid (the ctypes binding to
# libdiscid, for "Lookup CD"), Markdown (plugin descriptions) and PyJWT.
# PyQt6, PyYAML, charset-normalizer and tomlkit come from their python3-*
# ports. pygit2 is optional upstream and only installs plugins from git
# repositories on the internet; it is not a port, and Picard runs without it.
mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# BUILD ISOLATION IS OFF, so each sdist builds with the backend already
# installed. PICARD_DISABLE_AUTOUPDATE is upstream's build switch that removes
# the update check and hides its setting.
PICARD_DISABLE_AUTOUPDATE=1 \
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	mutagen discid markdown pyjwt .

# Bundled data is English only: Picard's strings are English in the source,
# and every compiled catalogue is removed.
_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
_plat=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"base": sys.argv[1]}))' "$PKG/usr")
rm -rf "$_site/picard/locale" "$_plat/picard/locale"

# UPSTREAM'S ENTRY IS REPLACED: Picard sets its desktop file name to
# org.musicbrainz.Picard, which is the Wayland app_id. MimeType is left out:
# Strawberry is the program that opens audio files; Picard is reached from
# the menu and opens folders from inside. The hicolor PNGs are upstream's.
cat > "$PKG/usr/share/applications/org.musicbrainz.Picard.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=MusicBrainz Picard
GenericName=Music Tagger
Comment=Tag and rename music files; lookups need the MusicBrainz server
TryExec=picard
Exec=picard %F
Icon=org.musicbrainz.Picard
Terminal=false
StartupNotify=true
StartupWMClass=org.musicbrainz.Picard
Categories=Qt;AudioVideo;Audio;AudioVideoEditing;
Keywords=tag;tagger;tags;metadata;musicbrainz;music;rename;picard;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.musicbrainz.Picard.desktop"
