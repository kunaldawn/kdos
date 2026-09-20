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

# EVERY OPTION IS PINNED, none left at `auto`. Autodetection makes the feature
# set a property of whatever happened to be installed when the build ran: drop
# a codec port and cmus still builds, silently without that format, and the
# first report is somebody's file not playing. Named y/n turns the same event
# into a build failure that says which library went.
#
# MP3 COMES FROM libmad AND NOT FROM FFMPEG. cmus 2.12.0 is the newest release
# and its ffmpeg input does not compile against ffmpeg 8 — the libraries and
# headers are found, then the feature test fails. Its mp3 support is libmad
# either way, so the format set loses only what ffmpeg alone decodes (.wma,
# .ape, .shn); mpv is on this image for anything stranger than that.
#
# ALSA IS THE OUTPUT AND THERE IS NO PULSE PATH. Audio on this system reaches
# PipeWire through ALSA's default device, so the ALSA plugin is the whole
# chain; a native PulseAudio output would need a libpulse this image does not
# carry.
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
	CONFIG_PULSE=n CONFIG_JACK=n CONFIG_AO=n CONFIG_ARTS=n CONFIG_ROAR=n \
	CONFIG_SNDIO=n CONFIG_SUN=n CONFIG_OSS=n CONFIG_COREAUDIO=n \
	CONFIG_AAUDIO=n CONFIG_WAVEOUT=n CONFIG_MPRIS=n CONFIG_SAMPLERATE=n \
	CONFIG_FFMPEG=n CONFIG_AAC=n CONFIG_MP4=n CONFIG_MPC=n CONFIG_MODPLUG=n CONFIG_MIKMOD=n \
	CONFIG_BASS=n CONFIG_VTX=n CONFIG_WAVPACK=n CONFIG_TREMOR=n \
	CONFIG_CDDB=n CONFIG_CDIO=n CONFIG_DISCID=n
make
make DESTDIR=$PKG install
