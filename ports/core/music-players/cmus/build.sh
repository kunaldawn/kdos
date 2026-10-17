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

patch -p1 -i "$PORT_SRC/ffmpeg8.patch"
./configure \
	prefix=/usr \
	mandir=/usr/share/man \
	docdir=/usr/share/doc/cmus \
	CONFIG_ALSA=y \
	CONFIG_FLAC=y \
	CONFIG_VORBIS=y \
	CONFIG_OPUS=y \
	CONFIG_MAD=y \
	CONFIG_WAV=y \
	CONFIG_CUE=y \
	CONFIG_AAC=y \
	CONFIG_WAVPACK=y \
	CONFIG_MIKMOD=y \
	CONFIG_MPRIS=y \
	CONFIG_FFMPEG=y \
	CONFIG_CDIO=y \
	CONFIG_PULSE=n CONFIG_JACK=n CONFIG_AO=n CONFIG_ARTS=n CONFIG_ROAR=n \
	CONFIG_SNDIO=n CONFIG_SUN=n CONFIG_OSS=n CONFIG_COREAUDIO=n \
	CONFIG_AAUDIO=n CONFIG_WAVEOUT=n CONFIG_SAMPLERATE=n \
	CONFIG_MP4=n CONFIG_MPC=n CONFIG_MODPLUG=n \
	CONFIG_BASS=n CONFIG_VTX=n CONFIG_TREMOR=n \
	CONFIG_CDDB=n CONFIG_DISCID=n
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/cmus.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Music (cmus)
GenericName=Music Player
Comment=Browse and play a music library
Exec=cmus
Icon=folder-music
Terminal=true
Categories=AudioVideo;Player;
Keywords=music;audio;player;library;playlist;cmus;
DESKTOP
chmod 644 "$PKG/usr/share/applications/cmus.desktop"
