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

patch -p1 -i "$PORT_SRC/system-libdovi.patch"
install -d download
for f in ffmpeg-8.0.2.tar.bz2 x265-snapshot-20260216-13309.tar.gz \
	SVT-AV1-v4.1.0.tar.gz dav1d-1.5.3.tar.bz2 zimg-snapshot-20250624.tar.gz \
	libdvdread-7.0.1.tar.bz2 libdvdnav-7.0.0.tar.bz2 libbluray-1.4.0.tar.xz; do
	cp "$PORT_SRC/$f" download/
done

export CFLAGS="${CFLAGS/-std=gnu11/}"
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
	--enable-libdovi
make -C build
make -C build -j1 DESTDIR=$PKG install

for s in 32x32 128x128 256x256; do
	install -Dm644 "gtk/icons/$s/apps/fr.handbrake.ghb.png" \
		"$PKG/usr/share/icons/hicolor/$s/apps/fr.handbrake.ghb.png"
done

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
