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
# FFMPEG IS THE CATCH-ALL INPUT. It plays what no dedicated plugin here does:
# .m4a (AAC and ALAC in an MP4 container, which is what a phone or an iTunes
# library holds), .wma, .ape and .shn. cmus 2.12.0 calls avcodec_close(),
# which ffmpeg 8 removed, so ffmpeg8.patch makes that call only on the old
# libavcodec that still has it. cmus ranks its dedicated plugins above ffmpeg,
# so MP3 is still libmad and raw ADTS .aac still faad2.
#
# ALSA IS THE ONLY OUTPUT. Audio on this system reaches PipeWire through ALSA's
# default device, so the ALSA plugin is the whole chain. A pulse output would be
# a second route to the same server, and cmus ranks pulse above alsa, so
# building it would move every cmus stream onto pipewire-pulse.
#
# CONFIG_MP4 is off: it is a second .m4a reader through mp4v2, which is not a
# port, and ffmpeg already reads the container.
#
# AN AUDIO CD PLAYS through CONFIG_CDIO, which reads the drive with libcdio's
# cdda layer. CDDB lookups of the track names need libcddb, which is not a
# port, so a disc lists as numbered tracks.
#
# MPRIS IS sd-bus FROM basu. configure takes libsystemd, then libelogind, then
# basu for the same API; neither of the first two exists here, so basu is what
# puts cmus on the session bus for media keys and the panel.
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
