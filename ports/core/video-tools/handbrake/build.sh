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

# HandBrake links its own patched FFmpeg 8, x265 (8, 10 and 12 bit), SVT-AV1,
# dav1d, zimg, libdvdread, libdvdnav and libbluray statically into libhb: its
# FFmpeg patches (mov/mp4 metadata, DVD and PGS subtitles) are not in the
# system FFmpeg, and libhb is written against that one release. Each tarball
# is a source of this recipe, placed in download/ under the name its
# contrib/*/module.defs fetches, and --disable-df-fetch makes a missing one a
# build error instead of a download. The sha256 in module.defs is checked
# again as each is extracted.
#
# NVENC, QSV and VCE need vendor SDKs, libdovi needs a cargo fetch, and
# FDK AAC would make the build nonfree: FFmpeg's own AAC encoder is used.
install -d download
for f in ffmpeg-8.0.2.tar.bz2 x265-snapshot-20260216-13309.tar.gz \
	SVT-AV1-v4.1.0.tar.gz dav1d-1.5.3.tar.bz2 zimg-snapshot-20250624.tar.gz \
	libdvdread-7.0.1.tar.bz2 libdvdnav-7.0.0.tar.bz2 libbluray-1.4.0.tar.xz; do
	cp "$PORT_SRC/$f" download/
done

./configure --prefix=/usr \
	--disable-df-fetch \
	--enable-gtk \
	--enable-x265 \
	--enable-numa \
	--disable-fdk-aac \
	--disable-nvenc \
	--disable-nvdec \
	--disable-qsv \
	--disable-vce \
	--disable-libdovi
make -C build
make -C build -j1 DESTDIR=$PKG install

# English only: the GUI falls back to its source strings.
rm -rf "$PKG/usr/share/locale"

# The panel reads no SVG; upstream installs only the scalable icon and keeps
# the raster sizes in its tree.
for s in 32x32 128x128 256x256; do
	install -Dm644 "gtk/icons/$s/apps/fr.handbrake.ghb.png" \
		"$PKG/usr/share/icons/hicolor/$s/apps/fr.handbrake.ghb.png"
done

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass, which it lacks: the app_id
# is fr.handbrake.ghb. It claims no MimeType: every one it lists belongs to
# the players, and a transcoder is opened on purpose, not on a double click.
cat > "$PKG/usr/share/applications/fr.handbrake.ghb.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=HandBrake
GenericName=Video Transcoder
Comment=Convert video files and discs to MP4, MKV and WebM
Exec=ghb %f
Icon=fr.handbrake.ghb
Terminal=false
StartupNotify=true
StartupWMClass=fr.handbrake.ghb
Categories=AudioVideo;Video;
Keywords=transcode;convert;encode;video;dvd;bluray;handbrake;
DESKTOP
chmod 644 "$PKG/usr/share/applications/fr.handbrake.ghb.desktop"
