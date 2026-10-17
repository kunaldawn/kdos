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

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

PICARD_DISABLE_AUTOUPDATE=1 \
pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	mutagen discid pyjwt .

_site=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("purelib", vars={"base": sys.argv[1]}))' "$PKG/usr")
_plat=$(python3 -c 'import sys, sysconfig; print(sysconfig.get_path("platlib", vars={"base": sys.argv[1]}))' "$PKG/usr")
rm -rf "$_site/picard/locale" "$_plat/picard/locale"

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
