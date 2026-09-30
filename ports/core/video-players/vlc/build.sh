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

# VLC 3 WITH ITS QT 5 INTERFACE. The interface module embeds video by window
# id when Qt runs on xcb and by wl_surface when it runs on Wayland; with
# DISPLAY set VLC picks its xcb interface first, so it runs under Xwayland and
# draws through the xcb/GLX/EGL-X11 video outputs. --enable-wayland builds the
# Wayland outputs as well, for `QT_QPA_PLATFORM=wayland vlc`.
#
# musl-ioctl.patch: the V4L2 access declares ioctl() with glibc's `unsigned
# long` request; musl's takes `int`, and the function-pointer assignment does
# not compile. fribidi_allow_deprecated.patch: the subtitle renderer defines
# FRIBIDI_NO_DEPRECATED before fribidi.h, which then leaves out FriBidi's
# compatibility header; the patch, Alpine's, keeps that header declared.
patch -p1 -i "$PORT_SRC/musl-ioctl.patch"
patch -p1 -i "$PORT_SRC/fribidi_allow_deprecated.patch"

# LUA 5.4 THROUGH ITS VERSIONED NAMES. configure asks pkg-config for lua5.2,
# then lua5.1, then "lua"; the system lua is 5.5, whose API the Lua modules do
# not build against. LUA_CFLAGS/LUA_LIBS answer the check outright, and LUAC
# compiles the playlist, extension and interface scripts for the same VM.
export LUA_CFLAGS="$(pkg-config --cflags lua5.4)"
export LUA_LIBS="$(pkg-config --libs lua5.4)"
export LUAC=luac5.4

# EVERY MODULE WITH AN OUTSIDE LIBRARY IS NAMED, --enable or --disable, so a
# missing port fails configure instead of shipping a VLC that quietly cannot
# open a format. Off, and why:
#   no port        live555, dc1394, dv1394, linsys, dsm, smb2, shout, mpc,
#                  shine, dca, tremor, daala, openapv, bpg, x262, vpl, zvbi,
#                  aribsub, aribb25, kate, tiger, vdpau, caca, srt, librist,
#                  goom, projectm, vsxu, upnp, microdns, cddb, aom, fluidlite
#   API too new    libplacebo (VLC 3 takes < 6), postproc (gone from FFmpeg 8),
#                  opencv (VLC 3 asks for opencv.pc; OpenCV 4 installs
#                  opencv4.pc), sdl-image (VLC 3 takes SDL_image 1.2, not
#                  SDL2_image), sid (VLC 3 takes libsidplay2, not libsidplayfp)
#   duplicate      gst-decode (avcodec decodes the same), oss, sndio, aa
#   network-only   the add-ons manager (videolan.org's catalogue), the update
#                  check, Chromecast
#   licence        fdkaac: the FDK AAC encoder cannot be combined with GPL code
#   not here       JACK; KWallet (the Secret Service keystore is --enable-secret);
#                  the skins2 interface and libnotify popups (the Qt interface
#                  covers both); freerdp and vnc (VLC 3 targets old APIs of
#                  both)
# --enable-lirc is the one exception: a missing liblirc_client drops the
# module without an error, so lirc in depends is what keeps it.
# --disable-nls: bundled data is English only.
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
	--disable-srt \
	--disable-librist \
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

# THE PLUGIN CACHE IS NOT SHIPPED. The install hook writes plugins.dat with
# each plugin's size and modification time; packaging resets every mtime, so
# the cache would describe files that no longer match it and differ between
# two builds. Without it VLC reads the plugin directory at start-up.
rm -f "$PKG/usr/lib/vlc/plugins/plugins.dat"

# The browser-plugin and KDE 4 service-menu leftovers name programs that are
# not here.
rm -rf "$PKG/usr/lib/mozilla" "$PKG/usr/share/kde4"

# UPSTREAM'S ENTRY IS REPLACED. The Qt interface sets WM_CLASS "vlc".
# MimeType is left out: mpv's entry claims the video and audio types, and
# mimeapps.list is where the default player is chosen. The hicolor PNGs are
# upstream's, installed above.
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
