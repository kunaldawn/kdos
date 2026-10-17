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

patch -p1 -i "$PORT_SRC/musl-ioctl.patch"
patch -p1 -i "$PORT_SRC/fribidi_allow_deprecated.patch"

export LUA_CFLAGS="$(pkg-config --cflags lua5.4)"
export LUA_LIBS="$(pkg-config --libs lua5.4)"
export LUAC=luac5.4

export BUILDCC="gcc -std=gnu11"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-rpath \
	--disable-nls \
	--disable-debug \
	--disable-update-check \
	--disable-addonmanagermodules \
	--enable-vlc \
	--enable-dbus \
	--enable-sout \
	--enable-lua \
	--enable-vlm \
	--enable-archive \
	--enable-dvdread \
	--enable-dvdnav \
	--enable-bluray \
	--enable-smbclient \
	--enable-sftp \
	--enable-nfs \
	--enable-v4l2 \
	--enable-vcd \
	--enable-screen \
	--enable-dvbpsi \
	--enable-gme \
	--enable-ogg \
	--enable-matroska \
	--enable-mod \
	--enable-wma-fixed \
	--enable-mad \
	--enable-mpg123 \
	--enable-merge-ffmpeg \
	--enable-avcodec \
	--enable-avformat \
	--enable-swscale \
	--enable-libva \
	--enable-faad \
	--enable-dav1d \
	--enable-vpx \
	--enable-a52 \
	--enable-libmpeg2 \
	--enable-twolame \
	--enable-flac \
	--enable-vorbis \
	--enable-speex \
	--enable-opus \
	--enable-theora \
	--enable-oggspots \
	--enable-png \
	--enable-jpeg \
	--enable-x264 \
	--enable-x265 \
	--enable-fluidsynth \
	--enable-telx \
	--enable-css \
	--enable-gles2 \
	--with-x \
	--enable-xcb \
	--enable-xvideo \
	--enable-wayland \
	--enable-freetype \
	--enable-fribidi \
	--enable-harfbuzz \
	--enable-fontconfig \
	--enable-libass \
	--enable-svg \
	--enable-svgdec \
	--enable-pulse \
	--enable-alsa \
	--enable-samplerate \
	--enable-soxr \
	--enable-spatialaudio \
	--enable-chromaprint \
	--enable-qt \
	--enable-ncurses \
	--enable-avahi \
	--enable-udev \
	--enable-lirc \
	--enable-mtp \
	--enable-libxml2 \
	--enable-libgcrypt \
	--enable-gnutls \
	--enable-taglib \
	--enable-secret \
	--disable-live555 \
	--disable-dc1394 \
	--disable-dv1394 \
	--disable-linsys \
	--disable-opencv \
	--disable-dsm \
	--disable-smb2 \
	--disable-decklink \
	--disable-libcddb \
	--disable-vnc \
	--disable-freerdp \
	--disable-realrtsp \
	--disable-asdcp \
	--disable-shout \
	--disable-sid \
	--disable-mpc \
	--disable-shine \
	--disable-omxil \
	--disable-crystalhd \
	--disable-gst-decode \
	--disable-postproc \
	--disable-aom \
	--disable-fdkaac \
	--disable-dca \
	--disable-tremor \
	--disable-daala \
	--disable-openapv \
	--disable-bpg \
	--disable-x262 \
	--disable-x26410b \
	--disable-vpl \
	--disable-fluidlite \
	--disable-zvbi \
	--disable-aribsub \
	--disable-aribb25 \
	--disable-kate \
	--disable-tiger \
	--disable-vdpau \
	--disable-sdl-image \
	--disable-aa \
	--disable-caca \
	--disable-mmal \
	--disable-evas \
	--disable-oss \
	--disable-sndio \
	--disable-jack \
	--disable-chromecast \
	--disable-skins2 \
	--enable-srt \
	--enable-librist \
	--disable-goom \
	--disable-projectm \
	--disable-vsxu \
	--disable-upnp \
	--disable-microdns \
	--disable-kwallet \
	--disable-notify \
	--disable-libplacebo
make
make -j1 DESTDIR=$PKG install

rm -f "$PKG/usr/lib/vlc/plugins/plugins.dat"

rm -rf "$PKG/usr/lib/mozilla" "$PKG/usr/share/kde4"

cat > "$PKG/usr/share/applications/vlc.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=VLC media player
GenericName=Media Player
Comment=Read, capture and stream multimedia, discs and devices
TryExec=vlc
Exec=vlc --started-from-file %U
Icon=vlc
Terminal=false
StartupNotify=true
StartupWMClass=vlc
X-KDE-Protocols=ftp,http,https,mms,rtmp,rtsp,sftp,smb
Categories=Qt;AudioVideo;Player;Recorder;Video;
Keywords=player;capture;dvd;audio;video;playlist;stream;vlc;
Actions=play-dvd;play-cd;

[Desktop Action play-dvd]
Name=Play a DVD
Exec=vlc dvd://

[Desktop Action play-cd]
Name=Play an audio CD
Exec=vlc cdda://
DESKTOP
chmod 644 "$PKG/usr/share/applications/vlc.desktop"
